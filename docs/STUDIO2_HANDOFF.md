# Studio 2 — continuidade

## Rodada B–G (checkpoints)

Missão frontend concluída até o limite desta sessão. Commits (branch
fix/unified-3d-gizmos, sem push):

- `a8bb7ef` (Etapa A) ícones por classe no Explorer, drag-target highlight.
- `e89deb2` (Etapa A) ribbon reorganizada: abas Avatar/Script; Game só
  Play/Stop/Settings, Stop desabilitado fora do Play.
- `217fa00` (Etapa B) Script Editor: tokenizador Lua por frame desenha o
  código (texto nativo transparente com seleção/undo nativos preservados),
  caret próprio, indentação automática no Enter, barra vermelha e
  contagem de erros de runtime por script e mensagens clicáveis no Output
  que abrem o script.
- `ab418f8` (Etapa C) loblox.png na toolbar/ícone da janela/menu Esc
  (proporção preservada); cabeça vira superelipsoide fechado (2/n, n≈4.5)
  com Lambert por quad; rosto1.png desenhado como quad na face -Z da
  cabeça acompanhando a rotação (fallback: olhos mínimos).
- `6c2d548` (Etapa D) skybox um quad por face ancorado na câmera (sem
  shader/cubemap): 4 faces do sky1/ + Back/Down opcionais (cor sólida);
  fog por distância no Lighting (fogColor/fogDensity persistidos
  opcionalmente, leitura tolerante v1/v2), aplicado no sombreamento CPU
  e no material cacheado da base; Properties com FogColor/FogDensity.
- `4f1d424` (Etapa E) importador portátil assets/ (assets dir relativo ao
  executável, id FNV-1a do conteúdo) e Decal/Texture na Part como
  atributos persistidos do JSON v3: decal_image/decal_face (6 faces)/
  decal_transparency/decal_rotation (0/90/180/270)/decal_tint (RRGGBB),
  texture_image/texture_face/texture_transparency/texture_repeat_u/v/
  texture_offset_u/v; quads acompanham Position/Rotation/Size com offset
  0.02 contra z-fight; Properties com Importar/Face/Transparência/Rotação/
  Tint; Explorer mostra linhas Decal/Texture sob a Part; acessível de Lua
  pelos bindings de atributos já existentes.
- `5ae4add` (Etapa F) Shift Lock real: enableShiftLock nas Game Settings
  (persistido opcionalmente), Left Shift alterna durante Play; personagem
  alinhado à câmera (pipeline cameraLock), target com offset direito
  moderado, cursor capturado, badge SHIFT LOCK ON/OFF + retículo próprio;
  Esc pausa (captura liberada), Stop/respawn nunca deixam lock preso.
  HUD do Play ganha logo inline 18 px.

Validação desta rodada: build incremental limpo (sem erros/warnings),
CTest 10/10 após cada etapa, `git diff --check` OK, fixtures críticos
(loblox_game_ui 208 checks, editor_restart) passam. Assets originais do
usuário preservados (loblox.png, rosto1.png, sky1/ nunca foram alterados,
carga por caminhos relativos ao executável).

Limitações registradas com transparência:
1. Captura real de GUI ainda não validada neste ambiente: rodar
   `tests/run_sway_input.sh` recusa root e a sessão Wayland do labubu
   só autoriza o dono (Invalid MIT-MAGIC-COOKIE); tentativa sway headless
   como labubu caiu no XWayland (broken pipe). Recomendado: o usuário abre
   o Studio e confere visualmente (Explorer/ribbon/logo, cabeça/rosto,
   skybox, decal, Shift Lock: Shift→Esc→Resume→Shift→Stop).
2. Decal/Texture são atributos da Part (1 decal + 1 texture por Part),
   não instâncias filhas independentes no Explorer-reparent: modelo
   enxuto e persistente, substituto futuro por classes próprias quando
   houver espaço (Lua continua via atributos existentes).
3. Autocomplete no Script Editor não implementado (etapa B inscrita como
   pendente); scroll horizontal não aplicável (multiline com wrap própria);
   fonte é a padrão monoespaçada do ImGui.
4. Rosto por Properties ainda não exposto (futuro: selector de face nos
   Scripts/cabeça, sem sobrescrever rigs existentes).
5. Fog não aplicável a quads texturados (faces de decal/textura) nesta
   versão.

