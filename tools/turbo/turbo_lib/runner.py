"""Supervised command execution with logs, timeouts and process-group cleanup."""

from __future__ import annotations

import os
import re
import signal
import subprocess
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path

from . import config


class RunnerError(RuntimeError):
    pass


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _write_header(handle, command: list[str], cwd: Path, timeout: float, extra_env: dict | None) -> None:
    handle.write(f"# TURBO run started: {_utc_now()}\n")
    handle.write(f"# cwd: {cwd}\n")
    handle.write(f"# timeout: {timeout:.1f}s\n")
    handle.write(f"# command: {subprocess.list2cmdline(command)}\n")
    if extra_env:
        handle.write(f"# environment: {extra_env}\n")
    handle.write("# " + "-" * 72 + "\n")
    handle.flush()


def _terminate_group(pid: int, grace: float = 3.0) -> None:
    """Terminate the child's whole session, then force it if needed."""
    try:
        pgid = os.getpgid(pid)
    except OSError:
        pgid = None
    if pgid is None:
        return
    try:
        os.killpg(pgid, signal.SIGTERM)
    except OSError:
        return
    deadline = time.monotonic() + grace
    while time.monotonic() < deadline:
        try:
            os.killpg(pgid, 0)
        except OSError:
            return
        time.sleep(0.05)
    try:
        os.killpg(pgid, signal.SIGKILL)
    except OSError:
        pass


def _cleanup_orphans(pgid: int) -> None:
    """A normal parent may exit while a test child keeps running."""
    try:
        os.killpg(pgid, signal.SIGTERM)
    except OSError:
        return
    time.sleep(0.15)
    try:
        os.killpg(pgid, signal.SIGKILL)
    except OSError:
        pass


def excerpt(log_path: Path, max_chars: int = config.MAX_EXCERPT_CHARS) -> str:
    try:
        text = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    if len(text) <= max_chars:
        return text
    lines = text.splitlines()
    important = []
    patterns = re.compile(
        r"(?i)(error|erro|failed|falhou|failure|exception|assert|timeout|travou|"
        r"not found|não encontrado|undefined reference|segmentation|core dumped)"
    )
    for line in lines:
        if patterns.search(line):
            important.append(line)
    if important:
        picked = "\n".join(important[-8:])
        if len(picked) > max_chars:
            picked = picked[-max_chars:]
        return (
            f"[trecho: {len(important)} linha(s) de erro; log completo no caminho]\n"
            + picked
        )
    head = text[: max_chars // 3]
    tail = text[-max_chars * 2 // 3 :]
    return (
        f"[log truncado: {len(text)} caracteres; início e fim]\n"
        + head
        + "\n[...]\n"
        + tail
    )


def run_command(
    command: list[str],
    *,
    cwd: Path,
    log_path: Path,
    timeout: float,
    name: str,
    extra_env: dict[str, str] | None = None,
    stdin_data: str | None = None,
) -> dict:
    if not command:
        raise RunnerError("comando vazio")
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started_at = _utc_now()
    start = time.monotonic()
    environment = os.environ.copy()
    if extra_env:
        environment.update({str(key): str(value) for key, value in extra_env.items()})
    timed_out = False
    exit_code = None
    with log_path.open("w", encoding="utf-8", errors="replace") as handle:
        _write_header(handle, command, cwd, timeout, extra_env)
        try:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                stdin=subprocess.PIPE if stdin_data is not None else subprocess.DEVNULL,
                stdout=handle,
                stderr=subprocess.STDOUT,
                text=True,
                start_new_session=True,
                env=environment,
            )
        except OSError as exc:
            handle.write(f"\nTURBO falhou ao iniciar o processo: {exc}\n")
            return {
                "name": name,
                "command": command,
                "cwd": str(cwd),
                "started_at": started_at,
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - start, 3),
                "exit_code": None,
                "status": "ERROR",
                "timed_out": False,
                "log": str(log_path),
                "excerpt": f"falha ao iniciar processo: {exc}",
            }
        pgid = os.getpgid(process.pid)
        try:
            if stdin_data is not None:
                process.communicate(stdin_data, timeout=timeout)
            else:
                process.wait(timeout=timeout)
            exit_code = process.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
            handle.write(f"\nTURBO TIMEOUT após {timeout:.1f}s; encerrando grupo {pgid}\n")
            handle.flush()
            _terminate_group(process.pid)
            try:
                exit_code = process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    process.kill()
                except OSError:
                    pass
                exit_code = None
        except KeyboardInterrupt:
            _terminate_group(process.pid)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            raise
        finally:
            if process.poll() is not None:
                _cleanup_orphans(pgid)
        handle.write(f"\n# exit code: {exit_code}; timed_out={timed_out}\n")
        handle.write(f"# finished: {_utc_now()}\n")
    duration = time.monotonic() - start
    if timed_out:
        status = "TIMEOUT"
    elif exit_code == 0:
        status = "PASSED"
    else:
        status = "FAILED"
    return {
        "name": name,
        "command": command,
        "cwd": str(cwd),
        "started_at": started_at,
        "finished_at": _utc_now(),
        "duration_seconds": round(duration, 3),
        "exit_code": exit_code,
        "status": status,
        "timed_out": timed_out,
        "log": str(log_path),
        "log_bytes": log_path.stat().st_size if log_path.exists() else 0,
        "excerpt": excerpt(log_path),
    }


