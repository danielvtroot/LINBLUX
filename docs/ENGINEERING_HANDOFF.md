# Continuidade da estabilização — 2026-10-20

Base confirmada: `8bd7f9f`, branch `fix/unified-3d-gizmos`. O commit que contém
este documento é o checkpoint da etapa 1 (consultar `git log -1`). Sem push.
`.desenvolvimento/` já existia e foi preservado fora dos commits. Nenhum projeto pessoal
é usado pelos testes novos; os artefatos ficam em `/tmp/loblox-input-*`.

## Causas confirmadas

O teste `loblox_editor_input` envia eventos X11 reais por xdotool para uma
janela raylib/rlImGui/EditorUI em Xvfb. Antes das correções falhou em 3 pontos:
RMB sobre handle não captura; LMB durante navegação troca seleção; Ctrl+S move
a câmera para trás. Os quatro testes antigos passaram na mesma revisão.

O hover do gizmo bloqueava toda a câmera, mas a seleção não consultava a
captura. A câmera lia teclas globais sem verificar Ctrl. Os atalhos usavam foco
do frame anterior e repetição automática. A sessão continuava sob popup/widget.

## Correções e contrato

- `InputRouting.h`: snapshot, contexto e decisão única, sem estado duplicado.
- `EditorUI.cpp`: contexto após UI/modais, foco da viewport, atalhos sem repeat,
  cancelamento explícito, seleção atualizada antes do desenho, ferramenta única.
- `EditorCamera.cpp`: snapshot recebido, gestos Look/Pan iniciados por press,
  captura persistente fora da imagem, sem câmera concorrente com drag.
- `TransformSession`: confirmação explícita também ao salvar; log distingue
  commit de cancel. `Diagnostics`: frame em todos os eventos.
- `LOBLOX_TRACE_INPUT=1`: dispatch nas bordas de mouse/ações de teclado/scroll;
  sem registrar movimento por frame.

## Evidência atual e próximos passos

Build incremental e CTest 5/5 passaram após a correção (teste gráfico de entrada:
24 verificações). Regressão determinística adicional de roteamento em validação.
Ainda ampliar o teste integrado para toolbar, drags dos três modos, Esc, modal,
foco, Properties, exclusão e save/restart; validar degenerações dos gizmos.

Comandos: `cmake --build build -j2`; `ctest --test-dir build --output-on-failure`.
Próximo comando recomendado: executar build e CTest após ampliar os casos em
`tests/editor_input_main.cpp` e `tests/test_main.cpp`.

Pendentes: build limpo Debug, sanitizers atuais, medição atual de performance,
revisão final. Sway/XWayland e hardware real não foram validados.
