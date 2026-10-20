# Continuidade da estabilização — 2026-10-20

Base confirmada: `8bd7f9f`, branch `fix/unified-3d-gizmos`. O commit que contém
este documento é um checkpoint funcional (consultar `git log -1`). Sem push.
Checkpoint de estabilização: `e035035`. Checkpoints anteriores desta missão:
`d550f85` (roteamento), `3148377` (viewport/axiais), `20e8d9f` (planos),
`a91d445` (studs/material), `6efaf9b` (startup/restart).
`.desenvolvimento/` já existia e foi preservado fora dos commits. Nenhum projeto pessoal
é usado pelos testes novos; os artefatos ficam em `/tmp/loblox-input-*`.

Incremento posterior: Ctrl+D duplica a Part selecionada na mesma posição, com
novo ID/nome e propriedades/classe preservadas; seleciona a cópia. Usa o mesmo
roteamento, sem repetir ao segurar D nem atuar em texto/modal/navegação. CTest
Release 6/6 passou: 1.575 checks headless e 223 de entrada real. Testes gráficos
usam Part ancorada para comparar posições sem separação física entre cópias;
o teste headless cobre também flags dinâmicas. A validação Sway/sanitizer abaixo
pertence ao checkpoint anterior. Commit do incremento: consultar `git log -1`.

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
  tela, congeladas no início. Vistas axiais antes falhavam em 10 casos de
  eixo/Scale/Rotate e 4 casos dos planos de Move.
- `TransformSession`: confirmação explícita também ao salvar; sincroniza o
  collider antes de liberar Bullet. Log distingue commit de cancel e motivo.
- `Lighting`: direção padrão normalizada como o setter, para evitar mudança
  de propriedade ao reabrir; setters rejeitam NaN/Inf.
- `SceneRenderer`: matriz dos studs agora aplica rotação antes da translação;
  12 amostras de pixels falhavam em Baseplate movida/rotacionada. Material dos
  studs é liberado antes do contexto (LSan encontrou leak real de 336 bytes).
- `Application`: consulta de arquivo sem exceção, primeiro uso sem projeto
  padrão não vira erro; evento startup_ready sinaliza inicialização concluída.
- `Diagnostics`: frame/correlação em todos os eventos; tool/change com origem,
  gizmo/drag_*, scene/selection_change e physics/edit_*.
- `LOBLOX_TRACE_INPUT=1`: dispatch nas bordas de mouse/ações de teclado/scroll;
  sem registrar movimento por frame.

## Evidência atual e próximos passos

Release incremental e Debug limpo: CTest 6/6 passaram. 1.520 verificações
headless; 300 casos / 7.529 verificações gráficas (inclui rotação/studs);
204 verificações na EditorUI com entrada X11 real; 4 processos do aplicativo
com primeiro uso e save/exit/restart. Toolbar, Move/Scale/Rotate,
Esc, modal, perda real de foco, Properties, repetição de tecla, exclusão durante
drag e liberação física foram exercitados. O teste de restart compara IDs,
transforms/propriedades (tolerância numérica), exclusão e nextId.

Sway 1.10.1 headless/pixman + XWayland privado: os 204 checks passaram e
4 launches/restarts passaram. Nenhuma entrada foi enviada ao desktop do usuário.
Reprodução como usuário comum: `bash tests/run_sway_input.sh`.

ASan/UBSan com detect_leaks=1: headless/help/startup passaram; 3 testes gráficos
falham no LSan por ~194 kB da pilha Mesa/GLX (também em probe só InitWindow /
CloseWindow, sem Loblox). O leak de 336 bytes do renderer desapareceu após a
correção. Não houve supressão nem desativação de leaks. A suíte de sanitizers
completa NÃO está verde. Logs: build/stabilization-sanitize/full-leak-check.log
e Testing/Temporary/LastTest.log. Builds parciais de /tmp foram preservados em
build/failed-tmp-* após esgotarem o tmpfs de 512 MB.

Medições em llvmpipe: startup_ready ~0,75–1,05 s; RSS ~127–131 MiB;
CPU ~165% (LP_NUM_THREADS=2) a ~465% (padrão) de um núcleo. Fixture Xvfb
1280×800/cap 60 teve amostra mediana 18,90 ms/p95 23,31 ms; em concorrência
ficou mais lenta. Sway headless/pixman/2 threads: 36,13 ms/p95 71,84 ms em uma
amostra. Não comparar como regressão nem extrapolar para GPU/notebook.
Com checagens adicionais e testes concorrentes, outra amostra Sway teve mediana
51,31 ms/p95 74,44 ms. Isso não comprova fluidez no hardware de destino.

Ambiente: pkg-config anuncia raylib 6.2.0, mas headers/runtime instalados dizem
6.5-dev. Não substituí a biblioteca do usuário; resultados pertencem à instalação
atual. Backup histórico informado existe; conteúdo não foi revalidado.

Comandos: `cmake --build build -j2`; `ctest --test-dir build --output-on-failure`.
Próximo passo: procedimento manual de `docs/INPUT_AND_VALIDATION.md` na sessão
física do notebook. Para reproduzir LSan, usar detect_leaks=1 no build sanitizer.

Pendentes: leak externo Mesa/GLX, sessão física Sway/XWayland, GPU, DPI real,
sessões longas e cenas grandes no notebook. Não declarar estabilização completa.