def build(
    root: Path,
    build_dir: Path,
    targets: list[str],
    *,
    jobs: int,
    log_dir: Path,
    timeout: float = config.BUILD_TIMEOUT,
    configure: bool = False,
) -> dict:
    results = []
    if configure or not (build_dir / "CMakeCache.txt").is_file():
        configure_result = run_command(
            ["cmake", "-S", str(root), "-B", str(build_dir)],
            cwd=root,
            log_path=log_dir / "configure.log",
            timeout=min(timeout, 600),
            name="cmake-configure",
        )
        results.append(configure_result)
        if configure_result["status"] != "PASSED":
            return {
                "status": "FAILED",
                "reason": "configuração CMake falhou",
                "commands": results,
                "targets": targets,
            }
    if not targets:
        return {
            "status": "PASSED" if all(item["status"] == "PASSED" for item in results) else "FAILED",
            "reason": "nenhum target de compilação necessário; configuração verificada"
            if results
            else "nenhum target necessário",
            "commands": results,
            "targets": [],
            "duration_seconds": round(sum(item["duration_seconds"] for item in results), 3),
            "log": str(log_dir / "configure.log") if results else None,
            "excerpt": "",
        }
    command = ["cmake", "--build", str(build_dir), "--parallel", str(max(1, jobs))]
    if targets:
        command.extend(["--target", *targets])
    build_result = run_command(
        command,
        cwd=root,
        log_path=log_dir / "build.log",
        timeout=timeout,
        name="build",
    )
    results.append(build_result)
    return {
        "status": "PASSED" if all(item["status"] == "PASSED" for item in results) else "FAILED",
        "reason": None if build_result["status"] == "PASSED" else "compilação falhou",
        "commands": results,
        "targets": targets,
        "duration_seconds": round(sum(item["duration_seconds"] for item in results), 3),
        "log": str(log_dir / "build.log"),
        "excerpt": build_result.get("excerpt", ""),
    }


def _parse_junit(path: Path) -> dict[str, dict]:
    if not path.is_file():
        return {}
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError):
        return {}
    result = {}
    for case in root.iter("testcase"):
        name = case.get("name") or ""
        status = "PASSED"
        detail = ""
        failure = case.find("failure")
        error = case.find("error")
        skipped = case.find("skipped")
        if failure is not None:
            status = "FAILED"
            detail = (failure.text or failure.get("message") or "").strip()
        elif error is not None:
            status = "ERROR"
            detail = (error.text or error.get("message") or "").strip()
        elif skipped is not None:
            status = "NOT_RUN"
            detail = (skipped.text or skipped.get("message") or "").strip()
        result[name] = {
            "name": name,
            "status": status,
            "duration_seconds": float(case.get("time") or 0),
            "detail": detail,
        }
    return result


