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

Infraestrutura de Script/LocalScript/ModuleScript e containers de scripts
persistidos está preparada. A VM real e Script Editor estão em implementação;
isso ainda NÃO comprova execução Lua. Instalada liblua5.4-dev (biblioteca
runtime 5.4 já existia). Lua é dependência pequena, não Luau/engine Roblox.

Limites atuais: Ctrl+C/V referencia a origem viva, não snapshot independente;
sem seleção múltipla nem undo/redo ainda. Storage guarda scripts/módulos e
folders, não Parts físicas fora de Workspace. Rig continua asset de autoria,
não uma segunda hierarquia de Instances. Visuais/imagens/skybox/vida/Shift Lock
da missão nova ainda pendentes. Próximo: LuaRuntime + bindings + scheduler,
Script Editor/Output, teste de Play, erros, loops, módulos e Stop.
