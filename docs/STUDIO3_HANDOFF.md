# Studio 3 — recuperação em andamento

Base real: f75e44d, branch fix/unified-3d-gizmos, seis arquivos rastreados
modificados antes da intervenção. Patch e originais preservados em backup
externo `studio3-Li2jVGjM`. PNGs, assets e referências permanecem originais.

Baseline: build OK; CTest 8/10. Game esperava Esc→Back; Game UI não conseguia
acionar Settings. Captura confirmou menu sobre ribbon/Toolbox. Nove imagens
locais examinadas; matriz/fontes em LOBLOX_VISUAL_REFERENCES.md.

Corrigido: menu usa centro e retângulo atual da imagem da viewport, com bounds
seguros, dim restrito ao mundo, tabs reais, scroll interno e footer fixo.
124 checks novos passaram em quatro resoluções (1280,1366,1600,1920).
Game UI passou incluindo Settings/input/respawn/Stop. Capturas inspecionadas:
`/tmp/loblox-visual-studio-8SYN2F/pause-settings-1366.png` e demais resoluções.

Tema compartilhado em StudioTheme.h, dark padrão/light opcional; substitui
cores claras fixas da ribbon. Explorer mantém cor no ícone e texto neutro.
Rótulos de atributos antes do processamento do item roubavam clique/drag da
Part; removidos desse ponto. Corrigido cast inválido double*→float* no slider
de transparência de decal. Regressão completa está sendo executada.

Próximos passos: conferir nova ribbon/capturas, corrigir realce Lua (scroll
em pixels, wrap divergente, whitespace), cabeça/rosto, skybox (quads eram
desenhados antes de BeginMode3D), cache/lifetime/paths, Decal/Texture reais.
Ainda não há aprovação visual do usuário nem validação na GPU do notebook.

Comandos: `cmake -E env TMPDIR=$PWD/build/compiler-tmp cmake --build build -j2`;
`ctest --test-dir build --output-on-failure -j2`.