def _parse_ctest_text(text: str) -> dict[str, dict]:
    """Fallback for old ctests or a run killed before JUnit was written."""
    result = {}
    pattern = re.compile(
        r"Test\s+#\d+:\s+(\S+).*?\n(?:.*?\n){0,8}?\s*(Passed|Failed|Timeout|Not Run|\*\*\*Failed|SKIPPED)",
        re.IGNORECASE,
    )
    for name, status in pattern.findall(text):
        status = status.lower()
        if "pass" in status:
            normalized = "PASSED"
        elif "timeout" in status:
            normalized = "TIMEOUT"
        elif "not run" in status or "skip" in status:
            normalized = "NOT_RUN"
        else:
            normalized = "FAILED"
        result[name] = {"name": name, "status": normalized, "duration_seconds": 0, "detail": ""}
    return result


def _safe_log_component(name: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_.-]+", "_", name).strip("._")
    return value[:120] or "test"


def run_supervised_tests(
    root: Path,
    build_dir: Path,
    tests: list[dict],
    *,
    log_dir: Path,
    budget_seconds: float,
    graphics_timeout: float = config.GRAPHICS_TEST_TIMEOUT,
    headless_timeout: float = config.HEADLESS_TEST_TIMEOUT,
) -> dict:
    """Run each selected test in its own supervised CTest process.

    Individual invocations make the per-test cap real even when a test has a
    larger CTest TIMEOUT property.  The global budget is checked before every
    launch; tests that do not fit are reported as pending, never as passed.
    """
    selected = [str(test["name"]) for test in tests]
    if not selected:
        return {
            "status": "NOT_RUN",
            "reason": "nenhum teste selecionado",
            "tests": {},
            "selected": [],
            "counts": {
                "selected": 0,
                "executed": 0,
                "passed": 0,
                "failed": 0,
                "timeout": 0,
                "not_run": 0,
                "budget_skipped": 0,
            },
            "pending": [],
            "interrupted": [],
            "duration_seconds": 0,
            "budget_seconds": budget_seconds,
            "budget_used_seconds": 0,
            "budget_exhausted": False,
            "timed_out": False,
            "exit_code": 2,
        }

    log_dir.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    deadline = started + max(0.0, float(budget_seconds))
    # Required regressions first; within that group, headless tests are cheaper
    # and finish before the software-rendered display tests.  Duration is only
    # a tie-breaker, never a reason to hide a required test.
    ordered = sorted(
        tests,
        key=lambda test: (
            0 if test.get("priority") == "required" else 1,
            0 if not test.get("graphics") else 1,
            float(test.get("duration_seconds") or 0),
            str(test.get("name")),
        ),
    )
    results: dict[str, dict] = {}
    pending: list[dict] = []
    interrupted: list[dict] = []
    budget_interruptions: list[dict] = []
    for test in ordered:
        name = str(test["name"])
        graphics = bool(test.get("graphics"))
        cap = float(graphics_timeout if graphics else headless_timeout)
        declared = float(test.get("timeout_seconds") or cap)
        cap = min(cap, declared)
        remaining = deadline - time.monotonic()
        if remaining <= 0.5:
            pending.append(
                {
                    "name": name,
                    "priority": test.get("priority", "recommended"),
                    "graphics": graphics,
                    "reason": "orçamento de testes esgotado antes do início",
                }
            )
            continue
        timeout = min(cap, remaining)
        budget_limited = remaining <= cap + 0.05
        run_dir = log_dir / "tests" / _safe_log_component(name)
        chunk = run_ctest(
            root,
            build_dir,
            [name],
            jobs=1,
            log_dir=run_dir,
            timeout=timeout,
        )
        item = (chunk.get("tests") or {}).get(name)
        if item is None:
            item = {
                "name": name,
                "status": "NOT_RUN",
                "duration_seconds": 0,
                "detail": "teste ausente no JUnit/CTest",
            }
        item = dict(item)
        item["graphics"] = graphics
        item["priority"] = test.get("priority", "recommended")
        item["per_test_timeout_seconds"] = round(timeout, 3)
        item["log"] = chunk.get("log")
        item["junit"] = chunk.get("junit")
        item["duration_seconds"] = round(float(chunk.get("duration_seconds") or 0), 3)
        if chunk.get("timed_out"):
            reason = (
                "orçamento global de testes esgotado durante a execução"
                if budget_limited
                else f"timeout individual de {timeout:.1f}s"
            )
            item["status"] = "TIMEOUT"
            item["detail"] = reason
            interrupted.append(
                {
                    "name": name,
                    "priority": item["priority"],
                    "graphics": graphics,
                    "reason": reason,
                }
            )
            if budget_limited:
                item["budget_interrupted"] = True
                budget_interruptions.append(interrupted[-1])
        elif chunk.get("status") == "FAILED" and item.get("status") == "NOT_RUN":
            item["status"] = "ERROR"
            item["detail"] = "CTest não executou o teste selecionado"
        if item.get("status") == "NOT_RUN" and not item.get("detail"):
            item["detail"] = "não executado pelo CTest"
        results[name] = item
        if budget_limited and chunk.get("timed_out"):
            # The failing test consumed the remaining global budget; report the
            # rest as pending instead of launching them with a zero timeout.
            current = ordered.index(test)
            for rest in ordered[current + 1 :]:
                rest_name = str(rest["name"])
                if rest_name not in results:
                    pending.append(
                        {
                            "name": rest_name,
                            "priority": rest.get("priority", "recommended"),
                            "graphics": bool(rest.get("graphics")),
                            "reason": "orçamento de testes esgotado",
                        }
                    )
            break

    counts = {
        "selected": len(selected),
        "executed": sum(1 for item in results.values() if item.get("status") != "NOT_RUN"),
        "passed": sum(1 for item in results.values() if item.get("status") == "PASSED"),
        "failed": sum(1 for item in results.values() if item.get("status") in {"FAILED", "ERROR"}),
        "timeout": sum(1 for item in results.values() if item.get("status") == "TIMEOUT"),
        "not_run": sum(1 for item in results.values() if item.get("status") == "NOT_RUN")
        + sum(1 for _ in pending),
        "budget_skipped": len(pending),
        "budget_interrupted": len(budget_interruptions),
    }
    if counts["failed"]:
        status = "FAILED"
    elif budget_interruptions:
        # The cap that stopped the test was the global validation budget, not
        # an individual test timeout.  Keep that distinction visible.
        status = "BUDGET_EXHAUSTED"
    elif counts["timeout"]:
        status = "TIMEOUT"
    elif pending:
        status = "BUDGET_EXHAUSTED"
    elif counts["not_run"]:
        status = "NOT_RUN"
    else:
        status = "PASSED"
    pending_required = [item["name"] for item in pending if item.get("priority") == "required"]
    timeout_required = [
        name
        for name, item in results.items()
        if item.get("status") == "TIMEOUT" and item.get("priority") == "required"
    ]
    index_path = log_dir / "tests.log"
    with index_path.open("w", encoding="utf-8") as handle:
        handle.write("# TURBO supervised tests\n")
        handle.write(f"# budget: {budget_seconds:.1f}s\n")
        handle.write(f"# per-test graphics: {graphics_timeout:.1f}s; headless: {headless_timeout:.1f}s\n")
        handle.write("# " + "-" * 72 + "\n")
        for name in selected:
            item = results.get(name)
            if item:
                handle.write(
                    f"[{item.get('status')}] {name} {float(item.get('duration_seconds') or 0):.3f}s "
                    f"log={item.get('log')}\n"
                )
            else:
                handle.write(f"[PENDING] {name}\n")
        if pending:
            handle.write("\n# Pendentes por orçamento:\n")
            for item in pending:
                handle.write(f"# {item['name']}: {item['reason']}\n")
        if interrupted:
            handle.write("\n# Interrompidos:\n")
            for item in interrupted:
                handle.write(f"# {item['name']}: {item['reason']}\n")
        handle.write(f"\n# used: {time.monotonic() - started:.3f}s; status: {status}\n")
    duration = time.monotonic() - started
    exit_code = 124 if status == "TIMEOUT" else 1 if status == "FAILED" else 2 if status in {"BUDGET_EXHAUSTED", "NOT_RUN"} else 0
    return {
        "status": status,
        "reason": None
        if status == "PASSED"
        else "orçamento de testes esgotado"
        if status == "BUDGET_EXHAUSTED"
        else "há testes interrompidos ou não executados",
        "tests": results,
        "selected": selected,
        "counts": counts,
        "pending": pending,
        "interrupted": interrupted,
        "budget_interruptions": budget_interruptions,
        "pending_required": pending_required,
        "timeout_required": timeout_required,
        "duration_seconds": round(duration, 3),
        "budget_seconds": round(float(budget_seconds), 3),
        "budget_used_seconds": round(min(duration, float(budget_seconds)), 3),
        "budget_exhausted": bool(pending or budget_interruptions),
        "timed_out": counts["timeout"] > 0,
        "log": str(index_path),
        "junit": None,
        "exit_code": exit_code,
        "excerpt": "",
    }


