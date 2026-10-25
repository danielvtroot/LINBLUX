# Módulo `physics`

**Responsabilidade:** ponte entre `Scene`/`Part` e Bullet: um corpo por Part,
formas de colisão, passo fixo, sincronização, raycast e validação.

**Fonte:** `src/physics/PhysicsWorld.{h,cpp}`.

**Entradas:** `Scene`, Parts e suas flags dirty, gravidade, passo fixo, raio/
direção de consulta e a cápsula do runtime.

**Saídas:** corpos físicos, posições/velocidades/rotações, colisões, raycast e
transformações sincronizadas de volta para Parts.

**APIs principais:** `attach`, `rebuildAll`, `addPart`, `removePart`,
`applyChanges`, `simulate`, `syncFromBodies`, `raycast`, `validate`,
`collisionGeometry`, `setPartEditing`.

**Invariantes:** exatamente um corpo por Part; não cria corpos por frame; a
cena deve viver mais que `PhysicsWorld`; `editingParts_` deve estar contido nos
corpos; passo fixo de 1/60 com limite de subpassos; física é autoritativa para
o corpo do personagem.

**Dependências:** Bullet, `objects`, `core/Geometry`, `scene`. Não depende de
ImGui/OpenGL. `RuntimeSession` é amigo explícito para a cápsula única.

**Ciclo de vida e erros:** `attach` registra listener e cria corpos; `clearBodies`
/ destrutor liberam corpos e formas; `validate` retorna `bool` + mensagem.
Não há thread própria.

**Testes:** `linblux_manipulation`, `linblux_tests`, `linblux_game`,
`linblux_spawn` e testes de runtime que exercitam queda/colisão.

**Como estender:** qualquer nova forma precisa de `createShape`, atualização em
`updateShape` e validação de correspondência. Mantenha o passo determinístico e
não consulte UI/render dentro da simulação.

**Limites:** não há física de juntas/ragdoll, veículos ou terreno avançado;
materiais/fricção são propriedades simples; CCD é complementado por sweep
manual para o personagem.

Consultas Lua de caixa orientada/esfera usam `RuntimeSession::overlap`: probe
transitório não registrado no mundo, `contactTest`, resultados por ID e filtros
de sensor/jogador/peça. O callback apenas coleta IDs; Lua recebe cópia após a
enumeração. Cota compartilhada por quadro e máximo 256 resultados.

`Part::BodyProperties` valida densidade/atrito/restituição/amortecimento. Dirty
`body` atualiza massa/inércia e parâmetros no corpo existente, preservando
velocidades e suspensão por gizmo. `bodyMass` consulta o Bullet. JSON opcional
`body`/cenas antigas e duplicação mantidos por ProjectIO/Scene.
