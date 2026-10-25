"""Explainable test/build selection based on CMake, CTest and ATLAS data."""

from __future__ import annotations

import os
from collections import defaultdict, deque
from pathlib import Path

from . import config

CRITICAL_MODULES = {"core", "serialization", "scene", "physics", "rendering", "ui", "app"}
RISK_WORDS = ("serialization", "public", "header", "render", "ui", "physics")
# Direct regression evidence runs before broad module/structural guesses when a
# test budget is tight.  The labels are a planning contract, not a semantic
# proof; reasons remain visible for audit.
REQUIRED_REASON_MARKERS = (
    "arquivo do teste ou fonte direta:",
    "target de teste direto:",
    "harness de teste alterado:",
    "consumidor do target:",
    "recurso associado:",
    "ferramenta ATLAS alterada:",
    "ferramenta TURBO alterada:",
    "teste do ATLAS alterado:",
    "teste do TURBO alterado:",
    "modo FULL:",
)


def _priority(reasons: list[str]) -> str:
    if any(any(reason.startswith(marker) for marker in REQUIRED_REASON_MARKERS) for reason in reasons):
        return "required"
    return "recommended"


def _normalize(path: str) -> str:
    return path.replace(os.sep, "/")


def _target_closure(target: str, deps: dict[str, list[str]]) -> set[str]:
    seen = set()
    queue = deque([target])
    while queue:
        current = queue.popleft()
        if current in seen:
            continue
        seen.add(current)
        queue.extend(deps.get(current, []))
    return seen


def _reverse_includes(inventory: dict) -> dict[str, set[str]]:
    reverse: dict[str, set[str]] = defaultdict(set)
    known_files = {row.get("path") for row in inventory.get("atlas_files", [])}
    for row in inventory.get("atlas_relations", []):
        source = _normalize(row.get("source_path") or "")
        target = _normalize(row.get("target") or "")
        if source and target and (target in known_files or target.endswith((".h", ".hpp", ".hh"))):
            reverse[target].add(source)
    return reverse


def _targets_for_paths(inventory: dict, paths: set[str]) -> set[str]:
    result = set()
    for target, sources in inventory.get("source_by_target", {}).items():
        if paths.intersection(_normalize(path) for path in sources):
            result.add(target)
    return result


def _test_target_closure(test: dict, inventory: dict) -> set[str]:
    target = test.get("target")
    if not target:
        return set()
    return _target_closure(target, inventory.get("deps_by_target", {}))


def _module_for_change(path: str, module_by_path: dict[str, str]) -> str:
    return module_by_path.get(path) or config.module_for_path(path)


def _asset_tests(path: str) -> list[str]:
    for prefix, tests in config.ASSET_TEST_RULES:
        if path.startswith(prefix):
            return list(tests)
    return []


