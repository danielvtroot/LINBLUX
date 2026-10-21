# Studio 3 — recuperação em andamento

## Retomada após Nemotron

HEAD de entrada 7afd682; todos os hashes relatados foram encontrados. Backup
do worktree misto em `resume-TbA2zhIY`, fora do repositório. Preservados céu
procedural, Lighting, câmera com pitch, coyote/buffer, Lua e física. As nove
referências foram reabertas. Baseline: build principal OK, CTest 9/12;
falhas: teste antigo esperava script automático e fazia acesso fora da lista;
export Lua rejeitava campos novos de Lighting; fixture Game UI usava posições
antigas após inserção de controles no Rig Editor.

Causas de rig confirmadas: enum do combo incorreto; proxy desenhado diferente
do manipulado; rest pose aplicada duas vezes; childAnchor sem inversa; escala
duplicava ajuste de attachments; cancel usava índice errado/dados sem snapshot.
Implementação usa tool_ como fonte única, jointProxy_ para desenho/picking,
transação com snapshot, solver da cadeia bind/rest/pose e resizeBody uma vez.
Scale é edição de proporções (Edit Rig); Pose/Animation usam Move/Rotate.
Regressão real por xdotool: 14 falhas antes; depois passou, incluindo XYZ,
Esc/restauração exata, câmera parada, commits e save/load/Play.
Capturas examinadas: `/tmp/loblox-rig-input-B9oTxH/{Move,Rotate,Scale}.png`.
Suíte focada rig/game_ui/studio/tests passou 4/4. Principal recompilado.
Ainda executar Debug, sanitizers e validar GUI completa atual.

Pendente na retomada: completar Fall independente (mudança local parcial
ainda reutiliza Jump como fallback), reparar rename/duplicate de rigs e delete
de animações atribuídas; conferir shader procedural (capturas mostram azul
plano; Material{0} e matrizes do fullscreen exigem investigação), face frontal.
Classes Decal/Texture em Instance.h são rascunho local preexistente sem ligação
com Scene/IO; preservadas, não anunciadas como recurso disponível.

Checkpoint rig: 25e8704. Fall agora tem asset novo r6-fall vazio (sem keys),
ID persistido separado, SetAnimation("Fall") e fallback neutro exclusivo.
Projetos existentes não recebem script automático; scripts customizados são
preservados. Exclusão de clips atribuídos informa erro e pede reassociação
no Game Config, evitando referência quebrada e save inválido. Teste físico
com subida/descida verificou transição e pose neutra, além de persistência e
proteção de exclusão. Studio/Game headless 2/2 passaram nesta revisão.

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
