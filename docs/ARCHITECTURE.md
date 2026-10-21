# Arquitetura — Loblox Open-Source 0.3

## 1. Tecnologias

| Camada | Tecnologia | Versão usada |
| --- | --- | --- |
| Linguagem | C++ | C++17 |
| Build | CMake | 3.16+ |
| Janela/render | raylib | 6.2.0 (mínimo 5.5) |
| Interface | Dear ImGui (docking branch) + rlImGui | 1.93.1 WIP (vendorizado) |
| Física 3D | Bullet Physics | 3.24 |
| Persistência | nlohmann/json | header único vendorizado |
| Testes | CTest + suíte própria | — |

Nada é baixado em tempo de build ou execução: ImGui, rlImGui e JSON estão em
`third_party/`. A aplicação não acessa a rede.

## 2. Princípios

1. **A UI não calcula física.** `EditorUI` só altera propriedades dos objetos;
   `PhysicsWorld` traduz essas alterações para o Bullet.
2. **A física não depende de UI/render.** `physics/` inclui apenas Bullet,
   `Part` e `Scene`.
3. **Persistência não serializa ponteiros nem handles.** O JSON guarda ids e
   valores explícitos.
4. **Um corpo físico por Part**, sempre. Corpos são criados/removidos por
   eventos (criação, remoção, alternância de Anchored, alteração de forma), não
   por frame.
5. **Sem recursos gráficos recriados por frame.** A malha de studs e a render
   texture são cacheadas; a textura só é recriada quando o painel muda de
   tamanho.

## 3. Módulos

```
src/
  main.cpp                     entrada: cria Application e roda
  app/Application.{h,cpp}      janela, ImGui, cena, física, loop principal
  core/Math.h                  Vector3/Quaternion/Color helpers, clamp de tamanho
  core/Config.h                AppConfig (FPS alvo, gravidade, passo fixo, projeto)
  core/Diagnostics.{h,cpp}     eventos JSONL com rotação e correlação
  objects/Instance.{h,cpp}     nó base: id, nome, classe, pai, filhos, deletável
  objects/Part.{h,cpp}         transformação, forma, cor, material, flags físicas
  objects/Workspace.{h,cpp}    serviço raiz da cena
  objects/Lighting.{h,cpp}     céu, ambiente, sol, brilho
  objects/Baseplate.{h,cpp}    Part especial, ancorada e protegida
  objects/SpawnLocation.{h,cpp} Part especial de spawn (sem personagem ainda)
  scene/Scene.{h,cpp}          posse de todos os objetos, hierarquia, seleção
  physics/PhysicsWorld.{h,cpp} mundo Bullet, corpos, raycast, passo fixo
  serialization/ProjectIO.{h,cpp} JSON v1, validação, escrita atômica
  serialization/SceneLua.{h,cpp} subset declarativo Lua sem execução
  rendering/EditorCamera.{h,cpp} câmera orbital/livre, entrada, raio/projeção
  rendering/SceneRenderer.{h,cpp} céu, partes sombreadas, studs, seleção
  editor/Tool.h                enum ToolMode
  editor/Gizmo.{h,cpp}         picking, desenho e estado do arraste
  editor/GizmoLayout.{h,cpp}   posições screen-space compartilhadas desenho/hit test
  editor/GizmoMath.{h,cpp}     matemática pura de move/scale/rotate/snap
  editor/TransformSession.{h,cpp} owner do ciclo gizmo ↔ suspensão física
  editor/EditorUI.{h,cpp}      toolbar, toolbox, viewport, explorer, properties
tests/test_main.cpp            testes headless
```

### 3.1 Modelo de objetos

`Instance` guarda:

- `InstanceId` de 64 bits (estável, monotônico, nunca reutilizado);
- nome e classe (`Workspace`, `Lighting`, `Part`, `Baseplate`, `SpawnLocation`);
- ponteiro para o pai e lista de filhos;
- flag `deletable` (serviços e Baseplate inicial são protegidos).

`Part` adiciona:

| Campo | Tipo | Observação |
| --- | --- | --- |
| `shape` | Block / Sphere | esfera usa o menor eixo como diâmetro |
| `position` | Vector3 | centro geométrico |
| `rotation` | Quaternion | identidade por padrão |
| `size` | Vector3 | mínimo 0.2 por eixo |
| `color` | Color | RGBA |
| `material` | enum | SmoothPlastic, Plastic, Wood, Metal, Concrete, Neon |
| `anchored` | bool | massa 0 / corpo estático quando verdadeiro |
| `canCollide` | bool | contato físico quando verdadeiro |

