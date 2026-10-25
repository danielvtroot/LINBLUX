# LINBLUX

<img src="midia/logo.png" width="112" alt="LINBLUX"/>

Plataforma independente e open-source para criar, editar e executar experiências
3D no Linux. C++17, raylib, Bullet, Lua 5.4 e RmlUi. Interface em português brasileiro.

Repositório oficial: **https://github.com/danielvtroot/LINBLUX** · branch `main`.

## O que funciona

- Studio com cena, hierarquia, propriedades, gizmos, câmera, scripts e animações.
- Início com vídeo em toda a janela, overlay e controles translúcidos sobrepostos.
- Experiências locais, ícone e banner, importação/exportação `.lo`, salvamento atômico.
- Arquivamento e exclusão permanente com confirmação e proteção da biblioteca.
- Mundo e personagem padrão editáveis e versionados, copiados para novas experiências.
- Execução isolada da cena autoral e Player independente do Studio.

O personagem atual usa seis corpos/juntas com configuração própria. Importação de
malhas/esqueletos arbitrários, terreno avançado e multiplayer ainda não estão prontos.
LINBLUX Server permanece no roadmap; não há serviço multiplayer de produção.

## Compilar no Linux

Dependências Debian/Ubuntu (o nome pode variar por distribuição):

```sh
sudo apt-get install build-essential cmake pkg-config libgl1-mesa-dev libbullet-dev \
  liblua5.4-dev libfreetype-dev zlib1g-dev libavformat-dev libavcodec-dev \
  libavutil-dev libswscale-dev fonts-dejavu-core python3 python3-pil zenity \
  xvfb xdotool scrot libx11-dev
```

Instale também **raylib 5.5 ou superior** com arquivo `raylib.pc` acessível ao
pkg-config. O ambiente de validação usa raylib 6.2.0. ImGui, rlImGui, stb e JSON
estão em `third_party`; RmlUi 6.3 é obtido pelo CMake de commit e SHA-256 fixados.
A primeira configuração precisa de rede ou do cache/checkout dessa dependência.

```sh
git clone https://github.com/danielvtroot/LINBLUX.git
cd LINBLUX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/linblux
ctest --test-dir build --output-on-failure
```

Neste checkout, use `build-linblux` para compilar e executar: o diretório `build`
preexistente conserva um cache com o caminho antigo e não foi descartado.

Sem FFmpeg, o Início usa fundo grafite estático e avisa na configuração.
`-DLINBLUX_ENABLE_VIDEO=OFF` também permite validar essa configuração.
Os testes gráficos usam Xvfb/Mesa quando disponíveis. Compilar não comprova a
aparência ou a interação: veja [TESTING](docs/TESTING.md) e os relatórios reais.

## Usar

**Criar experiência** copia o mundo e personagem padrão atuais. **Abrir experiência**
aceita pasta, manifesto, `.lo` ou JSON legado. Projetos existentes são preservados.
WASD com botão direito navega no editor; Q desce, E sobe; F enquadra. F5 executa;
Shift+F5 encerra. O Player usa WASD, espaço para pular e Esc para pausar.

```sh
./build/linblux --project /caminho/experiencia/experience.json
./build/linblux_player --project /caminho/experiencia
./build/linblux_player --validate /caminho/cena.loblox.json
cmake --install build --prefix "$HOME/.local"
```

## Documentação

- [Guia do Studio](docs/GUIA_STUDIO.md) · [experiências](docs/EXPERIENCES.md)
- [Mundo e personagem padrão](docs/PADROES_DA_ENGINE.md) · [compatibilidade](docs/COMPATIBILIDADE.md)
- [Arquitetura](docs/ARCHITECTURE.md) · [desenvolvimento e contribuição](CONTRIBUTING.md)
- [Testes](docs/TESTING.md) · [vídeo e desempenho](docs/VIDEO.md)
- [Backup e restauração](docs/BACKUP_E_RESTAURACAO.md) · [roadmap](docs/ROADMAP.md)
- [Auditoria de recursos e licenças](docs/ASSETS_E_LICENCAS.md)
- [Relatórios da migração](docs/relatorios/) · [registros históricos](docs/historico/)

Código próprio sob [MIT](LICENSE). Créditos e licenças de terceiros são
preservados em `third_party` e documentados na auditoria. A designação LINBLUX
não altera os direitos de seus respectivos autores.
