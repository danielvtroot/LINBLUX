# Loblox Open-Source

Editor 3D nativo para Linux, inspirado no Roblox Studio clássico. Esta é a
**fundação 0.3**: viewport 3D real, física 3D contínua, hierarquia de objetos,
painel de propriedades, ferramentas de construção e salvamento em JSON.

O projeto foi pensado para um notebook modesto (3 GB de RAM, HD mecânico,
Sway/Wayland com XWayland): interface nativa em C++, sem Electron, sem
carregamento remoto e sem assets pesados.

## Estado atual

- Viewport 3D com céu azul, iluminação direcional simples, grade e Baseplate
  grande com studs (malha única, sem um objeto por stud).
- Câmera orbital: orbitar, deslocar, zoom e focar a seleção.
- Física Bullet 3D rodando continuamente (sem botão Play/Stop):
  - peças ancoradas ficam estáticas e servem de apoio;
  - peças soltas caem, colidem e repousam sobre a Baseplate/outras peças;
  - `Anchored` e `CanCollide` afetam o mundo físico imediatamente;
  - exclusão remove o corpo físico junto com o objeto.
- Barra de ferramentas agrupada: Select, Move, Scale, Rotate, Part, Sphere,
  Spawn, Color, Material, Anchor, Explorer, Properties, Save e Open.
- Toolbox com Block, Sphere e SpawnLocation.
- Explorer com árvore real da cena (Workspace, Baseplate, SpawnLocation,
  Lighting e peças criadas) e seleção sincronizada.
- Properties com Name, ClassName, Position, Size, Rotation, Color, Material,
  Anchored e CanCollide; alterações aplicam no objeto real.
- Gizmos de mover, redimensionar e girar nos eixos X/Y/Z.
- Peças novas começam ancoradas para permanecerem no lugar durante a edição;
  desative `Anchored` para simular gravidade e colisões.
- Salvar/abrir projeto JSON versionado, com validação, escrita atômica e
  tratamento de erros.
- Testes automatizados headless via CTest.

Não há botão Play, Test ou Stop nesta etapa, nem personagem R6, scripts,
terreno ou multiplayer.

## Requisitos

- Linux com X11 ou Wayland via XWayland (raylib usa X11).
- Compilador C++17, CMake 3.16 ou maior, `pkg-config`.
- raylib **5.5 ou mais recente** (testado com 6.2).
- Bullet Physics 3.24 (`libbullet-dev`).
- OpenGL e X11.

Debian/Ubuntu/Loc-OS (dependências básicas):

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libbullet-dev \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev \
  libxinerama-dev
```

Se o pacote `libraylib-dev` da sua distribuição não existir ou for antigo,
compile a raylib 6.2 e instale:

```bash
git clone --depth 1 --branch 6.2 https://github.com/raysan5/raylib.git /tmp/raylib
cmake -S /tmp/raylib -B /tmp/raylib/build -DBUILD_EXAMPLES=OFF \
  -DCMAKE_BUILD_TYPE=Release
sudo cmake --install /tmp/raylib/build
sudo ldconfig
pkg-config --modversion raylib   # deve mostrar 6.2.0
```

Dear ImGui (docking) e nlohmann/json já estão vendorizados em `third_party/`;
não é necessário baixar nada durante o build ou a execução.

## Compilar e executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/loblox
```

## Controles

| Ação | Controle |
| --- | --- |
| Selecionar objeto | clique esquerdo na viewport |
| Mover / redimensionar / girar | escolha a ferramenta e arraste o eixo na viewport |
| Orbitar câmera | arrastar com o botão direito |
| Câmera livre | segurar botão direito + `W`, `A`, `S`, `D`; `Q` desce e `E` sobe |
| Acelerar câmera livre | `Alt` enquanto move |
| Deslocar câmera | botão do meio (ou Shift + botão direito) |
| Zoom | roda do mouse |
| Focar seleção | `F` |
| Excluir seleção | `Delete` ou `Backspace` |
| Salvar | `Ctrl+S` (ou botão Save) |
| Abrir | `Ctrl+O` (ou botão Open) |
| Cancelar arrasto | `Esc` |
| Ferramentas | `1` Select, `2` Move, `3` Scale, `4` Rotate |

A Baseplate inicial e os serviços Workspace/Lighting são protegidos contra
exclusão acidental.

## Projetos

O caminho padrão é `projects/default.loblox.json`. O botão **Save** abre um
diálogo com caminho editável; **Ctrl+S** grava direto no caminho atual. O
formato é JSON legível, com versão, ids estáveis, hierarquia e todas as
propriedades. Arquivos inválidos ou de versão desconhecida são rejeitados sem
destruir a cena atual.

## Estrutura

```
src/
  main.cpp
  app/            ciclo de vida da aplicação
  core/           constantes e utilidades de matemática
  objects/        Instance, Part, Workspace, Lighting, Baseplate, SpawnLocation
  scene/          Scene (posse, hierarquia, seleção)
  physics/        PhysicsWorld (Bullet, um corpo por Part)
  rendering/      EditorCamera e SceneRenderer
  editor/         EditorUI, Gizmo, ToolMode
  serialization/  ProjectIO (JSON versionado e escrita atômica)
tests/            suíte headless (CTest)
third_party/      Dear ImGui 1.93 docking, rlImGui, nlohmann/json
prototype/        protótipo single-file preservado (não compilado)
docs/             arquitetura, progresso, decisões, testes e roadmap
```

## Testes

```bash
ctest --test-dir build --output-on-failure
```

A suíte cobre criação/remoção, hierarquia, propriedades, serialização,
validação de projetos, alternância Anchored, CanCollide, sincronização de
transformações, raycast, redimensionamento sem recriar corpos e limpeza de
corpos. Veja `docs/TESTING.md` para os resultados e para o smoke test gráfico.

## Limitações desta etapa

- Materiais são presets visuais de cor/tonalidade, não PBR.
- Seleção única (sem multi-seleção), sem undo/redo e sem duplicar.
- Sem personagem, animações, scripts ou modo de jogo.
- A interface usa X11/XWayland; não há backend Wayland nativo.
- Sem sombras dinâmicas ou iluminação global (decisão de desempenho).
- A FPS depende da GPU; em renderização por software (Xvfb/llvmpipe) o valor
  medido foi baixo, como documentado em `docs/TESTING.md`.

## Documentação

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/PROGRESS.md`](docs/PROGRESS.md)
- [`docs/DECISIONS.md`](docs/DECISIONS.md)
- [`docs/TESTING.md`](docs/TESTING.md)
- [`docs/ROADMAP.md`](docs/ROADMAP.md)

## Licença

MIT — veja `LICENSE`.