`Workspace` contém todas as Parts (incluindo Baseplate/SpawnLocation).
`Lighting` guarda apenas configuração lida pelo renderizador a cada frame.

### 3.2 Posse e hierarquia

`Scene` é a única dona dos objetos (`std::vector<std::unique_ptr<Instance>>`).
Os ponteiros pai/filho são vínculos não-possuidores usados pela árvore do
Explorer. `Scene` mantém também:

- `byId`: mapa id → objeto (busca O(1));
- `parts`: lista de `Part*` para física/render (sem `dynamic_cast` por frame);
- `selection`: objeto selecionado (seleção única nesta etapa);
- listeners de remoção.

Ao remover, `Scene` dispara os listeners **antes** de destruir o objeto. A
física registra um listener e destrói o corpo correspondente. Isso elimina
corpos órfãos e ponteiros pendentes em `btRigidBody::setUserPointer`.

O loader cria objetos com o id vindo do arquivo quando ele não conflita com os
serviços (ids 1 e 2 são Workspace e Lighting por convenção da cena). O contador
`nextId` é elevado para nunca reutilizar um id carregado.

## 4. Integração com a física

`PhysicsWorld` possui exatamente um `btRigidBody` + `btCollisionShape` por
`Part`, em `unordered_map<InstanceId, BodyEntry>`.

| Evento no editor | Ação no Bullet |
| --- | --- |
| criar Part | cria shape e corpo (dinâmico se `!anchored`) |
| mudar posição/rotação | `setWorldTransform`, zera velocidade, ativa corpo |
| mudar tamanho | **redimensiona o shape existente**, recalcula massa/inércia e AABB |
| alternar `anchored` | reconstrói o corpo (muda massa estática/dinâmica) |
| alternar `canCollide` | liga/desliga `CF_NO_CONTACT_RESPONSE` |
| excluir Part | remove e destrói corpo, shape e motion state |
| `syncScene` | cria corpo se a Part ainda não tiver um |
| editar Part dinâmica | suspende temporariamente o body existente, sem duplicá-lo |

Detalhes:

- Corpo ancorado: massa 0, `CF_STATIC_OBJECT`, `DISABLE_DEACTIVATION`.
- Corpo dinâmico: massa = volume × densidade (mínimo 0.1), damping baixo,
  CCD ligado para não atravessar a Baseplate.
- Gravidade: −98.1 unidades/s² (1 unidade ≈ 10 cm; ver `docs/DECISIONS.md`).
- Passo fixo de 1/60 s, no máximo 5 subpassos, `dt` limitado a 0.1 s.
  A simulação não depende do FPS de renderização.
- `SceneRenderer` nunca lê o Bullet; `PhysicsWorld::syncFromBodies` copia
  posição/rotação dos corpos dinâmicos para as Parts antes do desenho.

As alterações do editor são propagadas por flags em `PartDirty`
(`transform`, `shape`, `anchored`, `collide`). A UI só chama setters; a física
consome as flags no próximo `simulate()`, evitando reconstruções por frame e
mantendo um único caminho de sincronização.

Durante um drag de gizmo, `setPartEditing(id, true)` coloca o corpo dinâmico em
`DISABLE_SIMULATION`, zera velocidades e mantém o mesmo body/collider. A UI
continua alterando o `Part`; o sync atualiza transform e AABB sem ativar a
simulação. Ao terminar ou cancelar, o corpo volta a `ACTIVE_TAG` com velocidades
zeradas e a gravidade age no próximo passo.

### 4.1 Baseplate

A Baseplate é uma `Part` real (Block 160 × 1 × 160, centro em y = −0.5), com
corpo estático no Bullet, `anchored = true`, `canCollide = true` e
`deletable = false`. Ela é registrada no mundo físico como qualquer outra peça;
o topo fica em y = 0. `Part::setAnchored(false)` e `setCanCollide(false)` são
ignorados para essa classe, e a UI desabilita ambos os controles.

## 5. Ciclo principal

