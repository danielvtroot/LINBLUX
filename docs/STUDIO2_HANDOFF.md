# Studio 2 — continuidade

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
não uma segunda hierarquia de Instances. Visuais/imagens/skybox/vida/Shift Lock
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
Próximo: Health/void/HUD. Guia: docs/STUDIO2_GUIDE.md. Sem push.
