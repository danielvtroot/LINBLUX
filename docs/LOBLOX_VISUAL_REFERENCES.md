# Studio 3 — referências e evidências visuais

## Inspeção inicial (21/10/2026)

As nove imagens originais de `referenciaspngs/` foram abertas individualmente.
Nenhuma é captura do Loblox; não há screenshot local do bug nessa coleção.

| Arquivo | Classificação | Observação utilizada |
|---|---|---|
| 1.png | Roblox Studio | Tema grafite, ribbon compacta, Toolbox esquerda, árvore à direita e Properties abaixo. |
| 2.png | Studio / R6 | Tema claro; avatar frontal, studs e sombra; painel lateral estreito nesta captura. |
| aa.png | Personagem R6 | Seis membros inteiros, cabeça arredondada com frente legível; olhos/boca proporcionais. |
| image.png | Player / HUD | Menu no canto superior esquerdo, vida superior direita; materiais e atmosfera. Marcas pretas não são UI. |
| images.png | R6 / mundo | Avatar em vegetação; referência de proporção, não exigência de sistema de terrain. |
| objetivoigualesse.png | Studio em Play | Referência principal: ribbon com ícones/rótulos, viewport ampla, Explorer acima de Properties, HUD contida na viewport. Chat/multiplayer não serão simulados. |
| q.png | Player / menu Esc | Painel escuro central, abas, conteúdo rolável, rodapé com três ações largas e Resume destacado. Círculos vermelhos não pertencem ao design. |
| roblox2.png | Player / Settings | Configurações em linhas alinhadas, fundo translúcido, conteúdo central e rodapé fixo. |
| roblox.png | Studio em Play | Mesma composição observada em objetivoigualesse.png. |

## Fontes oficiais consultadas

- [Studio interface](https://create.roblox.com/docs/studio/ui-overview): abas, docking, Output.
- [Explorer](https://create.roblox.com/docs/studio/explorer): árvore, inserção contextual, reparent, busca, duplicação.
- [Properties](https://create.roblox.com/docs/studio/properties): título da seleção, categorias, filtro e vetores.
- [Script Editor](https://create.roblox.com/docs/studio/script-editor): highlighting, navegação e edição.
- [Settings and Help](https://en.help.roblox.com/hc/en-us/articles/203314230-In-experience-Settings-and-Help): Classic, Follow, Shift Lock e contraste.
- [Skyboxes](https://create.roblox.com/docs/environment/skybox): seis faces e continuidade das bordas.
- [Textures and decals](https://create.roblox.com/docs/parts/textures-decals): superfícies locais e repetição.

Inspiração funcional e de composição; código, ícones e marca continuam Loblox.
Nenhum recurso externo foi copiado para o projeto.

## Comparação antes da implementação

Base real: HEAD f75e44d mais seis arquivos modificados localmente. Backup dos
diffs/originais realizado fora do Git. Build passou; CTest inicial 8/10:
game falhou por expectativa antiga de Esc/Back; game_ui falhou no clique de
Settings (retângulo da aba igual a zero). Capturas do binário real em
`/tmp/loblox-studio3-before-uzmLtwEM/studio.png` e fixture de produção em
`/tmp/loblox-game-ui-q39CDe/pause.png`, ambas abertas e examinadas.

| Componente | Referência observada | Loblox observado | Diferença concreta | Correção necessária |
|---|---|---|---|---|
| Ribbon | Ícones/rótulos completos, grupos compactos | Botões claros sobre tema escuro; rótulos cortados no aplicativo | Cores fixas; altura do dock insuficiente para novo padding | Tokens comuns e orçamento real de altura |
| Explorer | Nome legível, cor no ícone, seleção clara | Nome inteiro colorido; árvore durante Play esmaecida | Cor de classe usada no texto; BeginDisabled global | Texto neutro; ícone colorido; distinguir autoria inativa sem apagar árvore |
| Properties | Categorias e duas colunas; vazio limpo | Estado vazio com caminho longo cortado, métricas; três números espremidos | Dados fora de contexto; colunas sem adaptação | Vazio simples e vetores expansíveis/legíveis |
| Menu Esc | Centro do gameplay, tabs, rodapé | Painel toca topo da janela e cobre ribbon/Toolbox; metade inferior vazia | Top-left passado com pivot .5; mínimo maior que viewport; footer no fluxo | Geometria única contida na viewport atual, conteúdo rolável e rodapé fixo |
| HUD | Marca/menu compactos e vida real | Faixa longa de instruções; Shift Lock sobrepõe texto | Layout não adapta largura | Marca/menu acionável, vida e indicador sem colisão |
| R6 | Cabeça com frente plana e rosto proporcional | Cabeça achatada/facetada; rosto pequeno e visualmente irregular na fixture | Superelipsoide e normais precisam de fixture frontal; face aspect já alterado localmente | Medir imagem e validar frente/perspectiva antes de substituir malha |
| Skybox | Céu com nuvens e horizonte | Azul plano na captura; quatro arquivos carregáveis | Pode ser orientação/Enabled; Back/Down são sólidos e não configuráveis | Fixture Enabled, vistas por face e referências explícitas |

## Estado

Retomada após Nemotron: commits até 7afd682 auditados; nove arquivos reabertos.
O usuário escolheu céu procedural como modo principal. A proposta anterior
de sky1/cubemap foi substituída; os PNGs permanecem intactos, sem uso padrão.
Menus/layout/paleta do checkpoint 2adcf45 foram preservados.

Correções verificadas: gizmos reais do rig (mesmo proxy no desenho/picking,
solver bind/rest/pose e transação cancelável), Fall vazio independente, sky
procedural realmente desenhado (teste de pixels falhava antes), comandos de
rig no Explorer e realce Lua com espaços/scroll/caret alinhados.

Capturas do aplicativo principal, e não de mockups:
`/tmp/loblox-resume-app-wJgzIHbF/{studio,play,pause}.png`, 1366×768.
Foram abertas: ribbon legível; HUD com logo/menu; Esc centralizado dentro do
mundo, sem cobrir Explorer/ribbon. Fixtures do renderer/UI de produção:
`/tmp/loblox-visual-studio-pSRc15/{r6-front,script,pause-settings-1366}.png` e
`/tmp/loblox-rig-input-hNb6l6/{Move,Rotate,Scale}.png`, também examinadas.
O rosto mantém a razão da imagem original e a frente -Z. Proporções 2×1×1
da cabeça autoral foram preservadas; equivalência da silhueta clássica ainda
não foi aprovada pelo usuário. Shaders avançados e autocomplete não estão
concluídos. Não declarar equivalência integral com Roblox Studio.

Documento de trabalho: screenshots posteriores e resultados devem acompanhar
cada checkpoint. Referência visual não equivale a aprovação do usuário, e
Xvfb/llvmpipe não equivale à GPU/DPI do notebook.

Última galeria inspecionada: `build/validation-studio3/` (artefatos locais,
não versionados). Contém aplicativo principal em Studio/Play/Esc, gizmos do
rig, Home/Avatar/Script, código Lua, Animation Editor, R6 frontal/perspectiva,
HUD/Shift Lock e menu Game/Settings nas quatro resoluções de aceitação.
O teste de geometria passou 147 verificações; o de rig, 192 no Xvfb e no
Sway privado. A ribbon Avatar/Script ainda é esparsa e o painel de animação
exige scroll em notebook: estes pontos não são equivalência visual concluída.