```
InitWindow + rlImGuiSetup
scene.buildDefaultScene()          // Workspace > Baseplate, SpawnLocation; Lighting
physics.attach(scene)              // listener + corpos iniciais
loop:
    dt = GetFrameTime()
    physics.simulate(scene, dt)    // sync flags -> stepSimulation -> sync bodies
    BeginDrawing()
        rlImGuiBegin()
        ui.draw()                  // toolbar, toolbox, viewport, explorer, properties
        rlImGuiEnd()
    EndDrawing()
```

`ui.draw()` do viewport:

1. ajusta a `RenderTexture2D` ao tamanho do painel;
2. desenha a imagem e consulta hover/foco; campos ativos e texto bloqueiam o
   teclado de navegação;
3. processa câmera livre quando a viewport tem foco; RMB captura cursor,
   MMB faz pan e a roda aproxima do foco;
4. resolve o handle do gizmo antes da seleção; drag em eixo/plano/anel usa raio
   da viewport, interseções geométricas e snapshot inicial da transformação;
5. clique esquerdo sem handle seleciona a Part via `PhysicsWorld::raycast`;
6. renderiza a cena + seleção + gizmo na render texture.

A física roda antes do desenho, então as posições exibidas no mesmo frame já
refletem a simulação.

## 6. Renderização e desempenho

- Céu: gradiente 2D com a cor de `Lighting`; sem céu procedural.
- Iluminação: Lambert por face/quad calculado na CPU durante o desenho
  (sem shaders customizados, sem luzes dinâmicas, sem sombras).
- Baseplate: caixa sombreada + **uma malha de studs** construída uma vez por
  tamanho e desenhada com `DrawMesh`. São ~400 studs (espaçamento 8) em uma
  malha de ~25 mil vértices, não 400 objetos por frame.
- Grade: linhas a cada 16 unidades sobre o topo da Baseplate.
- Esferas: malha facetada imediata (7 anéis × 12 fatias) com cor por quad.
- Materiais: presets de cor/tonalidade aplicados antes do sombreamento.
- Interface: ImGui com fonte padrão, sem ícones em fonte externa (os ícones da
  toolbar são desenhados com primitivas do ImGui).
- Sem pós-processamento, sem MSAA, sem texturas de imagem.
- Limite de FPS configurável (`AppConfig::targetFps`, padrão 60).
- A viewport só recria a render texture quando o tamanho muda.

## 7. Formato de projeto nativo (JSON versão 1)

```json
{
  "format": "loblox-project",
  "version": 1,
  "nextId": 6,
  "objects": [
    {"id": 1, "class": "Workspace", "name": "Workspace", "parent": 0},
    {"id": 2, "class": "Lighting", "name": "Lighting", "parent": 0,
     "skyColor": [120,180,240,255], "ambientColor": [150,150,160,255],
     "sunDirection": [-0.45,-1,-0.35], "sunIntensity": 1.0, "brightness": 1.0},
    {"id": 3, "class": "Baseplate", "name": "Baseplate", "parent": 1,
     "shape": "Block", "position": [0,-0.5,0], "rotation": [0,0,0],
     "size": [160,1,160], "color": [163,162,165,255],
     "material": "SmoothPlastic", "anchored": true, "canCollide": true},
    {"id": 4, "class": "SpawnLocation", "name": "SpawnLocation", "parent": 1,
     "shape": "Block", "position": [0,0.5,0], "rotation": [0,0,0],
     "size": [12,1,12], "color": [0,162,255,255],
     "material": "SmoothPlastic", "anchored": true, "canCollide": true}
  ]
}
```

- `rotation` é gravada em graus Euler (XYZ), legível e editável; internamente
  a Part usa quaternion.
- A ordem dos objetos é a ordem de desenho do Explorer (serviços primeiro).
- `parent` referencia ids; 0 indica raiz. Parts só podem pertencer ao
  Workspace; Lighting e Workspace ficam na raiz.
- A Baseplate é forçada a `anchored = true` na carga (é estrutural).

### 7.1 Validação

Antes de tocar na cena, o loader valida:

- `format` e `version` (1; versões maiores são rejeitadas);
- cada objeto é um objeto JSON com `id` positivo, `class` conhecida e nome;
- ids únicos (inclusive contra os ids de serviço da cena atual); exatamente um Workspace e um Lighting;
- pais coerentes (serviços na raiz, Parts no Workspace);
- `shape` e `material` conhecidos; booleanos presentes;
- posições/rotações finitas, tamanho ≥ 0.2, canais de cor 0..255.