Próximo comando: `./build/loblox` para conferência visual pelo usuário;
`ctest --test-dir build --output-on-failure -j2` para regressão.

## Estado do checkpoint final desta rodada

Há avanço funcional nas fases A–E, mas elas não cobrem todos os requisitos
da missão (limites abaixo). A missão NÃO está concluída. Commits: `6bfa1fa` (hierarquia/UI),
`cc21833` (Lua), `3c29611` (IDs/scripts padrão). O commit deste documento
inclui Health/void/HUD e correções de lifecycle/IDs; consultar `git log -1`.

Studio: 1.363 checks; Game: 252; suite antiga: 1.575; GUI Studio: 95;
GUI Game: 208; input antigo: 223; gizmos: 300 casos/7.529 checks.
Release/Debug completos 10/10 passaram; rodada final arquivou resultados em
build/studio2-release-validation.log e build/studio2-debug-validation.log.
ASan/UBSan detect_leaks=1 headless 3/3 passaram. GUI faz 95 checks sem falha,
mas LSan REPROVA por 194.140 bytes/865 alocações em libGLX_mesa.so.0 (assinatura
anterior, sem suppressions). Não declarar sanitizer gráfico limpo.
Evidência gráfica ASan: build/studio2-asan-gui-validation.log. Headless:
build/studio2-asan-headless-validation.log; usar detect_leaks=1/UBSan halt.
Sway privado headless/pixman: 95 checks e 4 restarts passaram; sessão física
do notebook/GPU/DPI/3 GB ainda pendente. Artifacts: /tmp/loblox-sway-FGD2TR,
/tmp/loblox-studio-ui-SpuFjS. Original PNG hashes seguem iguais ao início.

Health: dano/cura, eventos, Dead, cooldown de contato Bullet, reset e respawn;
HUD real conferida em captura 75/100. 70 resets não acumularam listeners/bodies.
Void configurável remove Parts só do runtime e mata/respawna jogador.
Regressão vermelha reproduziu Died duplicado quando Lua matava em CharacterAdded;
agora distingue morte inicial de morte disparada por script. JSON v3 também
preserva IDs não convencionais de Workspace/Lighting, sem remapear para 1/2.
Revisão final reproduziu perda de Script filho de Part ao salvar/copiar para
runtime: serializer não descia nos filhos de Part. Travessia corrigida e teste
de round-trip + execução incluído (2 falhas antes da correção).
Bindings também limitam expansão nativa a 10.000 nós/8 MiB de conteúdo;
Clone calcula o custo antes da cópia para não contornar a quota da VM Lua.

Medições finais disponíveis (software, não alvo): startup 0,961 s/RSS 130.292 kB;
Explorer com 1.000 nós adicionados: 79,38 ms/frame, pico 140,1 MiB em execução
concorrente. Isso não comprova fluidez no notebook nem ausência de regressões
em GPU. Logs de testes novos são isolados em /tmp/loblox-studio-*.
Play Release: mediana 57,18 ms/p95 70,71 ms, pico 137,3 MiB; CPU de idle
243,6% de um núcleo nessa amostra llvmpipe. Não extrapolar para GPU real.

Próximo passo: F, assets de imagem portáveis/Decal/Texture, cabeça/rosto, depois
G/H (Sky/branding/shaders/perfis/Shift Lock). NÃO usar caminhos absolutos do
usuário como referência permanente. Antes disso, validar fluxo manual do guia
no notebook; continuar cobrindo input e Play aprovados. Próximo comando:
`./build/loblox`, ou `ctest --test-dir build --output-on-failure -j2`.

## Histórico e contratos preservados

Base: `894e89c`, branch `fix/unified-3d-gizmos`; nenhum push.
Baseline Release: build e CTest 8/8 passaram antes de mudanças.
Worktree inicial sem mudanças rastreadas; `.desenvolvimento/`, `loblox.png`,
`rosto1.png` e `sky1/` são arquivos preexistentes preservados.

Prioridade: hierarquia verdadeira, Explorer/Properties utilizáveis, depois Lua
executável durante Play, sem mudar o controller/animações já aprovados.
Limitação confirmada: Scene.validate e ProjectIO exigem Parts diretamente em
Workspace, embora renderer/Physics já consumam um índice plano de Parts.
Folder/Model serão agrupadores sem transform próprio; reparent mantém o
transform mundial da Part e seu ponteiro/ID.