def _is_source_change(path: str) -> bool:
    return path.startswith(("src/", "tests/")) or path.endswith(
        (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hh", ".hxx", ".lua")
    )


def _is_cmake_change(path: str) -> bool:
    return path == "CMakeLists.txt" or path.endswith(".cmake") or path == "CMakePresets.json"


def _mode_filter(test: dict, mode: str) -> tuple[bool, str | None]:
    if mode == "full":
        return True, None
    if mode == "fast":
        if test["graphics"]:
            return False, "teste gráfico excluído do FAST"
        if test["slow"]:
            return False, "teste lento excluído do FAST"
        return True, None
    if mode == "impact":
        # IMPACT accepts graphics that were selected by module/target evidence.
        # It does not add unrelated graphics merely to fill a quota.
        return True, None
    return False, f"modo inválido: {mode}"


def _fallback_baseline(inventory: dict, mode: str) -> list[str]:
    choices = []
    for test in inventory.get("tests", []):
        if test["graphics"] or test["slow"] or test["video"]:
            continue
        if test["labels"] and not set(test["labels"]).intersection({"unit", "smoke"}):
            continue
        choices.append(test["name"])
    return sorted(choices)


def build_plan(
    inventory: dict,
    changes: dict,
    mode: str,
    *,
    root: Path | None = None,
    max_graphics: int | None = None,
) -> dict:
    mode = mode.lower()
    changed_files = changes.get("files", [])
    tests = {test["name"]: test for test in inventory.get("tests", [])}
    modules_by_path = {
        _normalize(row.get("path", "")): row.get("module") for row in inventory.get("atlas_files", [])
    }
    reverse_includes = _reverse_includes(inventory)
    reasons: dict[str, set[str]] = defaultdict(set)
    if mode == "full":
        # FULL is an explicit whole-suite request.  Do not reduce it to the
        # files that happened to match the change; the reason remains visible
        # in the plan and the build uses the all target.
        for name in tests:
            reasons[name].add("modo FULL: todos os testes registrados")
    changed_modules: set[str] = set()
    affected_paths: set[str] = set()
    direct_targets: set[str] = set()
    affected_targets: set[str] = set()
    unknown_paths: list[str] = []
    warnings: list[str] = []
    docs_paths: list[str] = []
    cmake_changed = False
    tests_changed = False
    source_changed = False
    assets_changed = False
    structural = False

    for item in changed_files:
        path = _normalize(item["path"])
        module = _module_for_change(path, modules_by_path)
        changed_modules.add(module)
        if config.is_doc_path(path):
            docs_paths.append(path)
            continue
        if _is_cmake_change(path):
            cmake_changed = True
            structural = True
        if path.startswith("tests/"):
            tests_changed = True
        if path.startswith("assets/") or path.startswith("midia/"):
            assets_changed = True
        if _is_source_change(path):
            source_changed = True

        item_targets = _targets_for_paths(inventory, {path})
        direct_targets.update(item_targets)
        affected_paths.add(path)
        if path.endswith((".h", ".hpp", ".hh", ".hxx")):
            includers = reverse_includes.get(path, set())
            affected_paths.update(includers)
            affected_targets.update(_targets_for_paths(inventory, includers))
            if len(includers) >= 5:
                warnings.append(
                    f"header amplamente incluído: {path} ({len(includers)} consumidores diretos); "
                    "validar consumidores e considerar FULL"
                )
        affected_targets.update(item_targets)
        if not item_targets and not path.startswith(("docs/", "assets/", "midia/", "third_party/")):
            unknown_paths.append(path)

        # Direct test source / test target mapping.
        for test in inventory.get("tests", []):
            test_files = {_normalize(value) for value in inventory.get("test_files", {}).get(test["name"], [])}
            if path == _normalize(test.get("source_path") or "") or path in test_files:
                reasons[test["name"]].add(f"arquivo do teste ou fonte direta: {path}")

        # Test targets containing the changed test source.
        if path.startswith("tests/"):
            for target, sources in inventory.get("source_by_target", {}).items():
                if path in {_normalize(value) for value in sources}:
                    direct_targets.add(target)
                    for test in inventory.get("tests", []):
                        if test.get("target") == target:
                            reasons[test["name"]].add(f"target de teste direto: {target}")
            # Python/shell harnesses are not always represented as test_files by
            # the textual CMake parser.  Match the exact test suffix, never a
            # loose substring, so an unrelated test is not selected.
            stem = Path(path).stem
            for test in inventory.get("tests", []):
                test_name = test["name"]
                if test_name in {f"linblux_{stem}", stem} or test_name.endswith(f"_{stem}"):
                    reasons[test_name].add(f"harness de teste alterado: {path}")
            if path.startswith("tests/atlas/") and "linblux_atlas" in tests:
                reasons["linblux_atlas"].add(f"teste do ATLAS alterado: {path}")
            if path.startswith("tests/turbo/") and "linblux_turbo" in tests:
                reasons["linblux_turbo"].add(f"teste do TURBO alterado: {path}")

        # Asset rules are explicit because CMake does not model asset -> test
        # coverage.  They are intentionally conservative.
        if path.startswith(("tools/backup/", "tests/backup/", "packaging/systemd/")) and "linblux_backup" in tests:
            reasons["linblux_backup"].add(f"sistema de backup alterado: {path}")
        if path.startswith(("assets/", "midia/")):
            for test_name in _asset_tests(path):
                if test_name in tests:
                    reasons[test_name].add(f"recurso associado: {path}")
            if not _asset_tests(path):
                unknown_paths.append(path)

        if module in config.MODULE_TEST_HINTS:
            for test in inventory.get("tests", []):
                if module in test.get("modules", []):
                    reasons[test["name"]].add(f"módulo afetado: {module}")

        if config.module_for_path(path) == "tooling" or path.startswith("tools/atlas/"):
            if path.startswith("tools/atlas/") and "linblux_atlas" in tests:
                reasons["linblux_atlas"].add(f"ferramenta ATLAS alterada: {path}")
            elif path.startswith("tools/turbo/") and "linblux_turbo" in tests:
                reasons["linblux_turbo"].add(f"ferramenta TURBO alterada: {path}")
            elif not path.startswith("tools/"):
                unknown_paths.append(path)

    # A CMake change can alter targets and test registration.  We cannot infer
    # that safely from the old graph, so ask for a configure and a wide plan.
    if cmake_changed:
        structural = True
        warnings.append(
            "CMake/CTest alterado: configuração será reexecutada; seleção estrutural é limitada "
            "e FULL é recomendado antes da entrega"
        )
        for test in inventory.get("tests", []):
            if mode == "full" or (mode == "impact" and not test["graphics"]):
                reasons[test["name"]].add("mudança de CMake: cobertura estrutural")
        unknown_paths.append("CMakeLists.txt/CMakePresets")

    # Target graph: a test target that depends on an affected target is a
    # consumer.  This is the useful bridge from ATLAS target data to CTest.
    closure_cache: dict[str, set[str]] = {}
    for test in inventory.get("tests", []):
        target = test.get("target")
        if not target:
            continue
        if target not in closure_cache:
            closure_cache[target] = _test_target_closure(test, inventory)
        if closure_cache[target].intersection(affected_targets):
            hit = sorted(closure_cache[target].intersection(affected_targets))
            reasons[test["name"]].add(f"consumidor do target: {', '.join(hit[:4])}")

    # Direct source modules may not have a test target mapping, but ATLAS
    # modules still provide a conservative clue.  Do not match solely on names.
    if not reasons and (source_changed or assets_changed or tests_changed):
        baseline = _fallback_baseline(inventory, mode)
        for name in baseline:
            reasons[name].add("linha de base headless; cobertura específica não determinada")
        if baseline:
            warnings.append(
                "nenhum teste específico foi associado com segurança; "
                "linha de base headless executada com COBERTURA DESCONHECIDA"
            )
        else:
            warnings.append("nenhum teste específico ou linha de base disponível; COBERTURA DESCONHECIDA")

    selected_names = []
    filtered: dict[str, str] = {}
    graphics_limited = False
    for name, why in reasons.items():
        test = tests.get(name)
        if not test:
            continue
        accepted, reason = _mode_filter(test, mode)
        if accepted:
            selected_names.append(name)
        else:
            filtered[name] = reason or "filtrado"
    selected_names = sorted(set(selected_names))
    if not selected_names and mode == "fast" and reasons:
        # If all evidence pointed to graphics, do not pretend FAST covered it.
        warnings.append(
            "todos os candidatos foram excluídos do FAST por exigirem gráficos/lentidão; "
            "COBERTURA DESCONHECIDA — use IMPACT ou FULL"
        )
    if max_graphics is not None and mode == "impact":
        graphics = [name for name in selected_names if tests[name]["graphics"]]
        if len(graphics) > max_graphics:
            # Prefer faster graphical integrations when a cap is active; the
            # filtered list still records every omitted test and its reason.
            graphics.sort(key=lambda name: (tests[name]["duration_seconds"], name))
            for name in graphics[max_graphics:]:
                selected_names.remove(name)
                filtered[name] = f"limite de gráficos do IMPACT ({max_graphics})"
            graphics_limited = True
            warnings.append(
                f"limite de {max_graphics} testes gráficos aplicado; "
                f"{len(graphics) - max_graphics} não executado(s); use --max-graphics para ampliar"
            )

    # Build targets: test executables are sufficient because CMake builds
    # their dependency closure.  For CMake changes we additionally configure.
    build_targets = set()
    for name in selected_names:
        target = tests[name].get("target")
        if target:
            build_targets.add(target)
    # Compile the changed/affected libraries even when FAST intentionally has no
    # matching test (for example a rendering-only change in a no-GPU run).
    build_targets.update(direct_targets)
    build_targets.update(affected_targets)
    if cmake_changed and mode in {"impact", "full"}:
        build_targets.add("all")
    if mode == "full":
        build_targets.add("all")
    if "all" in build_targets and len(build_targets) > 1:
        build_targets = {"all"}
    if not build_targets and affected_targets and mode == "full":
        build_targets.update(affected_targets)

    selected_tests = []
    for name in selected_names:
        test = dict(tests[name])
        test["reasons"] = sorted(reasons.get(name, []))
        test["priority"] = _priority(test["reasons"])
        selected_tests.append(test)
    required_tests = [test for test in selected_tests if test["priority"] == "required"]
    recommended_tests = [test for test in selected_tests if test["priority"] == "recommended"]
    coverage = "known" if any(
        any("linha de base" not in reason for reason in test["reasons"]) for test in selected_tests
    ) else "baseline" if selected_tests else "none"
    if cmake_changed and mode == "fast":
        coverage = "unknown"
    if graphics_limited and coverage == "known":
        coverage = "partial"
    if not changes.get("files") and mode != "full":
        coverage = "none"
    estimated = sum(test["duration_seconds"] for test in selected_tests)
    graphics_count = sum(1 for test in selected_tests if test["graphics"])
    slow_count = sum(1 for test in selected_tests if test["slow"])
    plan = {
        "mode": mode,
        "changed_count": len(changes.get("files", [])),
        "changed": [item["path"] for item in changes.get("files", [])],
        "docs": docs_paths,
        "docs_only": bool(docs_paths) and not (source_changed or assets_changed or cmake_changed),
        "changed_modules": sorted(changed_modules),
        "affected_targets": sorted(affected_targets | direct_targets),
        "build_targets": sorted(build_targets),
        "selected_tests": selected_tests,
        "selected_names": [test["name"] for test in selected_tests],
        "required_names": [test["name"] for test in required_tests],
        "recommended_names": [test["name"] for test in recommended_tests],
        "required_count": len(required_tests),
        "recommended_count": len(recommended_tests),
        "required_estimated_seconds": round(sum(test["duration_seconds"] for test in required_tests), 3),
        "recommended_estimated_seconds": round(sum(test["duration_seconds"] for test in recommended_tests), 3),
        "filtered": filtered,
        "coverage": coverage,
        "coverage_unknown": coverage != "known",
        "unknown_paths": sorted(set(unknown_paths)),
        "structural": structural or cmake_changed,
        "configure_required": cmake_changed,
        "estimated_seconds": round(estimated, 3),
        "graphics_count": graphics_count,
        "slow_count": slow_count,
        "warnings": sorted(set(warnings)),
        "test_count": len(selected_tests),
        "atlas_updated": inventory.get("atlas_info", {}).get("updated"),
        "inventory_warnings": inventory.get("warnings", []),
    }
    return plan


def explain(plan: dict) -> str:
    lines = [
        f"Modo: {plan['mode'].upper()}",
        f"Arquivos alterados: {plan['changed_count']}",
        f"Targets: {', '.join(plan['build_targets']) or 'nenhum'}",
        f"Testes selecionados: {plan['test_count']} "
        f"(gráficos={plan['graphics_count']}, lentos={plan['slow_count']})",
        f"Prioridade: obrigatórios={plan.get('required_count', 0)}, "
        f"adicionais={plan.get('recommended_count', 0)}",
        f"Cobertura: {plan['coverage']}",
    ]
    if plan.get("budget_seconds"):
        cap = plan.get("per_test_timeout_seconds") or {}
        lines.append(
            f"Orçamento de testes: {plan['budget_seconds']:.0f}s; "
            f"timeout individual: gráficos={cap.get('graphics', '?')}s, "
            f"headless={cap.get('headless', '?')}s"
        )
    if plan.get("estimated_seconds"):
        lines.append(f"Duração estimada (histórico CTest): {plan['estimated_seconds']:.1f}s")
    if plan.get("unknown_paths"):
        lines.append(f"Sem mapeamento seguro: {', '.join(plan['unknown_paths'][:8])}")
    for test in plan.get("selected_tests", []):
        marker = "obrigatório" if test.get("priority") == "required" else "adicional"
        lines.append(f"- [{marker}] {test['name']}: {'; '.join(test['reasons'][:3])}")
    for warning in plan.get("warnings", []):
        lines.append(f"AVISO: {warning}")
    return "\n".join(lines)