Em vez de limpar a cena antes de reconstruir, o loader constrói uma `Scene`
candidata completa, aplica os objetos e chama `Scene::validate`. Somente se tudo
passar, `Scene::replaceContentsFrom` troca o conteúdo em O(1). Os listeners de
remoção permanecem no `Scene` de destino e são avisados sobre as Parts antigas,
para que a física não conserve corpos órfãos; o chamador reconstrói os corpos da
cena nova (`EditorUI` faz isso via `onSceneLoaded`). Erros retornam mensagem
legível e não alteram a cena, a seleção ou os corpos atuais.

### 7.2 Escrita atômica

`ProjectIO::save` valida as invariantes da cena, serializa e valida o JSON
antes de tocar no destino. `writeFileAtomically` cria um temporário único no
mesmo diretório (`mkstemp`), escreve em loop com `EINTR`, sincroniza com
`fsync`, fecha e substitui com `rename` POSIX atômico. Depois do rename, abre o
diretório e faz `fsync` quando o filesystem suporta. Falha de criação, gravação,
sincronização, fechamento ou rename remove o temporário e preserva o destino
anterior. Ao substituir um arquivo regular, mantém as permissões dele. O
exportador Lua usa o mesmo escritor, eliminando o `.tmp` fixo que podia colidir
com outra execução. Esse caminho é específico ao Linux/POSIX, plataforma
suportada atualmente; `EINVAL`/`ENOTSUP` no `fsync` do diretório são tolerados.

## 8. Formato declarativo de cena (Lua v1)

`serialization/SceneLua` importa/exporta uma tabela literal Lua com
`format = "loblox-scene"`, `version = 1`, `nextId` e `objects`. Preserva IDs,
parent, Workspace, Lighting, Baseplate, SpawnLocation, shape, transform, cor,
material, Anchored e CanCollide. Lighting inclui céu/ambiente, direção do sol,
intensidade e brilho.

O parser aceita somente comentários de linha, `return`, tabelas nomeadas e
sequenciais, strings, números finitos e booleanos. Não aceita expressões,
índices computados, funções, chamadas, `require`, `load` ou código após o valor.
Há limites de 16 MiB, profundidade e quantidade de valores. Classes e campos
são whitelisted; os dados são convertidos ao esquema versionado e passam pela
validação do JSON antes de substituir a cena. Nenhum runtime Lua/Luau é
invocado. JSON v1 segue sendo o formato nativo de projeto.

## 9. Testes

`tests/test_main.cpp` roda sem janela/OpenGL e cobre o núcleo:

- cena padrão, hierarquia, proteção da Baseplate;
- criação/remoção, nomes únicos, propriedades e clamp de tamanho;
- listener de remoção e limpeza de corpos;
- Anchored ↔ dinâmico, queda e repouso sobre a Baseplate;
- CanCollide (atravessar o piso);
- sincronização transformação ↔ corpo e raycast;
- redimensionamento sem duplicar corpo;
- round-trip JSON, escrita atômica, validação de arquivos inválidos;
- carga transacional: falha não muda cena/física e sucesso remove corpos antigos;
- reconstrução de corpos após carregar;
- invariantes de cena/física, rejeição de NaN/infinito e diagnóstico JSONL;
- sessão de transformação: clique consumido, confirmação, cancelamento por
  foco/ferramenta/API e liberação do corpo;
- inicialização sem display e parse de `--project`/`--help`;
- matemática de eixos/planos, snap, face oposta fixa na escala, mínimo e rotação;
- navegação livre, delta time, limite de pitch, zoom e foco;
- suspensão/restauração de corpos durante edição;
- round-trip Lua de classes, IDs, hierarquia, Lighting e propriedades;
- rejeição de código, classes/campos desconhecidos e nextId inválido sem mudar
  a cena atual.

## 10. Limitações conhecidas da arquitetura atual

- `Part` usa tipos da raylib (`Vector3`, `Color`) também no núcleo; isso reduz
  conversões, mas acopla o modelo à biblioteca de renderização. Uma separação
  total exigiria tipos próprios e uma camada de conversão.
