# LINBLUX — manual técnico oficial

LINBLUX é uma engine de criação 3D local para Linux, composta por um **Studio**
(editor de mundos) e um **Player** (execução de experiências). O projeto está em
**Alpha 0.1.0**: experimental, sem distribuição binária pública aprovada, e
tudo documentado aqui é verificável no código-fonte. Visão pública e comandos
rápidos: [README.md](README.md).

Este manual é a fonte central de documentação do projeto. Convenções:

- **Fato** é o que o código faz hoje (com referência de arquivo quando útil).
- **Plano** é objetivo futuro, sem promessa de data nem de entrega.
- Recursos marcados como *em desenvolvimento* fazem parte de trabalho ativo
  (WIP ainda não commitado) e podem mudar.
- Caminhos relativos partem da raiz do repositório.

## Índice

1. [Visão geral](#1-visão-geral)
2. [Arquitetura](#2-arquitetura)
3. [Estrutura de diretórios](#3-estrutura-de-diretórios)
4. [Compilação](#4-compilação)
5. [Execução](#5-execução)
6. [Interface do Studio](#6-interface-do-studio)
7. [Mundo 3D](#7-mundo-3d)
8. [Materiais e gráficos](#8-materiais-e-gráficos)
9. [Física](#9-física)
10. [Lua](#10-lua)
11. [Inventário e armazenamento](#11-inventário-e-armazenamento)
12. [Player](#12-player)
13. [Áudio e mídia](#13-áudio-e-mídia)
14. [Persistência](#14-persistência)
15. [Interface e renderização de UI](#15-interface-e-renderização-de-ui)
16. [ATLAS](#16-atlas)
17. [TURBO e testes](#17-turbo-e-testes)
18. [Desenvolvimento](#18-desenvolvimento)
19. [Instruções para agentes de IA](#19-instruções-para-agentes-de-ia)
20. [Backup e segurança](#20-backup-e-segurança)
21. [Limitações conhecidas](#21-limitações-conhecidas)
22. [Roadmap](#22-roadmap)
23. [Histórico resumido](#23-histórico-resumido)
24. [Licenças e créditos](#24-licenças-e-créditos)

---

## 1. Visão geral

LINBLUX reúne autoria e execução em dois executáveis independentes:

| Executável | Papel |
| --- | --- |
| `build-linblux/linblux` | Studio: Início, biblioteca de experiências, editor 3D |
| `build-linblux/linblux_player` | Player: execução de experiências, sem vínculo com a UI do Studio |

A base técnica: C++17, CMake, raylib/OpenGL, Bullet Physics, Lua 5.4, RmlUi,
Dear ImGui, stb e nlohmann/json. A interface oficial é em **pt-BR**.

Estado da Alpha, área por área:

- **Implementado e validado**: edição local, Player, persistência
  transacional, física Bullet, scripting Lua com cotas, recursos de imagem e
  áudio locais, importação/exportação `.lo`, padrões da engine.
- **Em desenvolvimento (WIP)**: superfície quadriculada de construção, tema
  monocromático, ícones Lucide, HUD de vida em barra, ajuste fino de materiais
  e qualidade gráfica; desempenho em GPU física real ainda não medido.
- **Não existe nesta Alpha**: multiplayer, contas, publicação online, malhas
  arbitrárias, terreno avançado, áudio espacial, distribuição binária pública.

A proposta é manter uma base local concreta, compreensível e modificável,
evoluindo por incrementos verificados em vez de promessas adiantadas.

## 2. Arquitetura

### 2.1 Targets CMake (`CMakeLists.txt`)

| Target | Responsabilidade |
| --- | --- |
| `linblux_engine` | objetos, cena, física Bullet, Lua, rigs/animação, persistência, experiências — camada sem interface |
| `linblux_editing` | gizmos, histórico, câmera do editor; sem posse da cena/física/Lua do runtime |
| `linblux_renderer` | `SceneRenderer`, `ModernMaterials`, sombras direcionais, `SceneTarget`, `PlayerMenu`, `ExperienceOverlay`, `LuaUiRenderer` |
| `linblux_presentation` | catálogo de localização (`src/ui/Localization.cpp`) |
| `linblux_media` | `VideoBackground` do Início (FFmpeg opcional) |
| `linblux_studio` | frontends, painéis e ações do editor (`linblux_editor` é alias) |
| `linblux_imgui` | Dear ImGui + rlImGui vendados em `third_party/` |
| `linblux_rml_gl3` | backend GL3 do RmlUi, desenhado em layer framebuffer próprio |
| `linblux_core` | fachada INTERFACE transitória de compatibilidade de testes/embutidos |
| `linblux` / `linblux_player` | executáveis de Studio / Player |

O Player **não linka** o Studio: sem ImGui, sem RmlUi e sem símbolos do editor
(`src/player/main.cpp`). O namespace C++ legado `loblox` e identificadores de
formato antigos são preservados por compatibilidade (§14.3).

### 2.2 Invariantes estruturais (contratos por módulo)

- `Scene` é dona da árvore de objetos e dos IDs persistentes; não é copiável;
  troca de conteúdo só depois de validar (`replaceContentsFrom`). Listeners de
  remoção são contratos com a física. Seleção é estado de editor e não é
  serializada.
- `ProjectIO` valida antes de alterar e grava com arquivo temporário, `fsync`,
  `rename` e `fsync` do diretório (`serialization/ProjectIO.h`). Carregamento
  inválido não substitui a cena atual.
- `RuntimeSession` recebe uma **cópia serializada do mundo autoral** e é dona
  da cena, física e controle de execução. Lua roda somente nessa cópia.
  Autoria nunca é alterada por F5/Stop (`src/game/Runtime.cpp`).
- `ExperienceManager` gerencia manifestos, biblioteca e recursos da
  experiência; importação de pacote cria cópia local; carregamento é
  transacional e falha não altera a cena nem a experiência.
- `PhysicsWorld` mantém **um corpo Bullet por Part**, passo fixo 1/60 e física
  autoritativa do personagem (`src/physics/PhysicsWorld.cpp`); consultas de
  overlap são probes transitórios com cota por quadro.
- `LuaRuntime`: sandbox de contenção (não de segurança), cotas de memória/
  instruções/tarefas, API `linblux` v1 (§10); execução antes da sincronização
  do Bullet, nunca dentro do passo físico.
- `linblux_player`: executável somente leitura; importa `.lo` para cópia local;
  sem link com Studio/ImGui/RmlUi.
- Renderização é cliente da `Scene`; nada de UI dentro do render. `FrameState`
  restaura o estado GL após os passes.
- `Media` decodifica vídeo fora da thread principal e nunca derruba o Studio;
  fallback estático quando FFmpeg não está disponível.

O renderizador recebe `Camera3D`; adaptadores de câmera do editor ficam
na camada de edição. Nenhum serviço de rede é iniciado. LINBLUX Server e
transporte multiplayer não estão implementados. O build oficial é
`build-linblux/`; instaladores usam `share/linblux/defaults` como fábrica
somente leitura (§14.2).

`Scene::parts()` mantém todos os dados autorais, inclusive armazenamento;
`worldParts()` usa um índice da árvore Workspace reconstruído sob demanda após
mudanças estruturais. Renderização, Bullet, navegação, NPCs, spawn, picking e
consultas de gameplay usam essa classificação comum. Hierarquia não é
percorrida por `Part` a cada frame. Reparenting notifica a física para
remover/restaurar corpos; edição de uma Part armazenada não cria corpo via
`applyChanges`.

## 3. Estrutura de diretórios

```text
CMakeLists.txt          build oficial em build-linblux/
README.md / LINBLUX.md  documentação consolidada do projeto
AGENTS.md               stub local para agentes (não rastreado: `.gitignore`),
LICENSE                 MIT
assets/
  builtin/              recursos embutidos do player
  defaults/             padrão de fábrica (index.json + factory/*.loblox.json)
  examples/
    lua/                ~32 exemplos Lua executados pelos testes
    rifledemo/          experiência de demonstração (world.loblox.json + Lua)
  templates/            empty, baseplate, classic, obby
  ui/                   documents *.rml, styles *.rcss, theme.json, icons/, locales/
docs/prints/            capturas públicas do Studio (galeria do README)
docs/videos/            vídeos curtos da galeria
midia/                  logo.png e careditfullhd.mp4 (mídia mantida pelo autor)
packaging/              .desktop, MIME, template do serviço systemd de backup
projects/               default.loblox.json (projeto legado)
prototype/              diretório reservado, vazio hoje
scripts/                generate-theme.py, checkpoint.py, sync-safe.sh, fixtures
src/                    código da engine (ver 3.1)
tests/                  executáveis de teste e suítes Python (atlas/backup/turbo)
third_party/            imgui, rlimgui, stb, nlohmann (vendados)
tools/                  atlas, turbo, backup, icons, examples, package_linux.py,
                        check_linux_package.py, update_readme_media.py
build-linblux/          árvore de build oficial (não versionada)
.atlas/ .turbo/         caches/estado local das ferramentas (não versionados)
```

### 3.1 `src/`

| Diretório | Conteúdo principal |
| --- | --- |
| `app` | ciclo de vida do Studio (`Application`): initialize → run → shutdown |
| `core` | utilidades determinísticas (quaternions, `diag`, validação de imagem) |
| `editor` | gizmo, transformações, histórico, câmera do editor |
| `experiences` | `ExperienceManager` |
| `game` | `RuntimeSession`, Spawn, Rig, Npc, Navigation, Inventory, ItemActions, Interaction, Audio, Ui, ProgressStore, assets |
| `media` | `VideoBackground` (Início) |
| `objects` | Instance/hierarquia, Part, Material3D, Workspace, Lighting, Baseplate, SpawnLocation |
| `physics` | `PhysicsWorld` (Bullet) |
| `player` | `main.cpp` do Player |
| `rendering` | SceneRenderer, ModernMaterials, sombras, overlay do Player |
| `scene` | `Scene`: árvore, IDs, classificação mundo/armazenamento |
| `scripting` | `LuaRuntime` (sandbox, cotas, API) |
| `serialization` | `ProjectIO` (cena/pacote), `SceneLua` (import declarativa) |
| `ui` | frontends (RmlUi/ImGui), ações, temas gerados, localização |

## 4. Compilação

O build oficial de desenvolvimento é `build-linblux/`. Builds antigos
(`build/`, `build-no-video/`) existem em alguns checkpoints e não são
referência de validação.

### 4.1 Dependências

C++17, CMake 3.16+, pkg-config, raylib 5.5+ (base validada com 6.2.0), Bullet
(`libbullet-dev`), Lua 5.4, FreeType, Zlib, OpenGL/X11 e fontes
DejaVu/Liberation. RmlUi 6.3 é obtido por FetchContent de commit e SHA-256
fixados: a **primeira configuração precisa de rede** ou de cache da
dependência. Vendados localmente: ImGui, rlImGui, stb e nlohmann/json em
`third_party/`.

Pacotes Debian/Ubuntu típicos:

```text
build-essential cmake pkg-config libgl1-mesa-dev libbullet-dev liblua5.4-dev
libfreetype-dev zlib1g-dev libx11-dev fonts-dejavu-core
libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

FFmpeg é ligado dinamicamente e habilita o vídeo do Início e os metadados de
áudio; sem ele o build usa fundo estático (`LINBLUX_ENABLE_VIDEO`). Zenity
alimenta os seletores nativos de arquivo. Python 3, Pillow, Xvfb e xdotool são
requisitos das integrações gráficas e do empacotamento, não do build.

### 4.2 Comandos

```sh
git clone https://github.com/danielvtroot/LINBLUX.git
cd LINBLUX
cmake -S . -B build-linblux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linblux -j2
./build-linblux/linblux
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
```

Opções e notas reais:

- `-DLINBLUX_ENABLE_VIDEO=OFF` desativa o backend FFmpeg (fundo estático) e as
  dependências de vídeo; não dispensa a revisão das demais licenças.
- `-DLINBLUX_RMLUI_MSAA_SAMPLES=0|2|4` controla o MSAA da camada de UI
  (padrão 4; 0 desabilita).
- `-DLINBLUX_INSTALL_BACKGROUND_VIDEO=ON` instala o vídeo do Início na
  instalação local; ele fica **fora do pacote distribuível**.
- Não execute o build como root.
- Instalação local: `cmake --install build-linblux --prefix CAMINHO`.

Empacotamento (candidato interno, exige build Release):

```sh
python3 tools/package_linux.py --build-dir build-linblux
python3 tools/check_linux_package.py \
  .turbo/dist/linblux-alpha-0.1.0-candidate-REVISAO-linux-x86_64.tar.gz
```

O `.tar.gz` (nomeado `linblux-alpha-0.1.0-candidate-<revisão>-linux-<arquitetura>
.tar.gz`) e seu `.sha256` saem em `.turbo/dist/`; use o nome real do artefato
gerado. O empacotador exige
`CMAKE_BUILD_TYPE:STRING=Release`, remove qualquer `*.mp4`, instala os avisos
de licença e não copia experiências pessoais, caches nem mídia sem direitos
documentados. `check_linux_package.py` extrai, confere SHA-256 do manifesto,
garante ausência de MP4, roda `--validate`/`--frames 8 --check-scripts` no
Player e abre janela do Studio com xdotool em ambiente isolado.

## 5. Execução

### 5.1 Studio

`build-linblux/linblux [opções]`:

- `--ui rml|legacy` — frontend RmlUi (oficial) ou ImGui legado/diagnóstico.
  Opção inválida encerra com exit 2.
- `--project CAMINHO` — abre JSON legado, pasta de experiência ou `.lo`.
- `-h/--help` — mostra o uso.

Sem argumentos, abre o **Início** com a biblioteca local. Falha de
carregamento preserva a cena padrão (load transacional). No frontend RmlUi o
Início inicia sem objetos de cena carregados até uma experiência ser aberta.

### 5.2 Player

`build-linblux/linblux_player [opções]`:

- `--project CAMINHO` — pasta de experiência (`experience.json`), `.lo`
  (importado para cópia local com nova identidade) ou JSON avulso. Obrigatório.
- `--validate CAMINHO` — roda ~2 s sem janela e imprime
  `experiência validada` / `validação falhou` + contagem de erros Lua.
- `--check-scripts` — o término reporta erro se houve falha de script.
- `--frames N` — limita o número de quadros (1..100000), para smoke tests.
- `--quality legacy|low|medium|high` — qualidade de renderização da partida.
- `--help` — uso. Opção inválida → exit 2.

Exit codes de referência: `2` argumento inválido, `1` falha (inclusive erros
Lua com `--check-scripts`), `0` sucesso.

### 5.3 Do Studio para o Player

F5 executa a experiência atual no runtime; **Esc abre o menu sem interromper a
simulação**; Shift+F5 volta ao Studio. No editor, botão direito + WASD move a
câmera, Q/E desce/sobe e F enquadra a seleção. No Player, WASD move, espaço
pula e botão direito orbita a câmera; itens ver §12.

### 5.4 Exemplo RifleDemo (`assets/examples/rifledemo`)

Demonstração jogável: abra `world.loblox.json` no Studio e clique Executar, ou
`./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json`.

- **Jogar**: aproxime-se do item do chão, segure **E** (0,6 s) para coletar,
  pressione **1** para equipar, **clique** dispara, **R** recarrega e
  **Delete** solta (o item volta como instância com a munição atual).
  **Esc** abre o menu protegido. Aliados do cenário: três alvos com
  rig/vida/respawn, um guia animado em patrulha e uma placa com a logo oficial.
- **Estrutura**: o item é um Modelo de Parts; `BocaDoCano` é um Attachment da
  Part `Cano`. `Item.lua` implementa ações, recuo, raycast/dano e UI pelas
  APIs públicas (sem arma fixa na engine, §11.2). `Demo.lua` monta o cenário e
  é dono dos efeitos temporários — sua limpeza continua mesmo se o Script do
  item for removido.
- **Sons**: cinco arquivos de síntese originais (licença MIT), gerados pela
  própria ferramenta `build_rifledemo.py`; IDs de biblioteca preservados na
  importação/exportação, streaming raylib com `owner` do item; sem
  dispositivo de áudio a demonstração fica silenciosa explicitamente.
- **Limites da demo**: sem rede/multiplayer nem balística simulada — dano é
  por raycast e o traçante visual é uma Part física sem colisão. Mira
  horizontal; não é um sistema FPS completo.
- **Regenerar/conferir** (`tools/examples/build_rifledemo.py`):
  `python3 tools/examples/build_rifledemo.py` (embute as fontes no mundo) e
  `--check`. Testes: `linblux_item_actions`, `linblux_rifledemo`,
  `linblux_rifledemo_player` (gráfico via TURBO, §17).

## 6. Interface do Studio

### 6.1 Início

Documento RmlUi `assets/ui/documents/home.rml`: biblioteca de experiências
(recentes), Criar experiência, Abrir experiência, arquivadas,
configurações (biblioteca e padrões da engine), cartão permanente **Mundo
Padrão da Engine → Editar padrão** e overlay de carregamento com
cancelamento. Vídeo de fundo assíncrono quando FFmpeg está presente, senão
fundo grafite estático.

### 6.2 Editor

`studio.rml` e `ribbon.rml` (`assets/ui/documents/`):

- **Ribbon** com abas Início (ferramentas), Modelo, Personagem, Script,
  Visualizar e Jogo: inserir Part/Esfera/SpawnLocation; cor/material/ancorar;
  qualidade 3D (Compatível/Baixo/Médio/**Alto — PBR leve**), AA
  (Auto/off/FXAA/MSAA4), sombras (Auto/off/Economia/Suave/Alto), Arestas F8,
  mostrar colisores (ciano: Bullet; magenta: divergência), tamanho das alças;
  executar/parar/configurar/salvar no Jogo.
- **Toolbox** com filtro Geometria/Personagem/Lógica e busca.
- **Explorer** com busca por nome/classe, botão **+** que localiza ou cria o
  serviço escolhido (também a pasta `ServerStorage/Itens`) sem duplicar, e o
  comando **Normalizar nomes das peças** (somente prévia).
- **Properties** com filtro e edição de propriedades gerais, corpo rígido,
  materiais, pivô de Modelo, protótipo de item, superfícies (Decal/Texture) e
  `Conectar a` entre Attachments.
- **Abas inferiores**: Editor de scripts, Animação, Editor de personagem,
  Configurações do jogo, Saída, Áudio, Recursos.

Serviços demonstrados na árvore, nesta ordem: Workspace/Mundo, Lighting,
ServerScriptService, ServerStorage, ReplicatedStorage. Ao abrir cenas antigas,
serviços ausentes e `ServerStorage/Itens` são instanciados em memória; nada é
gravado sem Salvar.

### 6.3 Atalhos (fonte única `src/editor/EditorUI.cpp`)

| Tecla | Ação |
| --- | --- |
| `1`–`4` | Selecionar / Mover / Escalar / Girar |
| `F5` / `Shift+F5` | Executar / Parar |
| `Esc` | Cancelar gesto; abre menu durante o Play |
| `F` | Enquadrar seleção |
| `F2` | Renomear (Explorer) |
| `F8` / `Shift+F8` / `Ctrl+F8` | Arestas / caixas AABB / normais |
| `F10` / `F11` | Maximizar jogo / tela cheia |
| `Ctrl+S` / `Ctrl+O` | Salvar / abrir |
| `Ctrl+D` / `Ctrl+C` / `Ctrl+V` | Duplicar / copiar / colar |
| `Ctrl+Z` / `Ctrl+Y` ou `Ctrl+Shift+Z` | Desfazer / refazer |
| `Ctrl+L` | Alternar espaço local/mundo do gizmo |
| `Ctrl+G` / `Ctrl+Shift+G` | Agrupar / desagrupar como Modelo |
| `Space` | Salto (durante o Play) |
| WASD + Q/E | Navegação de câmera |
| Delete/Backspace | Remover objeto |

Na árvore Explorer: clique seleciona, Ctrl multisseleção, F2 renomeia, arrastar
reparente com indicador de contorno; no menu de pausa do Player, Tab navega o
foco e Enter ativa (`src/ui/StudioFrontend.cpp`).

### 6.4 Gizmo e construção de precisão

- **Mover**: arrastar eixo/plano mantém, quando possível, o tamanho da peça;
  com **Ctrl** sobre uma face de caixa posiciona a seleção nessa face (raycast
  OBB com desempate estável; contato SAT de 15 eixos). **Shift** libera a
  grade e combina com Ctrl. Sem Ctrl, valem os incrementos da barra:
  `GridMode::Relative` preserva o arredondamento do início do gesto;
  `GridMode::Absolute` quantiza a coordenada projetada.
- **Escalar**: alças de face (`ScaleFace`) preservam a face oposta; **Alt**
  escala centrada. Incremento absorve arredondamentos per-gesto.
- **Girar**: anéis com leitura de arco em graus; passo 15° pode ser
  configurado.
- **Encaixar face/Borda/Vértice** (aba Modelo → Construção): funcional apenas
  em caixas (esferas informam indisponibilidade); **Alinhar à normal** opcional
  gira o eixo Y local da seleção; fallback de último contato aceito se houver
  interseção.
- **Posicionar → Encaixar à superfície**: prévia verde/vermelha; clique
  confirma, `Esc` cancela e invalida (mudança de seleção/Play/concorrência
  também invalidam a proposta); grade `Afastamento` define a distância normal
  (0 = contato). Grupo **Montagem**: duplica ao lado/acima pela extensão real;
  **Repetir deslocamento** reaplica o último confirmado.
- **Undo/redo** (`EditHistory`): transforma o conjunto (Parts + pivôs de
  Modelos) como uma única operação; limite 128 operações / 32.768 registros
  de poses/pivôs; mudanças de topologia ainda não têm undo estrutural.
- **Superfície quadriculada de construção** (em desenvolvimento): `Material3D
  .surface` com `gridScale`/`gridIntensity`, gerador de textura de grade em
  `ModernMaterials` e fallback CPU `DrawConstructionGrid` no passe legado.

### 6.5 Painéis ImGui (frontend legado)

`EditorUI`, `StudioPanels`, `GamePanels` e `ConstructionUI` (ImGui)
implementam capacidades equivalentes num frontend de depuração/diagnóstico.
Layout persistido em `loblox_imgui.ini`.

## 7. Mundo 3D

Classes de instância (`src/objects/Instance.h`): `Workspace`, `Lighting`,
`Part`, `Baseplate`, `SpawnLocation`, `Folder`, `Model`, `Script`,
`LocalScript`, `ModuleScript`, `ServerScriptService`, `ServerStorage`,
`ReplicatedStorage`, `Decal`, `Texture`, `Attachment`.

- **Part**: formas `Block` e `Sphere` (a esfera usa o menor eixo como
  diâmetro), tamanho padrão 4×4×4 e mínimo 0,2 por eixo, cor padrão
  RGB(163,162,165); posição/rotação mundiais (quaternião armazenado);
  `Anchored` (padrão falso) e `CanCollide` (padrão verdadeiro);
  `BodyProperties` (`density` 1, `friction` 0,8, `restitution` 0,
  `linearDamping` 0,02, `angularDamping` 0,1); `NpcProperties` embutido
  (`enabled`, `speed`, `maxHealth`, `respawnDelay`, `turnSpeed`).
- **Workspace**: raiz do Mundo; `fallenPartsDestroyHeight` remove peças
  caídas abaixo do nível. `Lighting` alimenta sol, ambiente, névoa e céu.
- **Cena de fábrica** (`buildDefaultScene`): Baseplate + SpawnLocation;
  `installDefaultScripts` cria o rig Linblux, a animação `r6-jump`, o script
  `Game.server.lua`, ServerStorage e ReplicatedStorage.
- **Modelos** (`Container`): agrupam Parts/Modelos/Scripts com IDs estáveis;
  pivô explícito persistido no JSON v4 (Modelos antigos usam o centro das
  caixas envolventes); escala uniforme 0,001–1000, sem cisalhamento;
  Ctrl+G/Ctrl+Shift+G preservam poses mundiais; validação antes de alterar
  filhos; arraste inteiro = 1 operação de histórico.
- **Serviços de armazenamento** (`ServerStorage`, `ReplicatedStorage`):
  guardam modelos/protótipos fora do Mundo — sem render, sem corpos Bullet,
  sem raycast, sem interação e sem encaixe; mover de volta ao Mundo restaura
  participação conforme os próprios dados. Não representam processos de rede.
  Serviços protegidos não são excluídos nem reparentados.
- **Spawns** (`Spawn.cpp`): `SpawnLocation` ou atributo `spawn/Spawn` em Part;
  prioridade `SpawnPriority` (clamp ±1e6); ponto de nascimento
  `SpawnTopCenter`.
- **Decals/Texturas** (`Surface`): componente por face de Part `Block`, com
  transparência, rotação UV, cor RGBA, repetição/offset e atributo opcional
  `flip_u`.
- **Attachments** (`Attachment`): âncora com transform local (posição/rotação,
  escala 1), pose global composta, frente −Z; um par de âncoras de Parts
  distintas define **junta fixa** Bullet (`connect_to`/`disconnect`, 64
  iterações de solver; sem dobradiça/motor com limites nesta fase).
- **Scripts**: `Script`/`LocalScript` com `source` + `enabled`; `ModuleScript`
  é biblioteca carregada por `require`.

## 8. Materiais e gráficos

### 8.1 Materiais (`Material3D`)

`Material3D` (JSON `material3D`, opcional e validado): `baseColor`,
`metallic`, `roughness` 0,45, `normalScale`, `emissive`+`emissiveStrength`,
texturas baseColor/normal/metallicRoughness/occlusion, studs 4×4,
`offset`/`rotation`, `finish` (0 liso, 1 concreto, 2 madeira) e, em
desenvolvimento, `surface{0 material, 1 grade, 2 liso}`, `gridScale`,
`gridIntensity`.

Materiais efetivos das Parts (6): `SmoothPlastic`, `Plastic`, `Wood`, `Metal`,
`Concrete`, `Neon`. Presets nomeados (`MaterialPresetName`): Plástico,
Concreto, Madeira, Metal, Vidro simples, Emissivo. `LegacyMaterial3D` mapeia
os materiais antigos (Concrete→1, Wood→2, Metal→3, Neon→5, Plastic
roughness 0,3).

### 8.2 Renderização (`src/rendering`)

- **Céu procedural** em GLSL com gradiente zenith/horizonte/ground, sol,
  nuvens, atmosfera e rotação (`SceneRenderer.cpp`).
- **Névoa e sombreamento de face "warm sun"** em CPU (sem shader), coerente
  entre editor e Play.
- **Iluminação**: `Lighting` entrega `sunDirection`, `sunColor`, ambiente e
  névoa com clamps determinísticos; passe de guias de autoria via
  `EditorGeometryView` com depth test e bias (F8/Shift+F8/Ctrl+F8).
- **Sombras direcionais** (`DirectionalShadow`/`ModernMaterials`): `Auto`,
  `Off`, `Economy`, `Soft`, `High`; mapas 1024²/2048²; `shadowDistance` 16–300
  (padrão 80), `shadowStrength` 0–1; filtros 4/8/16 taps; sem cascatas e sem
  SSO. Variáveis: `LINBLUX_SHADOW_QUALITY`, `LINBLUX_DISABLE_SHADOWS=1`,
  `LINBLUX_DISABLE_HIGH_SHADOWS=1`.
- **Qualidade** `Legacy/Low/Medium/High` (`LINBLUX_RENDER_QUALITY` ou
  `--quality` do Player); Médio é padrão; Alto usa GGX/Schlick — **não** é
  PBR/IBL completo. `LINBLUX_DISABLE_MATERIAL_SHADER=1` injeta falha manual.
- **Anti-aliasing** Auto/None/FXAA/MSAA4 (`LINBLUX_ANTIALIASING`,
  `LINBLUX_DISABLE_MSAA=1`, `LINBLUX_DISABLE_FXAA=1`); `SceneTarget` executa o
  postprocess e guarda fallback quando o hardware recusa o modo.
- **Passes**: opacos → decals → translúcidos por distância → overlays de
  seleção — sem OIT. A AABB de esfera usa `position ± sphereRadius`, coerente
  com picking e Bullet.

### 8.3 Limitações gráficas

Sem SSAO, sombras de contato ou IBL. Capturas com llvmpipe não medem
desempenho de GPU física. Coplanaridade de faces segue sendo problema de
geometria (passe de contornos não corrige superfícies duplicadas).

## 9. Física

Bullet Physics (`src/physics/PhysicsWorld.cpp`):

- `btDiscreteDynamicsWorld`, gravidade padrão **(0, −98,1, 0)**, passo fixo
  **1/60 com até 5 subpassos**; gravidade configurável por Lua/Studio
  (1–500) e aplica-se também aos cálculos do jogador.
- **Um corpo por Part**, criado em `addPart` e destruído em `removePart`.
- `Anchored=true` → `CF_STATIC_OBJECT`; corpo de NPC mantém fator angular
  (0,0,0) e ativação permanente; CCD para peças rápidas.
- `CanCollide=false` → sensor: notifica contatos quando o Bullet gera o par,
  não responde; pares entre dois estáticos não são produzidos.
- **Edição** congela corpos (`setPartEditing`); física é autoritativa do
  personagem (`RuntimeSession` é amigo explícito).
- **Raycast** `FilteredRay` (`solidOnly`, `staticOnly`, peça ignorada) usado
  pelo Studio e pelo player; `isSpaceClear` valida degraus/colocação;
  `setBodyYaw`/`lockAngularMotion` para NPCs; `cameraClearance` (sweep de
  esfera) e `preventFallThrough` (sweep de cápsula) no runtime.
- **Juntas fixas**: pares de Attachments como fonte da verdade na `Scene` via
  `btFixedConstraint` por ID, 64 iterações, removidas antes dos corpos e
  desabilitando colisão entre as duas peças conectadas.

## 10. Lua

Fonte: `src/scripting/LuaRuntime.cpp` (sandbox e cotas); exemplos executados
pelos testes em `assets/examples/lua/*.lua`.

### 10.1 Modelo de execução

- Ambiente controlado: `dofile`, `loadfile`, `load`, `collectgarbage`,
  `pcall`/`xpcall` removidos; `setmetatable` recusa `__gc` não nil; sem
  `io`, `os`, `package`, `debug`, rede ou carregamento de binários.
- **`require`** só aceita ModuleScript do projeto (máx. 64 módulos,
  profundidade 16); sem caminho nem biblioteca nativa.
- Scripts executam na **cópia de runtime**, apenas sob `Workspace` ou
  `ServerScriptService`; reabilitar um Script reinicia seu contexto; fora da
  árvore executável callbacks/tarefas são cancelados.
- `linblux.self` é o pai do script; `linblux.script` é o próprio script.
- **Cotas**: 8 MiB de Lua; 100 mil instruções por retomada, 250 mil por passe;
  128 tarefas e 128 listeners; conteúdo nativo ≤ 10 mil objetos / 8 MiB de
  nomes/fontes/atributos; erros são reportados com script, linha e traceback
  sem encerrar o jogo.
- Hooks de finalização desativados (proteção da cota contra `__gc`);
  o evento `stop` serve de cleanup.
- Bindings legados preservados: `Instance.new`, `Vector3.new`,
  `Color3.fromRGB`, `task.spawn/wait`, `game`, `workspace`, metatable
  `Loblox.Handle`.

### 10.2 API `linblux` (v1, `api_version == 1`)

```lua
-- Cena e objetos
linblux.scene.create("part"|"group"|"model", {name=, parent=, position=,
    rotation=, size=, color=, material=, shape=, anchored=, collidable=,
    transparency=})
linblux.scene.find(nome_ou_id); linblux.scene.root
linblux.scene.group({...}, "Nome")
-- Instância: name, id, type, alive, parent, FindFirstChild/GetChildren/
-- GetDescendants/Destroy/Clone/GetAttribute/SetAttribute (aliases e nativas)
-- Part: Position/Size/Rotation/Anchored/CanCollide/Transparency/Color/Material,
-- velocity, mass, body{density,friction,restitution,linear_damping,
-- angular_damping}, apply_force/impulse/torque/apply_angular_impulse,
-- transform, get/set_transform, point/vector_to_world/local, scale_by,
-- add_script, components, remove_component
-- Modelo: position/rotation/scale, transform, pivot/get_pivot/set_pivot,
-- ungroup, add_script
-- Attachment: position/rotation/world_*, direction, connect_to/disconnect/joint

-- Física
linblux.physics.raycast(origem, direção, distância, ignorar?)->{object,point,normal,distance}|nil
linblux.physics.nearby(centro, raio)            -- centros no raio, sem o jogador
linblux.physics.overlap_box/sphere(centro, dim, {rotation=, ignore=,
    include_player=, solid_only=, max_results=1..256})  -- 8 consultas/quadro
linblux.physics.gravity() / set_gravity(1..500)

-- Tempo (pausa sincronizada, intervalos 0..3600 s)
linblux.time.now() / wait(s) / spawn(f,...) / after(s,f,...) / every(s,f) /
cancel(tarefa)

-- Eventos
linblux.events.on("collision_enter", function(hit) end, escopo?)
linblux.events.emit("nome", {})
```

**Eventos nativos** (não emitíveis por Lua): `start` (recebe 0, entregue
após inicialização), `update` (dt), `physics_update` (dt = 1/60 antes de cada
passo, independente do FPS), `collision_enter/exit`
(`{first,second,first_id,second_id}`; tolerância 0,02; até 1024 pares;
jogador usa ID 0), `spawn`, `health_changed`, `death`, `destroy` (requer
escopo e entrega o ID), `key_pressed/released`, `mouse_pressed/released`,
`gamepad_pressed/released`, `inventory_changed`, `interaction_completed`,
`item_action`. Tópicos emissíveis customizados são permitidos via
`events.emit`.

### 10.3 Jogador e câmera

- `linblux.player` — jogador local estável (ID 0): `health`, `max_health`,
  `walk_speed`, `jump_height`, `auto_rotate`, `position`, `velocity`,
  `grounded`, `state` (idle|walking|jumping|falling|landing|dead); métodos
  `damage/heal/respawn/teleport/set_checkpoint/set_animation/move/jump`,
  `apply_force/impulse` na cápsula. Troque vida/velocidade com segurança;
  morte e respawn mantêm regras nativas (evento `spawn`).
- `linblux.camera.configure({mode="orbit|follow|first_person", distance=,
    min_distance=, max_distance=, sensitivity=, allow_first_person=, locked=,
    allow_shift_lock=, fov=1..170})`; cameraFov persistido nos padrões do
    jogador; cenas antigas usam 60.
- `linblux.camera.set_pose({position=, target=|rotation=, up=, fov=,
    duration=0..60})` assume a câmera com transição de quaternion (evita
    colapso em rotação de 180°); `release()` devolve o controle;
    `state()` consulta posição/target/fov/scripted/transitioning.
- `linblux.animations.list()` metadados dos recursos (`id`, `name`,
  `duration`, `loop`).

### 10.4 Persistência privada

`linblux.storage.get/set/remove/status` — escopo atribuído pelo host:
`experience:<installationId>` ou `scene:<caminho canônico>`; limites: 128
chaves de 1..128 bytes, 64 KiB por valor, 256 KiB no conjunto, profundidade 8,
1024 nós, 128 leituras/64 alterações por quadro; arquivo `progress.json`
chmod 0600 em `$XDG_STATE_HOME/linblux/progress` (fallback
`~/.local/state/linblux/progress`) com flock exclusivo; gravação limitada a
1 a cada 2 s + tentativa final no stop/pause; corrupto/versão desconhecida não
é sobrescrito (erro explícito e preservação dos bytes).

### 10.5 Entrada

`linblux.input.key_down/pressed/released(nome)` (`a..z`, dígitos,
`space,enter,tab,backspace,left/right/up/down,left_shift/right_shift,
left_ctrl/right_ctrl,left_alt/right_alt,home,end,page_up/page_down,insert,
delete`), `mouse_down/pressed/released` (`left,right,middle,side,extra`),
`pointer() -> {x,y,dx,dy,wheel}`,
`gamepad_connected/down/pressed/released/axis` (`up/right/down/left,
north/east/south/west,left_bumper/right_bumper,left_trigger/right_trigger,
select/start,left_stick/right_stick`; eixos brutos sem deadzone). No Studio
exige posse de gameplay pelo roteador; no Player só com janela focada e sem
pausa. É consulta de amostra, não dispositivos diretos; sem remapeamento
persistente nem vibração.

### 10.6 Serviços via `game:GetService`

`Players` (LocalPlayer, `Character`, `CharacterAdded`, `Respawn`),
`RunService` (somente `Heartbeat`), `AnimationService`
(`GetAnimation`), Workspace, Lighting, ServerScriptService, ServerStorage,
ReplicatedStorage. **Não há serviços de rede.**

### 10.7 NPCs e navegação

NPC é uma Part com `NpcProperties`; comportamento vem de Script.
`linblux.npc.create/attach/detach`, `move_to/stop/state
(idle|moving|arrived|unreachable|dead)/speed/set_speed,
health/max_health/set_health/damage/heal, set_rig/rig/animation/attack,
target/path`, `find_path(de,para)`, `navigation()` (grade XZ da física real:
superfícies estáticas, normal ≥ 0,7, altura livre 4,5, folga lateral 1,0;
A* 8 vizinhos degrau ≤ 1,2 sem cortar cantos; máx. 4096 células; reconstrução
após mover geometria estática). Controlador dinâmico dirigido pelo Bullet com
giro limitado (`turn_speed` rad/s).

### 10.8 Áudio

`linblux.audio.play(ref, {volume=, loop=, owner=})` e
`stop/pause/resume/set_volume/set_loop/volume/is_playing/is_paused/looping/
duration/is_streaming/device_ready/list/stop_all` — até **64 vozes**,
streaming para músicas longas, `owner` libera a voz quando o objeto some.
Sem áudio espacial/3D nem mixer.

### 10.9 Itens, interações e UI

- `linblux.items` e `linblux.interactions` — ver §11.
- `linblux.ui` — UI declarativa por script (Panel/Label/Button/Image/Bar),
  tela virtual **1280×720**, escala `min(largura/1280, altura/720)`;
  128 elementos, profundidade 4, 64 mutações/quadro, 64 eventos, texto
  ≤ 512 bytes; propriedades `x,y,width,height,anchor,text,color,background,
  opacity,visible,font_size,value,max,z,image`; eventos `click`/`hover`
  (somente Button recebe ponteiro); cada elemento pertence ao script criador.
  Não substitui o HUD protegido nem o RmlUi do Studio.

### 10.10 Recursos autorais

Imagens/decals: atributos `decal_image/texture_image` (+ `decal_face`,
`decal_transparency`, `decal_flip_u`/`texture_flip_u`); rigs via
`bodies[i].image` ou `rig:add_decal`. `game::ImportMediaTo` copia para
`assets/<id>.<ext>` (ID FNV-1a do conteúdo; reimportação reusa destino);
remoção exige resolver usos (manifesto, cena, atributos, scripts, sons):

- `DescribeSoundUsage`/`ImageInUse` identificam o objeto/atributo/script que
  cita a referência; a remoção salva a cena antes de mover o arquivo;
  o arquivo externo original nunca é apagado.
- Referência canonical de mídia é sempre `assets/<arquivo>` relativa à raiz
  (normalização única `game::NormalizeImageReference`; caminhos absolutos não
  são gravados).

## 11. Inventário e armazenamento

### 11.1 Protótipos de itens

No Studio, um `Model` marcado **Protótipo de item** tem tipo estável e ícone
opcional; registrado antes dos scripts do Play. Estrutura convencional em
`ServerStorage/Itens` (Folder criada pelo menu **+**; **Itens** não é serviço).

```lua
linblux.items.register("chave", modelo, {name=, icon="assets/x.png",
    equipable=, grip_position=vec3, grip_rotation=vec3})
linblux.items.spawn("chave", {position=, rotation=, scale=})
-- collect/add, equip/unequip, drop (procura chão/espaço livre), remove,
-- list, equipped, info, state (world|stored|equipped)
```

Limites: 64 tipos, 128 instâncias, 64 itens guardados, 256 componentes/128
Parts por protótipo. `inventory_changed` entrega `{action, id, item}`; estado
da bolsa é transitório de Play (persistência real entre sessões usa
`linblux.storage`). Clonar um protótipo registra tipo derivado com sufixo do
ID da cópia.

### 11.2 Ações programáveis

`linblux.items.configure(item,{magazine=, reserve=, consume=, interval=,
reload_time=})` define estado por instância; `activate/reload` retornam
`sucesso,motivo`; `deactivate/cancel_reload/set_ammo/combat` completam a API;
`item_action` entrega `{item, action, ammo, reserve, magazine, reloading,
active}`. Faixas: pente 0..100000, reserva 0..1000000, consumo 1..100000,
intervalo/recarga 0..60 s. A engine não tem arma fixa: efeitos ficam nos
scripts (RifleDemo demonstra isso).

### 11.3 Coleta, interação e hotbar

Segurar **E** por 0,6 s (ou botão oeste do gamepad) coleta itens world ou
completa interação registrada:

```lua
linblux.interactions.register(porta, {label="Abrir porta", distance=6,
    hold_seconds=0.6, key="e"})
```

Limites: 256 entradas; `distance` 0,25–20; `hold_seconds` 0,05–10; máx. 4
candidatos por atualização com linha de visão. Raycast evita interação através
de obstáculos. **Hotbar** compartilhada entre Play do Studio e Player:
`1–9` equipa/desequipa, PgUp/PgDn pagina, bumpers percorrem, `Delete` solta o
equipado com física preservando identidade/scripts/dados. Menu captura a
entrada antes do runtime.

## 12. Player

- **Runtime**: `RuntimeSession` (estados idle/walking/jumping/freefall/
  landing/dead; coyote time + jump buffer); `PlayerSettings` (gravity,
  cameraFov 1..170 padrão 60, câmera `Classic/Follow/FirstPerson` com shift
  lock, occlusion e transições scriptadas).
- **Personagem**: rig R6 de 6 corpos/juntas (Torso, Head, LeftArm, RightArm,
  LeftLeg, RightLeg), rig padrão `R6Character`, walkSpeed 16, jumpHeight 7,
  gravidade 98,1, cápsula de colisão 5,8. Rig de mapas antigos recebe um
  runtime-only R6 compatível quando necessário.
- **Spawn/respawn**: tenta checkpoint (Lua) e depois spawns normais; queda
  significativa é detectada (tempo/descent/altura) e relatada.
- **HUD** (`ExperienceOverlay` + `LuaUiRenderer`): barra compacta de vida no
  canto superior-direito (em desenvolvimento), hotbar inferior com slots
  ocupados, prompt de interação com progresso e avisos de item; desenhado
  apenas em jogo. Scripts complementam a UI (§10.9) sem tocar no HUD nativo.
- **Menu** (`PlayerMenu`): lateral Continuar/Sair + páginas Jogador/Câmera/
  Controles/Áudio/Vídeo/Ajuda; volume master/mudo; **protegido contra
  alterações por scripts**. Simulação continua enquanto o menu está aberto.
- **Marca 3D no Player standalone** (`BrandLogo3D`): carrega
  `midia/3d/logo-3d.glb` uma vez, centraliza e orienta sua face XZ para a
  câmera. As duas aparições compartilham cache a 30 Hz, rotação de 12°/s
  e supersampling 2× limitado a 1024 px. Atualizar antes de `BeginDrawing`
  apenas quando visível; destruir antes de `CloseWindow`. Controle de menu
  sem caixa ou ícone adicional, com clique e Esc. PNG permanece como fallback
  e ícone nativo. CMake copia/instala em `share/linblux/midia/3d`.
  Home e header RmlUi ainda usam PNG; migração pendente.
- **Controles nativos compartilhados** (`rendering/UiControls`): Button,
  Slider e ProgressBar usam tokens de `ThemeColors.generated.h`, sem
  dependência do editor e sem framebuffer por controle. Menu standalone
  usa os dois primeiros; HUD compartilhado de Studio/Player usa ProgressBar
  com preenchimento proporcional mesmo abaixo da largura das bordas.
  A biblioteca completa de componentes RmlUi ainda não foi migrada.
- **Janela**: 1280×720, mínimo 900×560, V-SYNC, MSAA 4×, redimensionável,
  ícone builtin do logo; fontes DejaVu/Liberation do sistema.
- **Controles**: WASD move, Espaço pula, Shift alterna shift lock, roda do
  mouse zooma, botão direito orbita/hide cursor; itens usam E (coletar),
  1–9 (equipar), Delete (soltar) e ações por script (RifleDemo usa clique e
  R para recarregar).

## 13. Áudio e mídia

### 13.1 Áudio

- Formatos importados: **MP3/WAV/OGG**; validação de contêiner via FFmpeg
  (orçamento de 2 s, whitelist); duração real por metadados; streaming para
  músicas longas; máximo de 64 vozes.
- Reprodução 2D global: **sem atenuação 3D, mixer por categoria ou efeitos**.
- `AudioSystem` do Studio faz pré-escuta; o Player abre o dispositivo
  **antes** de iniciar os scripts. Em CI/headless o contrato de handles e
  timing vale pela duração decodificada — não comprova som audível.
- **Música de fundo/playlist declarativa** (`MusicSettings/MusicPlaylist`,
  fila 64): playlist tem precedência; `stop()`/`stop_all()` não reiniciam a
  playlist (avanço só por término natural); Lua pode controlar tudo por
  `linblux.audio`.
- Remoção segura de áudio exige resolver usos (`DescribeSoundUsage`), move o
  arquivo para backup temporário e restaura se o salvamento falhar.

### 13.2 Imagens

- Importação via Studio → Recursos; PNG/JPEG cópias gerenciadas com ID
  FNV-1a: 32 MiB por arquivo, 64 MiB decodificado (JPEG máximo 16384 px).
  Aliases antigos `loblox.png` → logo oficial e `rosto1.png` → rosto embutido.
- Recursos nomeados: `assets/image-names.json` guarda nomes amigáveis;
  exclusão recusa enquanto houver uso (manifesto, cena, atributos, scripts),
  salva a cena antes e restaura o arquivo se o salvamento falhar.
- Superfícies decal/textura preservam UVs legados; atributo `flip_u` optativo
  (atributos incluídos no `.lo`).

### 13.3 Vídeo

O Início usa `midia/careditfullhd.mp4` (HEVC, 1280×720, 30 fps, ≈24 s, sem
áudio), decodificação assíncrona FFmpeg com dois buffers RGBA; fallback
grafite **estático** quando a biblioteca/codec não está disponível
(`LINBLUX_ENABLE_VIDEO`). Não existe player de vídeo in-world nesta Alpha.
O vídeo fica fora do pacote distribuível.

## 14. Persistência

### 14.1 Projetos e experiências

- **Serialização**: `kFormatVersion=7`; JSON canônico; Lua declarativa
  `serialization/SceneLua.cpp` (não executa scripts); leitor recusa versão
  futura; v1–v6 lidas com as regras antigas; toda gravação é atômica
  (temporário + fsync + rename + fsync do diretório). Cenas antigas de
  superfícies usam `EncodeSurface/DecodeSurface` (v3), Material3D v7 opcional,
  Model.modelPivot v4, Attachment v5, jointTarget v6.
- **Manifesto** `experience.json` v1 (assinatura legada `loblox-experience`
  preservada): id/installationId/name/description/icon/banner/trashed/
  archived/...; lugares em `places/*.loblox.json`; mídia em `assets/` com
  caminhos relativos. Novas experiências copiam mundo e personagem atuais da
  engine; editar experiência comum não muda o padrão.
- **Biblioteca local**: `$XDG_DATA_HOME/linblux` (reutiliza a legada `loblox`
  quando é a única existente; override de teste via `LINBLUX_DEFAULTS_ROOT`).
  Catálogo/armazém em `library.json`; salvar grava revisão nova imutável e
  troca o manifesto atomicamente (atual + até 2 revisões anteriores).
- **`.lo`** — contêiner binário próprio **LOEXP v1** (não ZIP): contagem 4 B,
  tamanho 2 B + 8 B + CRC32 zlib; orçamento de 256 MiB; importação em staging
  + rename; recusa ZIP (mensagem explícita no formato). Importação cria
  instalação nova; mover a instalação preserva a identidade.
- **Templates de criação**: `empty`, `baseplate`, `classic`, `obby`
  (`assets/templates/*.loblox.json`, via `LINBLUX_FACTORY_DIR`).
- **Arquivamento × exclusão**: arquivar mantém a pasta no disco e permite
  restaurar; excluir permanentemente pede confirmação com o nome, reserva a
  pasta como `.deleting-ID-...` e remove via descritores POSIX
  (openat/O_NOFOLLOW, renameat/unlinkat) sem seguir links nem percorrer
  vizinhos; erro real é exibido; uma falha pode deixar `.deleting-*`
  com o conteúdo restante — caminho informado, sem prometer desfazer.

### 14.2 Padrões da engine

- **Padrão personalizado do usuário**: `$XDG_DATA_HOME/linblux/engine-defaults`.
  Publicar (**Salvar como padrão da engine**) grava snapshots e troca
  atomicamente os dois ponteiros (mundo + personagem); rascunho é
  `Salvar rascunho/Ctrl+S`.
- **Padrão de fábrica**: `assets/defaults` no checkout, `share/linblux/defaults`
  em instalações (somente leitura); índice `index.json` com
  `format: linblux-engine-defaults`.
- Publicação deliberada na árvore versionada:
  `LINBLUX_DEFAULTS_ROOT=/abs/caminho/assets/defaults ./build-linblux/linblux`
  (caminho absoluto e gravável; grava revisões, guarda backup de índice).
- Restaurações fazem backup do índice personalizado antes de restaurar o que
  está em `factory/`.
- Testes de verificação: `linblux_engine_default_lifecycle` (cria original,
  publica, reinicia em outro processo, roda o personagem correto),
  `linblux_engine_default_paths` (raízes separadas, XDG respeitado),
  `linblux_engine_default_restart` (UI real, cartão, reinício).

### 14.3 Compatibilidade (resumo)

Identificadores legados preservados e **não** renomeados na interface:
namespace C++ `loblox`, metatable Lua privada `Loblox.Handle`, assinatura
JSON `loblox-project`, manifesto `loblox-experience`, pacote `LOEXP`, extensão
`.loblox.json`; pasta de dados legada `loblox` reaproveitada; Variáveis
`LOBLOX_*` de diagnóstico aceitas. Parts novas têm nomes compactos; renomeação
manual recusa espaços/Unicode; carregamento preserva nomes antigos e IDs.
Configuração de `cameraFov` ausente assume 60. Malhas/esqueletos arbitrários
**não** são uma evolução pronta (§21).

## 15. Interface e renderização de UI

- **RmlUi 6.3** (FetchContent, commit fixado, MIT) desenha a UI em layer
  framebuffer GL3 exclusivo com MSAA própria (opção de build
  `LINBLUX_RMLUI_MSAA_SAMPLES`, padrão 4); a camada resolve no target de
  postprocess antes do backbuffer do raylib, então botões/health bar usam
  border-radius com suavização genuína.
- **Documentos/estilos** (`assets/ui`): documents `home.rml`, `ribbon.rml`,
  `studio.rml`; styles `studio.rcss`, `experiences.rcss`, `player.rcss` e
  `theme.rcss` gerado. Ordem de carregamento: `experiences.rcss` antes de
  `theme.rcss` (contrato de UI).
- **Localização**: `assets/ui/locales/pt-BR.json` no catálogo central, usado
  por `i18n::Text/Label/Markup`; RML estático usa pt-BR. IDs RML, comandos,
  nomes de arquivo e APIs não são traduzidos.
- **Tema**: `assets/ui/theme.json` alimenta ImGui e RmlUi; geração de
  `theme.rcss`, `src/ui/ThemeColors.generated.h` e
  `src/editor/StudioPalette.generated.h` por `scripts/generate-theme.py`
  (com `--check` para CI). Tema em desenvolvimento: monocromático
  preto/grafite com painéis translúcidos (mistura alfa na janela, sem blur) e
  destaque branco, substituindo o destaque vermelho da base anterior.
- **Dear ImGui**: sempre inicializado (`rlImGuiSetup`); compõe o frontend
  legado/diagnóstico (`EditorUI`, `StudioPanels`, `GamePanels`,
  `ConstructionUI`); layout persistido em `loblox_imgui.ini`.
- **Fontes** são carregadas do sistema (DejaVu/Liberation/Noto), sem copiar
  arquivos; como último recurso, fallback automático para o caminho anterior.
- **Ícones**: procedurais vetoriais em `EditorUI::DrawIcon` no legado e ícones
  Lucide (PNG branco 96×96) gerados de SVGs (licença ISC upstream) na árvore
  `assets/ui/icons/` — pipeline por `tools/icons/generate_icons.py`, ainda em
  desenvolvimento no frontend RmlUi (`RmlFrontendSupport.cpp`).
- **UI de Lua** usa o mesmo renderizador no Play do Studio (textura do
  viewport) e no Player (tela), garantindo igualdade de hitbox.

## 16. ATLAS

ATLAS é ferramenta local de Python (sem rede) para investigação estrutural:
índice reconstruível de arquivos, símbolos, testes e grafos CMake em
`.atlas/index.sqlite`. **O índice é cache; código, CMake, testes e este manual
são a fonte da verdade.** Relações textuais não provam chamadas nem cobertura
(extrator C++ é regex com `confidence=medium`).

### 16.1 Comandos

```sh
python3 tools/atlas/atlas.py <comando> [--json] [--root DIR]
```

| Comando | Para que |
| --- | --- |
| `init [--force]`, `update` | construir / atualizar índice incremental |
| `status`, `doctor [--strict]`, `verify`, `health --brief` | estado e sanidade |
| `locate "..." [--limit N] [--module M]` | busca (FTS5 BM25 + símbolos/glossário/testes) |
| `symbol Nome` / `text "..." --limit 5` | definições e ocorrências |
| `impact <caminho> --depth 2 --limit 30` | consumidores, riscos e documentação (`LINBLUX.md`) |
| `trace "..." --limit 4` | onde um sintoma pode morar |
| `tests-for <caminho>` | testes associados |
| `diagnose "sintoma" --brief` | diagnóstico composto (locate+symbol+tests-for+impact) |
| `fix-plan` | plano de correção somente leitura |
| `docs` | regenerar inventário em `docs/atlas/generated/inventory.md` |
| `bench --iterations 5`, `eval` | métricas locais; 12 perguntas mantidas manualmente |

Saída JSON usa envelope estável `{ok, command, data}` (erro no topo e dentro
de `data`); exit codes 0/1/2.

### 16.2 Modos de I/O e recomendação HDD

- `ATLAS_IO_MODE=normal|hdd-safe|hdd-critical` (ou `--io-mode`).
  `hdd-safe` é recomendado em HDD mecânico: só consultas leitura (SQLite
  `query_only`/`immutable`), indexando apenas em `init`/`update`.
  `hdd-critical` **bloqueia** `update/verify/doctor/bench/docs` e reporta o
  índice ausente sem reconstruí-lo.
- Fluxo recomendado: `update` → `diagnose "sintoma" --brief` → abrir os
  destaques → `impact` + `tests-for` → corrigir o menor suficiente →
  **uma** atualização final do índice após mudanças (`atlas.py update`).

### 16.3 Confiança e limites

- `inferred=true` não é evidência textual; includes de sistema aparecem
  "não resolvidos"; cota de 2 MiB por arquivo; BFS de impacto limitado é
  resultado parcial.
- Contratos por módulo vivem na §2.2 deste manual; `doctor`, `graph`/`impact`
  e relacionamento de testes consultam `LINBLUX.md` (adaptado aos três
  arquivos que antes eram `docs/atlas/module-contracts/*.md` e
  `CONTRIBUTING.md`).
- Índice incompatível é recuperado por `update` sozinho ou `init --force`;
  `ModuleNotFoundError` se não rodar da raiz ou sem `--root`.

## 17. TURBO e testes

TURBO (`tools/turbo/turbo.py`) compila incrementalmente e seleciona/executa a
validação relevante com timeout individual, orçamento global, encerramento dos
processos e logs — sem rede. Dados usados: Git, CMake, CTest e ATLAS.

### 17.1 Comandos

| Comando | Para que |
| --- | --- |
| `doctor` | dependências e tamanho do índice |
| `inventory [--json]` | alvos e testes |
| `build [--target auto]` | build incremental (log em `.turbo/logs/<id>/`) |
| `test --mode fast|impact|full --changed [--plan]` | seleção + execução |
| `report` / `status` | última corrida/estado |
| `benchmark --run-fast` | medições |
| `organize --dry-run` | somente leitura |

Opções úteis do `test`: `--plan` (mostra seleção e o que fica de fora),
`--no-build`, `--no-cache`, `--budget S`, `--per-test-timeout S`,
`--max-graphics N`, `--jobs`, `--no-atlas-update`. Lock em `.turbo/run.lock`
(flock), runner com `start_new_session=True` e SIGTERM→SIGKILL; logs em
`.turbo/logs/`.

### 17.2 Regras de aprovação (não negociáveis)

- **FAST** = headless; **IMPACT** = FAST + consumidores + integrações (limite
  padrão 4 testes gráficos → `PASSED_PARTIAL`); **FULL** = só release/audit,
  nunca automático. FAST aprovado não é FULL aprovado.
- Estados: `PASSED`, `PASSED_PARTIAL`, `REUSED`, `FAILED`, `TIMEOUT`,
  `BUILD_FAILED`/`CONFIG_FAILED`, `NOT_RUN`, `UNKNOWN`, `NO_TESTS`, `DOCS_OK`,
  `ASSET_OK`. Exit codes do `test`: 0 aprovado, 1 falha, 2 cobertura parcial/
  desconhecida, 124 timeout.
- `PASSED_PARTIAL`, `BUDGET_EXHAUSTED`, `UNKNOWN`, `REUSED`, `NO_TESTS` e
  `TIMEOUT` **não são aprovação plena**. `REUSED` exige estado anterior
  aprovado (não parcial), fingerprint idêntico e cobertura conhecida.

### 17.3 CTest direto (somente headless confirmados)

```sh
ctest --test-dir build-linblux -N
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

Inventário de testes (67 registrados no CMake atualmente; `-N` lista tudo):

- **Headless** (direto permitido): `linblux_player_help`,
  `linblux_player_validate`, `experiences`, `tests`, `game`, `studio`,
  `manipulation`, `spawn`, `defaults`, `engine_default_lifecycle`,
  `engine_default_paths`, `item_actions`, `mouse_lock`, `startup_no_display`,
  `lua_player_validation`, `video`...
- **Gráficos/Xvfb** (**sempre via TURBO**): `decal_render`, `rig_image`,
  `gizmo_visual`, `experience_ui`, `rml_ui`, `lua_ui_render`, `lua_ui_player`,
  `rig_input`, `visual_studio`, `sky_capture`, `graphics_quality`,
  `studio_ui`, `game_ui`, `editor_input`, `experience_startup`,
  `editor_restart`, `home_video`, `engine_default_restart`,
  `progress_player_ui`, `player_menu`, `player_menu_ui`, `rifledemo_player`
  e demais capturas supervisionadas.
- **Suítes de ferramentas**: `linblux_atlas`, `linblux_backup`,
  `linblux_turbo` (`tests/atlas`, `tests/backup`, `tests/turbo`, em
  `python3 -m unittest discover -s tests/<pasta> -p 'test_*.py'`).

Capturas com llvmpipe não medem GPU física; sink de áudio null não comprova
som audível; teste não registrado/não executado nunca contam como aprovado.
Ferramentas ausentes (Xvfb, xdotool, Pillow, zenity) podem impedir o registro
de alguns testes.

## 18. Desenvolvimento

### 18.1 Convenções de código

- C++17 sem extensões; CMake por módulos; sem novos sinônimos de serviço.
- **Preservar IDs persistentes**, hierarquia e serialização legada (JSON
  v1–v7, `.lo` LOEXP v1) e APIs Lua antiga (handles, nomes); Nova API Lua
  usa casing minúsculo e vetores `x/y/z`.
- **Isolamento autoria/runtime** (§2.2): Edit não simula queda físico;
  Play executa cópia; Stop preserva autoria. UI não duplica serialização,
  física ou cálculo de gameplay.
- Move/Scale por face, Snap e undo/redo são contratos de precisão existentes;
  não refaça sistemas aprovados para resolver problema de outra área.
- **Localização** e **tema** com catálogo central e geração (§15); sem
  duplicar cor/texto hardcoded.
- headers alterados exigem `atlas.py impact <caminho>` antes do assunto;
  mudanças de contrato obrigam a seção estável de edição (doc) e testes.

### 18.2 Procedimentos de mudança

1. Confira `git status -sb`, HEAD local/remoto e WIPs pendentes.
2. Localize com ATLAS (hdd-safe) e confirme no código real.
3. Configure/compile no build oficial; faça a menor correção suficiente.
4. Rode o conjunto de testes afetados (ctest headless direto quando o
   contrato o permite; gráficos via TURBO com plan/budget).
5. Revise o diff; atualize este manual se o comportamento/documento mudou.
6. Ao final: `python3 tools/atlas/atlas.py update` (índice atualizado).

### 18.3 Comunidade e comunicação de mudanças

- `scripts/checkpoint.py --files A B C`: commits por arquivos explícitos
  (compila, testa, valida origem, envia SEM reescrita de históricos);
  rejeita pastas genéricas, links, >50 MiB.
- `scripts/sync-safe.sh`: validação do remoto, `git diff --check`, worktree
  limpo, e `git push origin HEAD:main` **normal**.
- PR e bug template não são automação atual; issues com commit/versão,
  distribuição Linux, GPU/driver, passos para reproduzir e logs;
  vulnerabilidades seguem canal privado do GitHub (§20.4).
- Não suba builds, caches, logs, segredos, mídia privada ou experiências
  pessoais. Adicione caminhos explicitamente depois de revisar o diff.

## 19. Instruções para agentes de IA

O arquivo local `AGENTS.md` (stub, não rastreado pelo Git; `.gitignore` o
ignora) é carregado automaticamente por agentes OpenCode; as regras completas
estão neste manual. Agentes de qualquer ferramenta devem honrar:

- **Preservar trabalho existente**: `git status -sb`, `git rev-parse HEAD`,
  `git diff --stat` e `git diff --cached --stat` antes de editar; WIPs de
  interface, logo, materiais e renderização são trabalho do autor — nunca
  descartar. Não alterar código-fonte, projetos pessoais, mídias, scripts Lua
  ou recursos da engine "para adaptar documentação".
- **Proibições destrutivas**: `git reset --hard`, `git clean`, `git restore .`,
  `git checkout .`, `git push --force`, `rm -rf`, limpeza recursiva em
  `build-linblux/`, `.git/`, `third_party/`, exclusão por extensão, apagar
  `LICENSE`, backups não-markdown, vídeos, imagens, assets, experiências,
  arquivos desconhecidos (protegidos até esclarecer papel).
- **HDD-Safe**: HDD mecânico e ~6 GiB RAM; minimizar leituras repetidas e
  varreduras binárias/recursivas; sem benchmarks, FULL, cópias amplas ou
  backups duplicados de assets; priorizar texto e Git. Em `hdd-critical`
  nada de update/verify/bench/docs no ATLAS.
- **Validação honesta**: compile e teste a correção (busca textual não
  valida); `ctest` direto somente **headless confirmado**; gráficos,
  Xvfb/Mesa e integrações visuais **sempre** TURBO com timeout/orçamento/logs;
  `PASSED_PARTIAL`, `BUDGET_EXHAUSTED`, `UNKNOWN`, `NO_TESTS`, `REUSED` e
  `TIMEOUT` não são aprovação plena; teste ausente/não executado não conta
  como aprovado; encerrar quando build + regressão + integração pertinentes
  forem suficientes e registrar limitações e validação manual pendente.
- **Não publicar automaticamente**: sem commit ou push sem autorização
  explícita; sem modificar branch remota.
- **Documentação**: o projeto mantém apenas `README.md` e `LINBLUX.md`
  como documentação; atualize a seção correspondente deste manual quando o
  comportamento mudar (ver também §2.2, contratos por módulo).
- **Ferramentas de documentação**: ATLAS reconstrói o índice usando este
  manual como fonte de documentos; TURBO checa links quebrados nos `.md`
  alterados (docscheck).

## 20. Backup e segurança

### 20.1 AutoBackup (branch `auto-backup` órfã)

`tools/backup/` executa checkpoints contínuos na **branch `auto-backup`** de
um repositório clone empurrado para `danielvtroot/LINBLUX` (órfã — não guarda
história de `main`). Política: allowlist explícita (sem `git add -A`),
limite **4 MiB por arquivo**, varredura de segredos por padrões conhecidos
(**não prova ausência**), sem links simbólicos, push sempre normal; push
pendente não protege (métricas `protected_files` e
`remote_protected_files` medem cobertura local e remota). `main` e index Git
intocados.

```sh
python3 tools/backup/backup.py status
python3 tools/backup/backup.py run [--once]
python3 tools/backup/backup.py verify
python3 tools/backup/backup.py service install --backend auto
```

Serviço via systemd de usuário (`packaging/systemd/linblux-backup.service.in` →
`~/.config/systemd/user/linblux-backup.service`) ou cron `@reboot` + watchdog
por minuto. Config: `tools/backup/config.json` (override em
`~/.config/linblux/backup.json`); env: `LINBLUX_BACKUP_CONFIG`,
`LINBLUX_BACKUP_STATE`, `LINBLUX_BACKUP_PROJECT`. Estado em
`~/.local/state/linblux-backup/` (work/, state.json, backup.lock, daemon.log,
watchdog.log). Testes: `python3 -m unittest discover -s tests/backup -p
'test_*.py'`.

Recuperação de arquivo: no clone isolado, `git show auto-backup:src/...`;
restauração integral em outra máquina vem por clone da branch `auto-backup`.

### 20.2 Sincronização manual

`scripts/checkpoint.py --files A B C` (commit por arquivos explícitos) e
`scripts/sync-safe.sh` (push seguro) descritos em §18.3. Conferência
`git ls-remote origin main` × `git rev-parse HEAD` antes do push.

### 20.3 Backup de dados privados

- **Progresso por experiência** fica fora do repo:
  `$XDG_STATE_HOME/linblux/progress`; incluir na cópia externa privada
  juntamente com as instalações correspondentes. GitHub não protege esses
  dados.
- **Padrão personalizado da engine** vive em
  `~/.local/share/linblux/engine-defaults`: versione a pasta completa
  (index.json + revisions/) em backup privado.
- **Experiências da biblioteca** (`~/.local/share/linblux`) precisam de
  backup externo pessoal — não confundir com o repositório `auto-backup`.

### 20.4 Vulnerabilidades

LINBLUX Alpha executa experiências/scripts locais; sem promessa de isolamento
contra código hostil nem infra multiplayer. Reporte vulnerabilidade pelo canal
privado do GitHub (**Report a vulnerability**) se habilitado, ou por issue
pedindo canal privado — sem publicar exploits/segredos/dados privados.
Executar somente experiências de origem confiável: a API Lua acessa
capacidades locais da engine.

## 21. Limitações conhecidas

Sem esconder problemas, o que ainda NÃO existe ou está incompleto:

- **Multiplayer, contas, servidor, publicação/descoberta online** — nada
  implementado; nenhuma entrada de rede nesta Alpha.
- **Malhas arbitrárias, terreno, deformação por esqueleto diferente do R6**;
  animação de seis juntas é o escopo atual.
- **Distribuição binária pública**: bloqueada por FFmpeg (a build atual
  declara `GPL version 2 or later`) e pendente de componentes ainda
  incompletos; sem tag/Release publicada; nenhum download aprovado. Sem
  validação em outra distribuição Linux, GPU física ou driver de áudio.
- **Áudio**: sem espacialização, sem mixer por categoria; playback real em
  hardware não validado.
- **Inventário**: sem persistência automática entre sessões; sem
  StarterPack/StarterItems; sem transferência multiplayer.
- **UI Lua**: sem layout automático, texto editável, listas/scroll/drag,
  fontes por elemento, imagens animadas, máscaras ou gradientes.
- **Física**: 1 corpo por Part (sem formas compostas por peça), sem
  material reativo real, sem ragdoll; `NpcSystem` sem navmesh triangulada,
  saltos entre plataformas ou desvio dinâmico de agentes.
- **Precisão**: encaixe/borda/vértice somente em caixas; esferas não
  podem caixas aproximar.
- **Undo**: 128 operações / 32.768 poses; sem undo estrutural de topologia
  (criar/eliminar/agrupar não têm desfazer aprovado).
- **Studio**: sem importador FBX/glTF/malha externa. EditorGUI ImGui é
  frontend de depuração, não é o oficial.
- **Estabilidade do WIP ativo** (superfície quadriculada, tema monocromático,
  ícones Lucide, barra de vida) ainda em ajuste: funcional no código atual
  (sujeito aos testes); a aparência visual pode mudar antes do commit.
- **Latência de disco**: em HDD mecânico degradado não há garantias
  (persistência privada e empacotamento incluem fsync, mas o hardware limita).

## 22. Roadmap

Itens futuros, sem datas prometidas — nenhum é anunciado como pronto:

1. Consolidar a engine e a estabilidade do Studio.
2. Melhorar renderização, materiais, sombras e desempenho.
3. Tornar construção e edição mais intuitivas.
4. Ampliar personagens e animações.
5. Importar malhas e recursos 3D.
6. Criar um sistema de terreno.
7. Expandir e documentar a API Lua.
8. Melhorar empacotamento e compatibilidade Linux.
9. Desenvolver multiplayer com servidor autoritativo.
10. Introduzir identidade, autenticação e contas.
11. Implementar replicação de mundo e experiências compartilhadas.
12. Criar publicação e descoberta de experiências.
13. Permitir colaboração entre criadores.
14. Desenvolver um ecossistema de extensões e ferramentas.

Publicação binária pública depende de concluir obrigações de redistribuição
e validar em outra instalação Linux (§21).

## 23. Histórico resumido

Marcos que explicam decisões atuais (fonte: históricos e Git; não reproduz
relatórios anteriores por inteiro):

| Data | Marco |
| --- | --- |
| 2026-10-19 | Decisões de base D001–D004: C++17 + raylib, Bullet com passo fixo 1/60 e gravidade −98,1, CMake por módulos, dependências vendadas em `third_party/` |
| 2026-10-23 | Animação reprodutível/determinística (Euler ZYX, crossfade 0,12 s; validação de 4612 checks) |
| 2026-10-24 | Migração loblox → LINBLUX concluída (merge `5c68449`); identificadores de formato legados preservados |
| 2026-10-25 | ATLAS 2.0 + NPCs: NavGrid, ataques com arco de braços, dano/vida reais,`partMotion` liberando atrito sob comando |
| 2026-10-27 | Auditoria do motor 3D: 12 causas catalogadas; decisão **"evoluir raylib, não migrar"**; fix do rig físico cápsula 5,8 u |
| 2026-10-28 | Precisão de construção por face (SAT 15 eixos, escala com Alt centrada, arcos de rotação, oclusão de faces traseiras) |
| 2026-10-30 | Preparação Alpha 0.1.0: limpeza do repositório, candidato Linux (`source_dirty`), galeria de mídia, `ATLAS 2.1` com política HDD-Safe |
| 2026-10-31 | Verificação de backup externo via API GitHub (497 arquivos de `main`) |
| 2026-11 | Trabalho em andamento (não commitado): superfície quadriculada de construção, tema monocromático, ícones Lucide, HUD de vida em barra; consolidação da documentação em `README.md` + `LINBLUX.md` |

Planos de sessão e auditorias seguem preservados em `docs/relatorios/`
(parcialmente rastreados; os planos Studio 2.0 / Mega Update 3D e as auditorias
B1–B24/A1–A12 estão fora do Git, somente no disco — não são documentação de
distribuição).

## 24. Licenças e créditos

| Componente | Licença | Onde / fonte |
| --- | --- | --- |
| Código LINBLUX | MIT | `LICENSE` (© 2026 Loblox Open-Source contributors) |
| raylib | zlib/libpng | dependência do sistema (pkg-config) |
| Bullet | zlib | dependência do sistema |
| Lua 5.4 | MIT | dependência do sistema |
| RmlUi 6.3 | MIT | FetchContent de commit fixado `ba95ffe8fb...`; aviso instalado |
| Dear ImGui | MIT | `third_party/imgui` (LICENSE.txt) |
| rlImGui | zlib | `third_party/rlimgui` (LICENSE) |
| stb_image | MIT / domínio público | `third_party/stb` (LICENSE, ORIGIN.txt) |
| nlohmann/json | MIT | `third_party/nlohmann` (LICENSE.MIT) |
| FFmpeg | LGPL/GPL conforme build | ligação dinâmica; **a build atual declara GPL v2 ou posterior** |
| Ícones Lucide | ISC | `tools/icons/generate_icons.py` → `assets/ui/icons/` |

Recursos autorais: `midia/logo.png` (logo oficial fornecido pelo autor),
`midia/careditfullhd.mp4` (vídeo oficial; direitos pendentes de
documentação antes de redistribuição), capturas e exemplos de
`docs/prints`/`assets/examples`. Imagens pessoais de nomes hash estão fora do
índice Git e da distribuição instalada (licenças não documentadas; histórico
antigo as contém; clone novo não traz essas imagens).

**Declaração de distribuição**: a Alpha publica **código e documentação**,
não binário aprovado. Avisos instalados cobrem ImGui, JSON, RmlUi, rlImGui e
stb; Bullet, Lua, raylib e FFmpeg são dependências do sistema e seus avisos
ficam a cargo da instalação/empacotamento. Distribuição binária ainda exige
revisar as obrigações GPL/avisos antes de qualquer lançamento público.
