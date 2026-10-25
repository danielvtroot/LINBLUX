# Módulo `scene`

**Responsabilidade:** dono da árvore de objetos e dos serviços `Workspace` e
`Lighting`, com IDs estáveis, hierarquia, duplicação, remoção, validação e
seleção autoral.

**Fonte:** `src/scene/Scene.{h,cpp}`.

**Entradas:** criações de Part/Baseplate/Spawn/serviços, operações de parentesco,
duplicação e remoção, listeners de remoção e cenas de carga.

**Saídas:** objetos e Parts indexados, serviços, seleção, `GameAssets` da cena,
IDs e validação. É a fonte da cena autoral; `RuntimeSession` usa uma cópia.

**APIs principais:** `buildDefaultScene`, `createPart`, `createBaseplate`,
`createSpawnLocation`, `duplicatePart`, `reparent`, `remove`, `clearObjects`,
`replaceContentsFrom`, `validate`, `pickPart`, `addRemovalListener`.

**Invariantes:** uma instância tem um pai; IDs não se repetem; serviços são
protegidos; `replaceContentsFrom` só troca depois de validar; listeners são
avisados sobre Parts removidas para a física não guardar corpos órfãos; a cena
não conhece ImGui/OpenGL/Bullet diretamente.

**Dependências:** `objects`, `game::GameAssets`/Rig, `core`. `PhysicsWorld`
registra-se como listener; serialização reconstrói a cena.

**Ciclo de vida e erros:** `Scene` não é copiável; objetos são `unique_ptr`;
`clearObjects`/`replaceContentsFrom` têm semântica de reset transacional;
operações inválidas retornam `false`/mensagem e não alteram a árvore.

**Testes:** `linblux_tests`, `linblux_manipulation`, `linblux_spawn`,
`linblux_defaults`, `linblux_engine_default_lifecycle`.

**Como estender:** adicione criação/remoção com validação e listener; mantenha
IDs e hierarquia; serialize via `ProjectIO`, não diretamente no objeto. Se mudar
o formato, atualize `serialization` e testes de ciclo de vida.

**Limites:** a cena ainda contém seleção/listeners usados pela autoria e
`GameAssets`; não é um ECS. Rigs não são Instances filhas. Multiplayer/Server
não existe.