- Seleção é única; não há undo/redo nem multi-seleção.
- Materiais não alteram rugosidade/reflexão (não há PBR).
- Picking de eixos/anéis usa tolerância em pixels e planos usam o centro
  projetado; cruzamentos visuais próximos ainda exigem um clique preciso.
- Sphere continua usando `btSphereShape` e raio uniforme; Scale mantém a
  semântica uniforme da primitiva (não há elipsoides).
- A render texture é recriada somente ao redimensionar; não há resolução
  dinâmica para GPUs muito fracas.

## 11. Redesign da interface (2026-10-20)

- Dear ImGui continua como única camada de interface e o docking nativo organiza
  Toolbox à esquerda, Viewport ao centro, Explorer e Properties à direita.
- O tema claro usa tokens centralizados no setup de `Application`: superfícies
  claras, texto escuro, bordas discretas e azul para ações/seleção. Fonte sans
  local é carregada quando disponível, com fonte vendorizada como fallback.
- Barra de menus File/Home/Model/View e ribbon Home/Model/View operam sobre os
  mesmos comandos da cena. Save/Open continuam usando `ProjectIO`; inserção,
  seleção e propriedades chamam o modelo e a física existentes.
- O arquivo `loblox_imgui.ini` guarda o docking entre sessões. Se não existir
  layout restaurável, `EditorUI::buildDockLayout` monta o padrão; o nó existente
  é preservado em vez de ser reconstruído a cada inicialização.
- A busca local do Toolbox filtra Block, Sphere e SpawnLocation. A busca do
  Explorer percorre a hierarquia real, sem manter uma lista paralela de objetos.

Na etapa do redesign visual descrita acima, esses módulos não foram alterados.
Na etapa de câmera/gizmos/SceneLua, `PhysicsWorld` ganhou suspensão temporária
de corpos durante edição e `ProjectIO` passou a validar IDs e faixas numéricas
antes de reconstruir a cena; `Scene` e `Part` mantiveram seus contratos.

## 12. Entrada de câmera e transformações

`EditorCamera` separa movimento livre (WASD/QE, Shift e delta time) da órbita
com RMB. Orbit captura o cursor via raylib/GLFW, usa delta relativo e restaura a
posição ao liberar; perda de foco, cancelamento e destruição também liberam.
Wheel faz zoom ao ponto focal, F enquadra e MMB desloca o alvo nos eixos da
câmera. A UI consulta foco/hover do ImGui e bloqueia navegação durante edição
de texto ou arraste de gizmo.

`Gizmo` identifica cada eixo, plano, face ou anel e guarda snapshot de
transformação no começo do arraste. `GizmoMath` calcula deltas/snap sem
dependência de UI. Move restringe ao eixo/plano escolhido; Scale desloca o
centro pela metade da variação assinada para manter a face oposta fixa; Rotate
usa ângulo entre vetores projetados no plano normal ao eixo. O espaço pode ser
global ou local. Sphere permanece uniforme devido a `btSphereShape`.

Os seis controles de Scale partem do limite de suporte da caixa orientada e
recebem offset calculado pela profundidade de projeção. Como duas alças podem
projetar no mesmo ponto (por exemplo, câmera próxima a 45° entre eixos), seus
ícones são afastados em tela com linhas-guia; o hit test usa exatamente esses
mesmos centros visuais. A escala Global escolhe a dimensão local mais alinhada
ao eixo mundial e aplica o deslocamento projetado com seu sinal. A
implementação anterior dividia pelo alinhamento e perdia seu sinal ao
determinar o lado local: isso amplificava o arraste e fazia algumas faces
diminuírem quando puxadas para fora. Transformar uma caixa rotacionada em
escala global exata exigiria shear, que `Part` não representa; o modo mantém
caixa orientada e escala apenas uma dimensão local por arraste.

Ao manipular Part dinâmica, `PhysicsWorld` suspende a integração do corpo
existente; a atualização in-place mantém um único collider e atualiza seu AABB.
Ao encerrar, o corpo é reativado sem velocidade residual do estado suspenso.

### 8.1 Identidade e escala unilateral

`GizmoMath::HandleId` é a identidade estável do controle (`XPositive`,
`XNegative`, planos, eixos e anéis). O picking dos seis controles de escala
usa os mesmos centros e retângulos de 24×20 pixels que o overlay desenha; o id
escolhido é mantido durante todo o drag. `None` não possui transformação de
fallback. Move e Rotate também mantêm ids explícitos (`XAxis`, `XYPlane`,
`RotateX`, etc.) do hover ao drag, e o realce desenhado consulta esse mesmo id.

