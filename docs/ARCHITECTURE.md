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
  core/Config.h                AppConfig (FPS alvo, gravidade, passo fixo)
  objects/Instance.{h,cpp}     nó base: id, nome, classe, pai, filhos, deletável
  objects/Part.{h,cpp}         transformação, forma, cor, material, flags físicas
  objects/Workspace.{h,cpp}    serviço raiz da cena
  objects/Lighting.{h,cpp}     céu, ambiente, sol, brilho
  objects/Baseplate.{h,cpp}    Part especial, ancorada e protegida
  objects/SpawnLocation.{h,cpp} Part especial de spawn (sem personagem ainda)
  scene/Scene.{h,cpp}          posse de todos os objetos, hierarquia, seleção
  physics/PhysicsWorld.{h,cpp} mundo Bullet, corpos, raycast, passo fixo
  serialization/ProjectIO.{h,cpp} JSON v1, validação, escrita atômica
  rendering/EditorCamera.{h,cpp} câmera orbital, raio e projeção
  rendering/SceneRenderer.{h,cpp} céu, partes sombreadas, studs, seleção
  editor/Tool.h                enum ToolMode
  editor/Gizmo.{h,cpp}         mover/escalar/girar nos eixos
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
2. desenha a imagem e consulta hover;
3. aplica órbita/pan/zoom e o gizmo (raycast/planos);
4. clique esquerdo seleciona a Part via `PhysicsWorld::raycast`;
5. renderiza a cena + seleção + gizmo na render texture.

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

## 7. Formato de projeto (versão 1)

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

Só depois `scene.clearObjects()` é chamado e a cena é reconstruída. Erros
retornam mensagem legível e não deixam o arquivo em estado parcial.

### 7.2 Escrita atômica

`ProjectIO::save` grava `<arquivo>.tmp`, fecha o stream e faz
`std::filesystem::rename` sobre o destino (atômico em POSIX). Em caso de erro,
o temporário é removido e o arquivo anterior permanece intacto.

## 8. Testes

`tests/test_main.cpp` roda sem janela/OpenGL e cobre o núcleo:

- cena padrão, hierarquia, proteção da Baseplate;
- criação/remoção, nomes únicos, propriedades e clamp de tamanho;
- listener de remoção e limpeza de corpos;
- Anchored ↔ dinâmico, queda e repouso sobre a Baseplate;
- CanCollide (atravessar o piso);
- sincronização transformação ↔ corpo e raycast;
- redimensionamento sem duplicar corpo;
- round-trip JSON, escrita atômica, validação de arquivos inválidos;
- reconstrução de corpos após carregar.

## 9. Limitações conhecidas da arquitetura atual

- `Part` usa tipos da raylib (`Vector3`, `Color`) também no núcleo; isso reduz
  conversões, mas acopla o modelo à biblioteca de renderização. Uma separação
  total exigiria tipos próprios e uma camada de conversão.
- Seleção é única; não há undo/redo nem multi-seleção.
- Materiais não alteram rugosidade/reflexão (não há PBR).
- O gizmo escolhe o eixo mais próximo em espaço de tela; eixos que se cruzam
  podem exigir um clique mais preciso.
- A render texture é recriada somente ao redimensionar; não há resolução
  dinâmica para GPUs muito fracas.