def run_ctest(
    root: Path,
    build_dir: Path,
    selected: list[str],
    *,
    jobs: int,
    log_dir: Path,
    timeout: float,
    extra_env: dict[str, str] | None = None,
) -> dict:
    if not selected:
        return {
            "status": "NOT_RUN",
            "reason": "nenhum teste selecionado",
            "tests": {},
            "selected": [],
            "duration_seconds": 0,
        }
    log_dir.mkdir(parents=True, exist_ok=True)
    escaped = "|".join(re.escape(name) for name in sorted(set(selected)))
    regex = f"^({escaped})$"
    junit = log_dir / "test-results.xml"
    log_path = log_dir / "tests.log"
    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "-R",
        regex,
        "-j",
        str(max(1, jobs)),
        "--output-on-failure",
        "--output-junit",
        str(junit),
    ]
    result = run_command(
        command,
        cwd=root,
        log_path=log_path,
        timeout=timeout,
        name="ctest",
        extra_env=extra_env,
    )
    per_test = _parse_junit(junit)
    if not per_test:
        try:
            per_test = _parse_ctest_text(log_path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            per_test = {}
    selected_set = set(selected)
    missing = sorted(selected_set - set(per_test))
    for name in missing:
        per_test[name] = {
            "name": name,
            "status": "TIMEOUT" if result["timed_out"] else "NOT_RUN",
            "duration_seconds": 0,
            "detail": "não apareceu no JUnit/CTest; execução interrompida ou não iniciada",
        }
    for item in per_test.values():
        item.setdefault("log", str(log_path))
    passed = sum(1 for item in per_test.values() if item["status"] == "PASSED")
    failed = sum(1 for item in per_test.values() if item["status"] in {"FAILED", "ERROR"})
    timed_out_tests = sum(1 for item in per_test.values() if item["status"] == "TIMEOUT")
    not_run = sum(1 for item in per_test.values() if item["status"] == "NOT_RUN")
    overall = result["status"]
    if overall == "PASSED" and (failed or timed_out_tests or missing):
        overall = "FAILED"
    if timed_out_tests or result["timed_out"]:
        overall = "TIMEOUT"
    return {
        "status": overall,
        "reason": None if overall == "PASSED" else result.get("excerpt") or "testes falharam",
        "command": result,
        "tests": per_test,
        "selected": selected,
        "counts": {
            "selected": len(selected),
            "executed": len(per_test),
            "passed": passed,
            "failed": failed,
            "timeout": timed_out_tests,
            "not_run": not_run,
        },
        "duration_seconds": result["duration_seconds"],
        "log": str(log_path),
        "junit": str(junit),
        "exit_code": result["exit_code"],
        "timed_out": result["timed_out"],
        "excerpt": result.get("excerpt", ""),
        "missing": missing,
    }