Todos os gizmos são renderizados em um passe x-ray local: o depth test é
desativado somente enquanto os manipuladores são desenhados e restaurado antes
da UI continuar. Isso permite ver e selecionar as alças mesmo atravessando a
Part sem alterar a oclusão normal da cena. Scale usa overlay 2D com a mesma
política.

O drag guarda um snapshot inicial. Para uma face local, o deslocamento assinado
é projetado no eixo selecionado, altera somente aquela dimensão e desloca o
centro por metade da variação no eixo local, mantendo fixa a face oposta. Em
Global, a dimensão local mais alinhada ao eixo mundial recebe a projeção
assinada do deslocamento no eixo local; o código anterior dividia pela
projeção e descartava o sinal do alinhamento ao escolher a face local,
amplificando o drag e invertendo algumas faces. Geometria de escala inválida
ou raio sem interseção não altera o snapshot.

As âncoras vêm do suporte atual da caixa orientada. Os ícones 2D são afastados
quando suas projeções coincidem e são desenhados em passe de overlay com depth
test desligado apenas durante esse desenho, para que a própria Part não os
oculte. A seleção usa exatamente os limites desses ícones. Sphere continua
uniforme por causa do `btSphereShape`.


## 13. Sessão de transformação, invariantes e inicialização

`Gizmo::update` recebe `GizmoInput` sintético (posição, botões, foco e Shift) e
retorna `GizmoUpdateResult` com `started`, `ended`, `cancelled`, `consumed` e o
`partId` da sessão mesmo depois de limpar o estado interno. `TransformSession`
é o único dono da chamada e aplica o resultado à física: ao iniciar, suspende o
corpo dinâmico; ao terminar, libera exatamente o corpo daquela sessão. A UI não
mantém um id de edição paralelo. Escape, perda de foco, mudança de ferramenta,
seleção, abertura de modal, exclusão e destruição passam por cancelamento/
liberação explícitos. O `Gizmo` restaura o snapshot apenas quando o cancelamento
pede restauração; soltar o botão confirma o transform atual.

A prioridade é resolvida em uma única ordem por frame: o gizmo vê a entrada
primeiro; se consumiu o clique, o raycast de seleção não roda; a câmera recebe
`interactionBlocked` enquanto houver arraste ou handle sob o cursor e não
processa RMB/MMB/roda/WASD. A navegação continua possível fora desses estados.

`Scene::validate` verifica serviços, índice id→objeto, unicidade, hierarquia,
seleção, Parts no Workspace, transform finito, tamanho mínimo e quaternion
normalizado. `PhysicsWorld::validate` verifica correspondência exata entre Parts
e corpos, `getUserPointer`, corpos órfãos e se o conjunto de edição está contido
nos corpos. `Part` rejeita posição/rotação/tamanho não finitos em vez de
propagar NaN ao Bullet ou ao renderer. Essas verificações rodam depois de carga,
exclusão e fim de arraste; violações viram evento de diagnóstico e mensagem de
erro, sem tentar “consertar” silenciosamente a cena.

`Application` valida `IsWindowReady` antes de inicializar ImGui/raylib. Sem
display, retorna falha controlada. Na inicialização, `--project` tem
precedência; sem a flag, o caminho padrão é tentado apenas se for arquivo
regular. Falhas mantêm a cena padrão e são mostradas na UI/stderr/log. O logger
`core/Diagnostics` grava JSONL com timestamp UTC, nível, subsistema, evento,
correlação, entidade, arquivo, duração, resultado, erro e contexto; rotação
ocorre em 1 MiB. Source não é registrado automaticamente; prints/erros de Lua
registram o texto fornecido pelo script. Não imprima segredos nos scripts.

# Extensões Studio 2

Este documento conserva o histórico da arquitetura inicial. A implementação
atual acrescenta hierarquia Folder/Model, JSON v3 e Lua 5.4 executável em Play,
além do runtime R6. Contratos e limites atuais: [Studio 2](STUDIO2_GUIDE.md) e
[R6/Play](R6_PLAY_GUIDE.md). A importação SceneLua continua declarativa e não
executa os arquivos importados; a VM de gameplay é um subsistema separado.
