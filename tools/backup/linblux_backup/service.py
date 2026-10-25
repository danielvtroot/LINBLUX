"""User service installation, with a cron watchdog on Linux without systemd."""
from __future__ import annotations

import fcntl
import hashlib
import json
import os
from pathlib import Path
import shlex
import signal
import shutil
import subprocess
import sys
import time

from .runner import BackupRunner
from .store import StoreError


def cron_text(previous: str, command: list[str], log: Path, project: Path) -> str:
    tag = "# LINBLUX-BACKUP-" + hashlib.sha256(str(project).encode()).hexdigest()[:12]
    if any("\n" in str(value) or "\r" in str(value) for value in [*command, log]):
        raise StoreError("caminho de serviço contém quebra de linha")
    # Cron interprets percent before the shell, even inside shell quotes.
    invocation = (shlex.join(command) + " >> " + shlex.quote(str(log)) + " 2>&1").replace("%", r"\%")
    kept = [line for line in previous.splitlines() if not line.endswith(tag)]
    return "\n".join([*kept, f"@reboot {invocation} {tag}", f"* * * * * {invocation} {tag}", ""])


def start(runner: BackupRunner, config_path: str | None = None) -> dict:
    runner.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    with (runner.state_dir / "service-start.lock").open("a+") as guard:
        fcntl.flock(guard.fileno(), fcntl.LOCK_EX)
        if runner.daemon_running():
            return {"active": True, "started": False}
        command = [sys.executable, str(runner.project / "tools/backup/backup.py"), "run",
                   "--project", str(runner.project), "--state-dir", str(runner.state_dir)]
        if config_path:
            command.extend(["--config", str(Path(config_path).resolve())])
        env = os.environ.copy()
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX"):
            env.pop(key, None)
        log_path = runner.state_dir / "daemon.log"
        descriptor = os.open(log_path, os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o600)
        with os.fdopen(descriptor, "ab") as log:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                       cwd=runner.project, env=env, start_new_session=True)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if runner.daemon_running():
                return {"active": True, "started": True, "pid": process.pid, "log": str(log_path)}
            if process.poll() is not None:
                raise StoreError(f"monitor encerrou ({process.returncode}); consulte {log_path}")
            time.sleep(.05)
        process.terminate()
        process.wait(timeout=3)
        raise StoreError("monitor não adquiriu a trava em 3 segundos")


def install(runner: BackupRunner, backend: str, config_path: str | None = None) -> dict:
    systemd = shutil.which("systemctl")
    if backend == "auto":
        ready = systemd and subprocess.run([systemd, "--user", "show-environment"],
                                          capture_output=True, timeout=5).returncode == 0
        backend = "systemd" if ready else "cron"
    runner.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config")))
    script = runner.project / "tools/backup/backup.py"
    common = ["--project", str(runner.project), "--state-dir", str(runner.state_dir)]
    if config_path:
        common += ["--config", str(Path(config_path).resolve())]
    if backend == "systemd":
        if not systemd:
            raise StoreError("systemctl indisponível; use --backend cron")
        unit = config_home / "systemd/user/linblux-backup.service"
        unit.parent.mkdir(parents=True, exist_ok=True)
        def quote(value: str) -> str:
            if "\n" in value or "\r" in value:
                raise StoreError("caminho de serviço inválido")
            return '"' + value.replace("\\", "\\\\").replace('"', '\\"').replace("%", "%%").replace("$", "$$") + '"'
        invocation = " ".join(quote(arg) for arg in [sys.executable, str(script), "run", *common])
        unit.write_text("[Unit]\nDescription=LINBLUX AutoBackup\nAfter=network-online.target\n\n"
                        "[Service]\nType=simple\nExecStart=" + invocation + "\nRestart=on-failure\nRestartSec=20\n"
                        "NoNewPrivileges=yes\nPrivateTmp=yes\nEnvironment=PYTHONDONTWRITEBYTECODE=1\n"
                        "\n[Install]\nWantedBy=default.target\n")
        for args in (("daemon-reload",), ("enable", "--now", "linblux-backup.service")):
            result = subprocess.run([systemd, "--user", *args], capture_output=True, timeout=15)
            if result.returncode:
                raise StoreError("systemd recusou a instalação; nenhum sucesso foi presumido")
        result = {"backend": backend, "unit": str(unit), "enabled": True}
    else:
        executable = shutil.which("crontab")
        if not executable:
            raise StoreError("nem systemd de usuário nem crontab disponíveis")
        with (runner.state_dir / "service-install.lock").open("a+") as guard:
            fcntl.flock(guard.fileno(), fcntl.LOCK_EX)
            old = subprocess.run([executable, "-l"], capture_output=True, text=True, timeout=5)
            if old.returncode not in (0, 1):
                raise StoreError("não foi possível ler o crontab do usuário")
            command = [sys.executable, str(script), "service", "start", *common]
            updated = cron_text(old.stdout, command, runner.state_dir / "watchdog.log", runner.project)
            saved = subprocess.run([executable, "-"], input=updated, capture_output=True, text=True, timeout=5)
            if saved.returncode:
                raise StoreError("não foi possível instalar o watchdog no crontab do usuário")
        result = {"backend": "cron", "enabled": True, "restart_delay_max_seconds": 60}
        result.update(start(runner, config_path))
    record = runner.state_dir / "service.json"
    record.write_text(json.dumps(result, indent=2) + "\n")
    record.chmod(0o600)
    return result


def restart(runner: BackupRunner, config_path: str | None = None) -> dict:
    if runner.daemon_running():
        pid = int(runner.lock_path.read_text().split()[0])
        process = Path(f"/proc/{pid}")
        command = (process / "cmdline").read_bytes().split(b"\0")
        expected = str(runner.project / "tools/backup/backup.py").encode()
        if process.stat().st_uid != os.getuid() or expected not in command or b"run" not in command:
            raise StoreError("monitor não identificado com segurança; nenhum processo encerrado")
        os.kill(pid, signal.SIGTERM)
        deadline = time.monotonic() + 10
        while runner.daemon_running() and time.monotonic() < deadline:
            time.sleep(.05)
        if runner.daemon_running():
            raise StoreError("monitor ainda finaliza um ciclo; nova instância não iniciada")
    return start(runner, config_path)
