#!/usr/bin/env bash
# Garante que a ausencia de display termina de forma controlada (exit 1),
# em vez de prosseguir com uma janela invalida e causar segfault.
set -u
unset DISPLAY
unset WAYLAND_DISPLAY
output="$("$1" 2>&1)"
status=$?
if [ "$status" -ne 1 ]; then
    printf 'FALHA: exit %d (esperado 1) sem display\n%s\n' "$status" "$output"
    exit 1
fi
case "$output" in
    *"Falha ao criar a janela grafica"*) ;;
    *)
        printf 'FALHA: mensagem controlada ausente sem display\n%s\n' "$output"
        exit 1
        ;;
esac
exit 0
