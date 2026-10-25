#!/usr/bin/env bash
# Instala o daemon de backup do LINBLUX na sessão do usuário:
#   - unit systemd de usuário (inicia com a sessão, reinicia em caso de falha)
#   - comando backup-status em ~/.local/bin
# Uso:
#   tools/backup/install-service.sh [--no-enable] [--project DIR] [--state-dir DIR]
set -euo pipefail

SELF=$(readlink -f "$0")
PROJECT="${LINBLUX_BACKUP_PROJECT:-$(cd "$(dirname "$SELF")/../.." && pwd)}"
STATE="${LINBLUX_BACKUP_STATE:-$HOME/.local/state/linblux-backup}"
PYTHON="${LINBLUX_BACKUP_PYTHON:-$(command -v python3)}"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
BIN_DIR="$HOME/.local/bin"
ENABLE=1

while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-enable) ENABLE=0 ;;
        --project) PROJECT="$2"; shift ;;
        --state-dir) STATE="$2"; shift ;;
        *) echo "argumento desconhecido: $1" >&2; exit 2 ;;
    esac
    shift
done

[ -x "$PYTHON" ] || { echo "python3 não encontrado" >&2; exit 1; }
[ -f "$PROJECT/tools/backup/backup.py" ] || { echo "projeto inválido: $PROJECT" >&2; exit 1; }

mkdir -p "$STATE" "$UNIT_DIR" "$BIN_DIR"
chmod 700 "$STATE"

sed -e "s|@PROJECT@|$PROJECT|g" \
    -e "s|@STATE@|$STATE|g" \
    -e "s|@PYTHON@|$PYTHON|g" \
    "$PROJECT/packaging/systemd/linblux-backup.service.in" \
    > "$UNIT_DIR/linblux-backup.service"

chmod +x "$PROJECT/tools/backup/backup.py" "$PROJECT/tools/backup/backup-status"
ln -sfn "$PROJECT/tools/backup/backup-status" "$BIN_DIR/backup-status"

echo "unit:            $UNIT_DIR/linblux-backup.service"
echo "comando:         $BIN_DIR/backup-status"
echo "estado isolado:  $STATE"

if [ "$ENABLE" -eq 1 ]; then
    exec "$PYTHON" "$PROJECT/tools/backup/backup.py" service install --project "$PROJECT" --state-dir "$STATE"
fi
echo "Instalação preparada; serviço não ativado (--no-enable)."
