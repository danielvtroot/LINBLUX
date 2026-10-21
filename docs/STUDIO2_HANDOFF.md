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
Próximo: testes de hierarquia/round-trip/Bullet e implementação de Folder/Model.
Ainda não implementadas nesta missão: fases A–I; não confundir baseline com
aceitação do Studio 2. Ver também ENGINEERING_HANDOFF.md (R6 anterior).