Recursos inspecionados: logo 1600×1600 RGBA com alpha 0–255; rosto 523×477
RGBA com alpha 0–255, orientado para leitura frontal; quatro céus 1024×1024
RGBA opacos. Faltam Back e Down. Originais não foram alterados. A logo tem
RGB magenta em pixels transparentes: respeitar alpha, não usar chroma key.

Comandos: `cmake --build build -j2`; `ctest --test-dir build --output-on-failure -j2`.
O build usa TMPDIR=build/compiler-tmp neste ambiente (tmpfs pequeno).
Implementado: Folder/Model organizacionais, Parent verdadeiro com ciclos/depth
budget, clone de subárvore, Delete seguro, drag/drop real, busca nome/classe,
F2, menus de inserção, reveal de ancestrais e JSON v3 transacional. Renderer/
Bullet usam o mesmo índice de Parts aninhadas. JSON v1/v2 continuam aceitos.
Properties de Part/Folder/Model tem busca, categorias, duas colunas ajustáveis,
Parent, valores numéricos finitos e Enter/Esc para campos de texto/transform.
Novo layout reserva mais largura à direita, respeita layout salvo e oferece
View → Reset Layout. Rig/painéis de animação anteriores preservados.

Testes: 1.045 checks Studio headless; 1.000 nós create/validate/round-trip em
18,9 ms numa amostra Release, não uma medição do notebook. Regressões antigas
passaram; teste GUI novo cobre botões Folder, F2, drag/drop e save/reload.
A fixture usa press/release entre frames, não xdotool key instantâneo.

Checkpoint Explorer/Properties: `6bfa1fa` (após baseline `8c003f8`).
Lua 5.4 real integrado ao Play/Stop: módulos lazy/cacheados, tarefas/eventos,
validações, handles por ID/geração, source persistido e Script Editor/Output.
Quota de 8 MiB Lua/100k instruções por resume, limites de tarefas/conexões,
sem bibliotecas de SO/arquivos/debug/package e sem pcall para capturar quotas.
Sandbox in-process não é proteção absoluta de código hostil (ver guia).
Instalada liblua5.4-dev (runtime já existia), sem nova dependência pesada.

Studio headless: 1.082 checks, inclusive loops, OOM, ModuleScript, eventos,
Destroy, espera e Play/Stop repetidos. ASan/UBSan detect_leaks=1 passou essa
suíte sem falhas. GUI Studio: 80 checks por xdotool, incluindo código digitado
na interface → Ctrl+S → Play → transform runtime alterado → Stop → mapa autoral
intacto → reload do source. Captura: /tmp/loblox-studio-ui-psPuFG/script.png.
Testes de GUI usam lotes de até 8 caracteres por frame para não lotar a fila
de caracteres raylib numa fixture síncrona. Não é truncamento do editor real.

Limites atuais: Ctrl+C/V referencia a origem viva, não snapshot independente;
sem seleção múltipla nem undo/redo ainda. Storage guarda scripts/módulos e
folders, não Parts físicas fora de Workspace. Rig continua asset de autoria,
não uma segunda hierarquia de Instances. Visuais/imagens/skybox/Shift Lock
da missão nova ainda pendentes. Script Editor ainda não tem syntax highlight,
autocomplete ou navegação de busca; linhas/Tab/edição/undo de texto estão reais.
Lua/editor checkpoint: `cc21833`, CTest 10/10. Animator:SetAnimation e IDs
anim:// estão implementados e cobertos por 1.099 checks headless. Jump agora
é asset editável, com a mesma pose aérea padrão. Novos projetos recebem R6 e
scripts padrão; projetos carregados não são alterados, instalação é opt-in.
Restart encontrou apenas ordem global da fixture após adicionar serviços:
IDs, objetos e game eram iguais, mas Parts appended no final não eram preorder.
A fixture agora insere Parts antes dos serviços, mantendo comparação estrita
do documento inteiro e a ordem real dos filhos. Restart corrigido passou os
6 processos; os outros 9 testes haviam passado na mesma revisão de produção.
Health/void/HUD foram concluídos depois deste checkpoint, conforme o estado
atual acima. Guia: docs/STUDIO2_GUIDE.md. Sem push.
