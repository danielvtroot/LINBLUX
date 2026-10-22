# Frontend RmlUi

RmlUi 6.3, MIT, fixado em `ba95ffe8bfb6370efb2cdcca927eaad4710c5413`.
O CMake verifica SHA-256 antes de compilar. Requer FreeType de desenvolvimento
(`libfreetype-dev`) e uma fonte sans TrueType do sistema. Não usa navegador,
outra VM Lua, outra janela GLFW nem outro contexto OpenGL.

## Separação

- `src/ui/EditorActions.*`: comandos com IDs estáveis e validação C++.
- `src/ui/StudioFrontend.*`: contexto RML, adaptação dos dados, layout, eventos.
- `src/ui/RmlFrontendSupport.*`: interface de sistema RML, arte dos ícones e
  política de textura do renderer GL3 (compartilhada com a fixture gráfica).
- `assets/ui/documents/studio.rml`: shell, Toolbox, Explorer, HUD e pausa.
- `assets/ui/documents/ribbon.rml`: composição dos grupos de ferramentas.
- `assets/ui/styles/studio.rcss`: tipografia, densidade, estados e componentes.

RML é o padrão. `build/loblox --ui legacy` mantém o frontend anterior para
diagnóstico. Nunca são desenhados simultaneamente os painéis comuns antigos
e seus substitutos. Script, timeline, Rig Editor e Output conservam os controles
especializados ImGui dentro de uma região integrada com abas RML. Game Config
e o importador/biblioteca de imagens têm controles RML. A migração especializada
não é uma reescrita desses editores.

## Render e input

A cena continua na RenderTexture existente. Depois do compositor ImGui, o GL3
oficial do RmlUi desenha a apresentação e restaura seu estado gráfico. Popups e
tooltips dos componentes especializados são separados da primeira passagem e
desenhados uma única vez acima do compositor RML. PNG/JPEG
usam o decoder raylib, conversão RGBA premultiplicada e cache de texturas RML;
ícones próprios são rasterizados uma vez por nome. Originais não são alterados.
O adaptador de caminhos mantém a raiz absoluta do filesystem Linux, em vez da
semântica de raiz virtual padrão do RmlUi. Imagens têm preflight de dimensões
PNG/JPEG e tamanho de arquivo antes da decodificação.

Coordenadas do layout são lógicas; eventos e contexto RML usam a escala do
framebuffer. HUD e pausa são filhos do retângulo efetivo da viewport. O novo
modo usa um único layout de splits; não cria um DockSpace ImGui concorrente.
Divisórias e proporções ficam em `$XDG_CONFIG_HOME/loblox/ui-layout.json`.
O modo RML não grava as posições fixas no INI de docking da UI legada.
O layout geral arbitrariamente flutuante do docking antigo não foi migrado.

O backend raylib→ImGui continua normalizando as bordas de teclas e a fila de
caracteres. Um sinal explícito informa quando RML possui um campo de texto,
evitando sintetizar letras como atalhos. Eventos RML são consumidos antes do
roteador existente. Captura de câmera e drags ativos conservam seu proprietário.
Atualizações programáticas dos controles não podem emitir comandos de edição.

## Pipeline de textura

O backend GL3 do RmlUi sobe toda textura com `GL_LINEAR` e sem cadeia de mip, e
o frontend desenha quase sempre as imagens menores que a resolução nativa. Por
isso `src/ui/RmlFrontendSupport.*` aplica uma política explícita:

- ícones: a arte de 48×48 é rasterizada em 96×96 (supersample 2×) para que a
  cadeia de mip tenha detalhe real;
- logo e PNGs importados: reduzidos por média de área (caixa), preservando a
  proporção e o alfa, até 256 px de aresta;
- upload: alfa premultiplicado (o RmlUi mistura com `GL_ONE,
  GL_ONE_MINUS_SRC_ALPHA`), cadeia de mip completa, filtro trilinear e um
  pequeno bias de LOD para o traço fino dos ícones não perder contraste;
- o atlas de fontes continua no caminho original do RmlUi: o FreeType rasteriza
  no tamanho exato do CSS com antialiasing de 8 bits, e ele não pode ser
  desfocado por mipmap.

A cena 3D usa a mesma regra em `src/game/ImageAssets.cpp`: as texturas de decal
e de superfície são carregadas premultiplicadas, com cadeia de mip e filtro
trilinear (e anisotropia quando `GL_EXT_texture_filter_anisotropic` existe), e
os quads correspondentes misturam com alfa premultiplicado. Sem isso, uma
imagem preto e branco minificada virava ruído serrilhado e o RGB guardado sob
alfa zero (magenta na logo) vazava para a arte visível.

A fixture `tests/graphics_quality_main.cpp` (`ctest -R loblox_graphics_quality`)
reproduz logo, ícones, texto, transparência, gradiente, cantos arredondados,
decal, textura repetida em superfície inclinada e rosto R6, compara a
amostra com a referência filtrada e mede a energia de borda da textura 3D antes
e depois da correção.

## Execução e diagnóstico

```sh
cmake -S . -B build
cmake --build build --target loblox -j2
./build/loblox
ctest --test-dir build --output-on-failure
```

`LOBLOX_UI_TIMINGS=1` mede apresentação, cena, atualização, render e frame, sem
HUD de métricas. `LOBLOX_TRACE_UI=1` imprime geometria inicial do documento.
O teste `loblox_rml_ui` exercita os documentos/renderer reais com X11 e gera
capturas em um diretório privado indicado no resultado. Nenhum projeto pessoal
é usado. Sway e DPI fracionário precisam de validação própria desta migração.

O importador reutiliza o formato de imagens existente, baseado em `assets/`
relativo ao diretório de trabalho. Ele não implementa um novo pacote de projeto
nem múltiplos decals por Part; as representações antigas não são descartadas.
