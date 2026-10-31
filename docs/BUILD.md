# Compilar LINBLUX no Linux

O diretório oficial é `build-linblux/`; Studio e Player são
`build-linblux/linblux` e `build-linblux/linblux_player`.
Build de desenvolvimento e pacote distribuível são artefatos diferentes.

## Dependências

C++17, CMake 3.16+, pkg-config, raylib 5.5+, Bullet, Lua 5.4, FreeType, Zlib,
OpenGL/X11 e fontes DejaVu/Liberation. FFmpeg habilita o vídeo opcional.
A base foi compilada com raylib 6.2.0 e Bullet 3.24. Os nomes dos pacotes variam
por distribuição; em Debian/Ubuntu, procure:

```text
build-essential cmake pkg-config libgl1-mesa-dev libbullet-dev liblua5.4-dev
libfreetype-dev zlib1g-dev libx11-dev fonts-dejavu-core
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Instale raylib com `raylib.pc` acessível ao pkg-config. RmlUi 6.3 é obtido pelo
CMake com commit e SHA-256 fixados; a primeira configuração precisa de rede ou
da dependência já em cache. ImGui, rlImGui, stb e JSON ficam em `third_party/`.
Zenity é usado por seletores nativos; Python 3, Pillow, Xvfb e xdotool são
necessários para as integrações automatizadas que os utilizam.

## Configuração e execução

```sh
cmake -S . -B build-linblux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linblux -j2
./build-linblux/linblux
./build-linblux/linblux_player --help
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
```

`-DLINBLUX_ENABLE_VIDEO=OFF` desativa o backend FFmpeg e usa fundo estático.
Isso muda as dependências do executável, mas não dispensa a revisão das demais
licenças. Para instalação local, use `cmake --install build-linblux --prefix CAMINHO`.
Não execute build como root: arquivos com dono diferente podem impedir incrementos.

## Diretórios existentes e pacote

`build/` e `build-no-video/` são árvores locais antigas. Não são a referência
de validação e não devem ser apagadas automaticamente; confirme processos,
configuração e conteúdo antes de qualquer limpeza. Nenhuma foi removida nesta missão.

O [empacotador Alpha](ALPHA_0_1_0.md) instala uma seleção de recursos em staging
e gera `.tar.gz` com manifesto e SHA-256. O pacote não inclui experiências pessoais,
caches ou vídeos sem redistribuição confirmada. Ele depende de bibliotecas do
sistema e não é um pacote Linux universal.
