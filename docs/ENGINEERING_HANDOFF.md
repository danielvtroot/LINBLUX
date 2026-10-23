# Continuidade — R6 e primeiro runtime local — 2026-10-21

Migração atual do frontend: [RMLUI_HANDOFF.md](RMLUI_HANDOFF.md).

Mouse Lock atual: [MOUSE_LOCK_FIX.md](MOUSE_LOCK_FIX.md) — captura real,
orientação por frame, câmera sobre o ombro e validação temporal.

Checkpoint anterior: [SHIFT_RIG_DECALS.md](SHIFT_RIG_DECALS.md) — Shift Lock
após a física, indicador original e decals independentes por face do R6.

Anterior: [CAMERA_HUD_FIXES.md](CAMERA_HUD_FIXES.md) — câmera Studio
FPS/Focus explícita e retorno à Health minimalista, preservando menu/topbar.

Player UI 2026 (23/10): [PLAYER_UI_2026.md](PLAYER_UI_2026.md). Escopo separado
do Studio: topbar, menu, Settings e Health. Referências JPEG novas analisadas.
O histórico abaixo antecede RmlUi; não descreve a UI padrão atual.

Retomada atual: [STUDIO3_HANDOFF.md](STUDIO3_HANDOFF.md), com correções de
rig, Fall, céu procedural, frontend e evidências Release/Debug/Sway.

Evolução posterior Studio 2: ver [STUDIO2_HANDOFF.md](STUDIO2_HANDOFF.md)
para hierarquia, Properties, Lua executável, IDs de animação e Health/void.
O restante deste arquivo conserva a evidência histórica do checkpoint R6.

## Estado atual: protótipo jogável validado em software, alvo físico pendente

Base desta evolução: `d6f1e95` (Ctrl+D), branch preservado. Sem push.
Checkpoints: `d8965ab` (vertical slice), `33801f1` (input/save e GUI real).
O checkpoint de câmera que contém este documento é identificado por `git log -1`.
`src/game/Rig.*`: R6 original Y-up/frente -Z, seis corpos visuais,
HumanoidRootPart interno, seis juntas com anchors/rest separados da pose;
SLERP, tracks/keyframes, Walk original de 0,8 s e Idle editáveis.
`src/game/Runtime.*`: cópia transacional da Scene, Bullet independente,
uma cápsula dinâmica bloqueada em rotação, passos fixos 60 Hz/limite 6,
spawn determinístico, WASD relativo à câmera, pulo, pause/reset/stop.
`GamePanels.cpp`: Game/Rig Editor/Animation Editor/Game Settings, F5,
Shift+F5, Esc, câmera Classic/Follow/First Person e sweep contra obstáculos.
O runtime nunca guarda ponteiros da cena de autoria. Template R6 não cria
seis corpos físicos; apenas o avatar runtime tem cápsula. Sem auto-step de
escadas, ragdoll, multiplayer, scripts executáveis ou animações externas.

JSON v1 continua para mapas sem dados de gameplay; v2 embute `game` com
assets versionados, joints, clips e PlayerSettings, pela mesma escrita atômica.
Lua declarativo v1 recusa exportar gameplay para não perder assets.
Prévia trabalha numa cópia de animação; Save Animation ou Ctrl+S atualiza
asset/projeto. Novos rigs não sobrescrevem a caminhada já personalizada.

CTest Release e Debug 8/8 passaram na versão final. GUI edita cor, tempo 0,25 s e Right Shoulder
55 graus por eventos reais, insere key, cancela retiming com Esc, usa Save As,
Ctrl+S, Play/WASD/Space, pausa/Settings/reset/zoom/perda de foco e Stop.
Core cobre também Follow e referências inválidas sem rig. Save As antes
ignorava o draft: agora passa pelo mesmo helper de Ctrl+S/Save Animation.
Use as Walk respeita falha de save. Gizmo do rig compartilha Local/Snap.
F5/Esc/gameplay usam o mesmo snapshot/RouteInput após UI/modais do frame atual.
Respawn é transacional, preservando a cápsula anterior se o candidato falhar.
Screenshots inspecionadas em `/tmp/loblox-game-ui-*`. O restart Xvfb
intermitente falhou ao conectar ao display entre processos; `-noreset`
eliminou a falha na repetição. Restart agora cobre 6 processos, incluindo
salvar/fechar/reabrir R6, Walk modificada e configurações do Player.

