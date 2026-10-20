# Loblox Open-Source

Validação de controles, atalhos, diagnóstico e procedimento manual em Sway:
[Entrada e validação integrada](docs/INPUT_AND_VALIDATION.md).

Editor 3D nativo para Linux, inspirado no Roblox Studio clássico. Esta é a
**fundação 0.3**: viewport 3D real, física 3D contínua, hierarquia de objetos,
painel de propriedades, ferramentas de construção e salvamento em JSON.

O projeto foi pensado para um notebook modesto (3 GB de RAM, HD mecânico,
Sway/Wayland com XWayland): interface nativa em C++, sem Electron, sem
carregamento remoto e sem assets pesados.

## Estado atual

- Viewport 3D com céu azul, iluminação direcional simples, grade e Baseplate
  grande com studs (malha única, sem um objeto por stud).
- Câmera de editor orbital e livre: `WASD` move sem RMB, `Q/E` sobe/desce,
  `Shift` acelera; RMB captura/oculta o cursor para olhar; MMB faz pan, roda
  aproxima ao foco e `F` enquadra a seleção.
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
- Gizmos Move com 3 eixos e 3 planos, Scale com faces positivas/negativas e
  preservação da face oposta, Rotate por anéis. Global/Local e snap configurável
  (Shift suspende snap durante o drag).
- As seis alças de escala ficam fora dos limites atuais e seus ícones são
  separados quando se sobrepõem na projeção; o hit test usa as mesmas posições.
- Peças novas começam ancoradas para permanecerem no lugar durante a edição;
  desative `Anchored` para simular gravidade e colisões.
- Salvar/abrir projeto JSON versionado, com validação, escrita atômica e
  carregamento transacional: um arquivo inválido não altera a cena atual.
- Exportar/importar cena `.lua` declarativa com hierarquia e propriedades. O
  parser aceita apenas `return` e tabelas de literais; arquivos nunca são
  executados e o formato JSON do projeto permanece intacto.
- Inicialização segura: sem display a aplicação sai com erro controlado; um
  projeto inválido mantém a cena padrão e mostra a falha.
- Invariantes de cena/física verificadas após carga, exclusão e fim de arraste.
- Diagnóstico estruturado em JSONL, com rotação limitada, para falhas de
  inicialização, persistência e transformação.
