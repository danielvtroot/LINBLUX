# Continuidade da estabilização — 2026-10-20

Base confirmada: `8bd7f9f`, branch `fix/unified-3d-gizmos`. O commit que contém
este documento é um checkpoint funcional (consultar `git log -1`). Sem push.
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
- A imagem da viewport agora tem ID próprio (InvisibleButton + AddImage): o
  ActiveId do ImGui pertence à viewport durante drag e não é confundido com UI.
- `Gizmo`: interseções singulares usam direção/tangente do mesmo layout de
  tela, congeladas no início. Vistas axiais antes falhavam em 10 casos.
- `TransformSession`: confirmação explícita também ao salvar; sincroniza o
  collider antes de liberar Bullet. Log distingue commit de cancel e motivo.
- `Lighting`: direção padrão normalizada como o setter, para evitar mudança
  de propriedade ao reabrir; setters rejeitam NaN/Inf.
- `Diagnostics`: frame/correlação em todos os eventos; tool/change com origem,
  gizmo/drag_*, scene/selection_change e physics/edit_*.
- `LOBLOX_TRACE_INPUT=1`: dispatch nas bordas de mouse/ações de teclado/scroll;
  sem registrar movimento por frame.

## Evidência atual e próximos passos

Build incremental e CTest 6/6 passaram: 1.328 verificações headless; 300 casos /
7.509 verificações gráficas; 191 verificações na EditorUI com entrada X11 real;
3 processos do aplicativo com save/exit/restart. Toolbar, Move/Scale/Rotate,
Esc, modal, perda real de foco, Properties, repetição de tecla, exclusão durante
drag e liberação física foram exercitados. O teste de restart compara IDs,
transforms/propriedades (tolerância numérica), exclusão e nextId.

Medições desta execução em llvmpipe: startup até evento de carga 0,729 s;
VmRSS 129.536 kB; CPU 480% de um núcleo (software multithread). Fixture 1280×800,
cap 60: mediana 18,90 ms, p95 23,31 ms. Não extrapolar para GPU/notebook.

Comandos: `cmake --build build -j2`; `ctest --test-dir build --output-on-failure`.
Próximo comando recomendado: configurar build Debug limpo e ASan/UBSan em
diretórios novos de `/tmp`, executar CTest completo e revisar o diff.

Pendentes: build limpo Debug, sanitizers atuais e revisão final. Sway/XWayland,
GPU, DPI real, sessões longas e cenas grandes no notebook não foram validados.
