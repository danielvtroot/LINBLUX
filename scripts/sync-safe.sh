#!/usr/bin/env bash
# Valida o commit preparado pelo desenvolvedor e envia sem reescrever historico.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
expected=https://github.com/danielvtroot/LINBLUX.git
[[ "$(git remote get-url origin)" == "$expected" ]] || { echo 'Remoto origin inesperado.' >&2; exit 1; }
git diff --check
[[ -z "$(git status --porcelain)" ]] || { echo 'Registre e valide as alteracoes antes de sincronizar.' >&2; exit 1; }
git push origin HEAD:main
