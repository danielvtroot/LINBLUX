# Módulo `objects`

**Responsabilidade:** modelo de objetos da cena — `Instance`, `Part`,
`Workspace`, `Lighting`, `Baseplate` e `SpawnLocation` — com hierarquia, IDs e
propriedades.

**Fonte:** `src/objects/{Instance,Part,Workspace,Lighting,Baseplate,SpawnLocation}.{h,cpp}`.

**Entradas:** criação por `Scene`, propriedades de transformação/material,
hierarquia pai/filho e flags de física.

**Saídas:** árvore de objetos, partes, superfícies, atributos, transforms e
estado de colisão/anchor.

**APIs principais:** `Instance::attachChild/detachChild/setAttribute`,
`Part::setTransform/setSize/setShape/setColor/setMaterial`,
`Part::setAnchored/setCanCollide`, `Workspace`, `Lighting`.

**Invariantes:** IDs estáveis; uma instância tem no máximo um pai; nomes únicos
são responsabilidade de `Scene`; `PartDirty` sinaliza atualização de física;
serviços protegidos não podem ser removidos; `Transform` mantém posição/rotação/
tamanho coerentes.

**Dependências:** `core` (math/geometria) e raylib para tipos de cor/vetor. Não
depende de Bullet diretamente; `PhysicsWorld` traduz `Part` em corpos.

**Ciclo de vida e erros:** objetos pertencem a `Scene` via `unique_ptr`;
`attach/detach` não destroem sozinhos. Operações inválidas retornam `false` ou
são rejeitadas por `Scene::validate`.

**Testes:** `linblux_tests`, `linblux_manipulation`, `linblux_spawn` e testes de
serialização/defaults.

**Como estender:** adicione propriedades com validação e serialização; marque
`PartDirty` quando a física precisar reagir. Não adicione dependência de UI ou
de física concreta.

**Limites:** não há componentes ECS genéricos; o modelo é hierárquico e
orientado a objetos. Rigs são dados de `game`, não `Instance` filhas.

**Attachments:** componente folha de Part/Modelo, com transformação local
validada, ID estável e pose global calculada por Scene. Lua usa referências
existentes; clone/remoção são os da hierarquia. JSON v5 preserva quaternions e
recusa documentos inválidos antes de substituir a cena. Testes: Attachments
em linblux_studio, exemplo attachments.lua no Player, pacote/defaults em
linblux_experiences e fluxo X11 em linblux_models_ui.