Última causa reproduzida: com primeira pessoa desativada, zoom entrava na
cabeça; a altura do alvo também ignorava proporções editadas. O novo teste
falhou antes da correção. PlayerCamera agora usa a cabeça de Rig.evaluate,
limita zoom pela saída das mesmas caixas visuais e oculta o avatar se uma
parede ainda forçar a câmera para dentro dele, sem mudar o modo de controle.
Regressões cobrem cabeça/torso redimensionados e parede lateral.

Resultados finais: 252 checks de game; 1.575 headless anteriores; 300 casos /
7.529 checks gráficos; 208 checks de GUI game e 223 de input do editor, todos
sem falhas funcionais. Restart Xvfb abre 6 processos reais. Sway/XWayland
privado (headless/pixman, usuário comum) passou novamente 208 checks e 4
launches/restarts, sem enviar entrada à sessão pessoal.

Comandos: `cmake -E env TMPDIR=$PWD/build/compiler-tmp cmake --build build -j2`;
`ctest --test-dir build --output-on-failure -j2`.
Build Debug inicialmente limpo: `build/r6-clean-debug`, CTest final 8/8.
ASan/UBSan: `build/stabilization-sanitize`, com detect_leaks=1 e halt_on_error=1.
Headless 2/2 passaram (252 + 1.575 checks); GUI fez 208 checks sem falha, mas
LSan encerrou com erro: 194.140 bytes/865 alocações em libGLX_mesa.so.0.
Mesma assinatura reproduzida por InitWindow/CloseWindow, sem editor/core.
Sem suppressions/desativação de leaks: sanitizer gráfico NÃO está verde.
Evidência: Testing/Temporary/LastTest.log em cada build. Artefatos da última GUI
Release: /tmp/loblox-game-ui-Dm4tBC; Sway: /tmp/loblox-sway-ifFgSe.

Medição Release llvmpipe: startup 0,829 s, RSS 130.748 kB; Play 138,2 MiB,
mediana 61,67 ms/p95 76,54 ms sob testes concorrentes. No Sway privado,
Play 136,1 MiB, mediana 37,54 ms/p95 57,17 ms. Não comprova 60 FPS nem o
orçamento real do notebook: host/software/compositor e carga são diferentes.
`git diff --check` passou; .desenvolvimento/ permanece intocado e fora dos commits.

Próximo comando no notebook: `./build/loblox`; executar passos 1–9 e checklist
manual do guia, coletando JSONL se houver conflito. Não iniciar outra grande
refatoração antes dessa validação física e de sessões/cenas maiores.
Sway físico/GPU/DPI/3 GB reais continuam pendentes. Guia e limites completos:
`docs/R6_PLAY_GUIDE.md`. Sem auto-step, camadas Action, preferências pessoais
persistentes, Jump/Fall assets atribuíveis, Play Here/F8 ou quality/fullscreen
no menu. Esses itens não são apresentados como implementados.

## Histórico de estabilização preservado

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
- `RmlFrontendSupport.*`: política de textura da interface (ícones em 96 px,
  logo/PNG reduzidos por média de área, alfa premultiplicado, cadeia de mip e
  filtro trilinear com bias de LOD). O atlas de fontes continua no caminho
  original do RmlUi.
- `ImageAssets.cpp` + `SceneRenderer.cpp`: texturas 3D com cadeia de mip, filtro
  trilinear (anisotropia quando a extensão existe) e mistura com alfa
  premultiplicado. Sem isso a textura preto e branco minificada virava ruído
  serrilhado e o RGB sob alfa zero vazava para a arte.
- Ordem do blend mode e do `rlSetTexture` nos quads texturados: trocar o modo de
  mistura esvazia o batch do raylib e apaga a textura registrada.

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