- Testes automatizados headless e gráficos via CTest.

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
# ou, para abrir um projeto específico:
./build/loblox --project /caminho/projeto.json
```

### Inicialização e recuperação

- `--project CAMINHO` abre explicitamente o arquivo indicado; ele nunca é
  sobrescrito automaticamente.
- Sem a flag, o caminho padrão de projeto é carregado somente se existir. Esse
  é o conceito de “último projeto válido” nesta etapa: o arquivo salvo por
  `Ctrl+S` no caminho padrão; não há histórico de projetos recentes.
- Se o arquivo estiver ausente, ilegível ou inválido, a cena padrão é mantida,
  a falha aparece na barra de status e uma linha de erro é gravada no log. O
  carregamento usa uma cena candidata e só troca o conteúdo depois da validação.
- Sem `DISPLAY`/`WAYLAND_DISPLAY`, a inicialização falha com código 1 em vez de
  continuar com uma janela inválida.

O log de diagnóstico fica em
`$XDG_STATE_HOME/loblox/loblox.jsonl` ou, sem essa variável,
`~/.local/state/loblox/loblox.jsonl`. É JSONL com timestamp UTC, nível,
subsistema, evento, correlação, arquivo, duração, resultado, erro e contexto
(contagens/formato), sem conteúdo de projeto. Cada arquivo é rotacionado ao
atingir 1 MiB, mantendo um backup `.1`.

## Controles

| Ação | Controle |
| --- | --- |
| Selecionar objeto | clique esquerdo na viewport |
| Mover / redimensionar / girar | escolha a ferramenta e arraste o eixo na viewport |
| Orbitar câmera | arrastar com o botão direito |
| Câmera livre | viewport focada: `W`, `A`, `S`, `D`; `Q` desce e `E` sobe |
| Acelerar câmera livre | `Shift` enquanto move |
| Ajustar velocidade/sensibilidade | ribbon **Model** |
| Deslocar câmera | botão do meio + arrastar |
| Zoom | roda do mouse |
| Focar seleção | `F` |
| Excluir seleção | `Delete` ou `Backspace` |
| Duplicar Part selecionada | `Ctrl+D` (cópia na mesma posição, já selecionada) |
| Salvar | `Ctrl+S` (ou botão Save) |
| Abrir | `Ctrl+O` (ou botão Open) |
| Cancelar arrasto / soltar captura | `Esc` |
| Ferramentas | `1` Select, `2` Move, `3` Scale, `4` Rotate |

A Baseplate inicial e os serviços Workspace/Lighting são protegidos contra
exclusão acidental.

## Projetos e cenas declarativas

O caminho inicial é `$XDG_DATA_HOME/loblox/projects/default.loblox.json`, ou
`~/.local/share/loblox/projects/default.loblox.json` quando `XDG_DATA_HOME` não
está definido. O botão **Save** abre um diálogo com caminho editável;
**Ctrl+S** grava direto no caminho atual. O
formato é JSON legível, com versão, ids estáveis, hierarquia e todas as
propriedades. Arquivos inválidos ou de versão desconhecida são rejeitados sem
destruir a cena atual.

No menu File, **Save Project / Open Project** continuam usando o projeto JSON
nativo. **Export Scene as Lua / Import Scene from Lua** usam o formato
declarativo `loblox-scene` v1. Ele preserva IDs, pais, objetos, Lighting e
propriedades de Part, mas não executa Lua e não é um sistema de scripts. O
runtime Luau permanece planejado para uma etapa separada.

## Estrutura

```
src/
  main.cpp
  app/            ciclo de vida da aplicação
  core/           constantes, matemática e diagnósticos JSONL
  objects/        Instance, Part, Workspace, Lighting, Baseplate, SpawnLocation
  scene/          Scene (posse, hierarquia, seleção)
  physics/        PhysicsWorld (Bullet, um corpo por Part)
  rendering/      EditorCamera e SceneRenderer
  editor/         EditorUI, Gizmo, GizmoMath, TransformSession, ToolMode
  serialization/  ProjectIO JSON e SceneLua declarativo sem runtime
tests/            suíte headless (CTest; matemática, física e round-trips)
third_party/      Dear ImGui 1.93 docking, rlImGui, nlohmann/json
prototype/        protótipo single-file preservado (não compilado)
docs/             arquitetura, progresso, decisões, testes e roadmap
```

## Testes

```bash
ctest --test-dir build --output-on-failure
```

A suíte cobre criação/remoção, hierarquia, propriedades, JSON e Lua declarativo,
validação transacional, matemática de câmera/gizmos, snap, faces de escala,
rotação renderizada, sessão de arraste, pausa física, invariantes, Anchored,
CanCollide, raycast, corpos únicos e inicialização sem display. Veja
`docs/TESTING.md` para resultados, contagens e limitações dos testes gráficos.

Teste com sanitizers (ASan/UBSan), em diretório separado:

```bash
cmake -S . -B /tmp/loblox-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build /tmp/loblox-sanitize -j2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/loblox-sanitize --output-on-failure
```

## Limitações desta etapa

- Materiais são presets visuais de cor/tonalidade, não PBR.
- Escala Global de uma caixa rotacionada é limitada à dimensão local mais
  alinhada; escala global exata exigiria shear, que o modelo `Part` não guarda.
- A substituição de arquivos usa `mkstemp`/`fsync`/`rename` e `fsync` do
  diretório em POSIX/Linux; portabilidade para sistemas não POSIX ainda não
  foi implementada.
- Não há autosave nem histórico/undo. O “último projeto” é apenas o caminho
  padrão salvo por `Ctrl+S`, não uma lista de arquivos recentes.
- A validação foi executada em Xvfb/llvmpipe; não houve teste no Sway/XWayland
  real, em GPU física ou em HD mecânico.
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
