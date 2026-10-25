# 20 — NPCs programáveis e navegação 3D

Data: 25/10/2026. Base: `48dba40`. Escopo: backend C++ (cena, física, runtime
Lua, persistência), exemplos Lua e testes. Nenhum arquivo de frontend foi
alterado: RmlUi, ImGui, HUD, menu do Player, tema, locales e layout permanecem
intactos.

## Incrementos

| Incremento | Estado | Entrega |
| --- | --- | --- |
| 1 — Controle de NPC por Lua | IMPLEMENTADO, TESTADO | `NpcProperties` na Part (dados serializados), `game::NpcSystem`, namespace `linblux.npc` (criar, anexar, desanexar, velocidade, destino, parar, consultas). |
| 2 — Navegação e caminhos | IMPLEMENTADO, TESTADO | `game::NavGrid`: grade XZ amostrada da física real, A* 8-vizinhos sem cortar cantos, suavização por linha de visão, destino inacessível explícito, degrau limitado a 1,2. |
| 3 — Patrulha e perseguição | IMPLEMENTADO, TESTADO | `npc_patrulha.lua` e `npc_perseguidor.lua`, ambos executados pelo Studio e pelo Player. |
| 4 — Vida, dano e monstro | IMPLEMENTADO, TESTADO | Vida/max/vida/respawn por NPC, morte e `npc_monstro.lua` com ataque intervalado usando a vida real do jogador. |
| 5 — Persistência, exemplos e testes | IMPLEMENTADO, TESTADO | Campo `npc` no JSON do projeto, três exemplos reais, testes headless e validação no Player. |

Publicação: os incrementos 1–4 compartilham os mesmos arquivos de implementação
(`Npc`, `Navigation`, `Runtime`, `PhysicsWorld`, `LuaRuntime`). Foram validados e
publicados como uma unidade funcional, com os artefatos de exemplo, teste e
documentação na sequência.

Commits (push normal em `origin/main`):

- `5f29424d29f20ddecec53fe6557db9ce9f2b324e` — implementação (cena, física,
  navegação, runtime Lua, persistência, CMake).
- `1fad7e30786040412d5c56909e606f03c01df901` — exemplos, testes, docs e este
  relatório; hash confirmado em `origin/main`, acima de `48dba40`.

Após o envio, uma linha redundante (`error.clear()` em `RuntimeSession::start`)
foi removida; `linblux_studio`, `linblux_game` e `linblux_lua_player_validation`
foram reexecutados e passaram 3/3 com o código final.

## API Lua adicionada (namespace `linblux.npc`)

- `create{name, position, size, color, material, shape, parent, script,
  script_name, speed, max_health, respawn_delay, turn_speed, enabled}` → Part.
- `attach(peca, opcoes?)`, `detach(peca)`.
- `move_to(peca, destino)` → `true` quando existe rota; `false` para destino
  inacessível (estado `unreachable`), sem mover o NPC.
- `stop(peca)`, `state(peca)` (`idle|moving|arrived|unreachable|dead`),
  `target(peca)`, `path(peca)`.
- `speed(peca)`, `set_speed(peca, n)`, `health`, `max_health`, `set_health`,
  `damage`, `heal`.
- `find_path(de, para)` → pontos, alcançável.
- `navigation()` → `{ready, cells, walkable, cell_size, columns, rows}`.

Nenhum personagem fixo foi criado no C++: o NPC é uma Part comum com componente
e o comportamento é sempre script.

## Implementação

- `src/objects/Part.{h,cpp}`: `NpcProperties` (já preparado localmente em
  trabalho anterior) com validação e marcação de corpo sujo ao anexar/remover.
- `src/game/Npc.{h,cpp}`: controlador por `InstanceId`, movimento por velocidade
  no passo fixo, giro limitado por `turn_speed`, morte, respawn configurável,
  detecção de travamento com replanejamento (máx. 2) e descarte de
  controladores de Parts destruídas.
- `src/game/Navigation.{h,cpp}`: grade derivada apenas de Parts estáticas
  sólidas (piso com normal ≥ 0,7, altura livre 4,5, folga lateral 1,0), no
  máximo 4096 células, construída sob demanda; A* com custo diagonal e
  orçamento de expansão.
- `src/physics/PhysicsWorld.{h,cpp}`: travamento de rotação para Parts com NPC,
  `setBodyYaw` sem tocar na velocidade, `isSpaceClear` para validar o degrau e
  filtros `solidOnly`/`staticOnly` no raycast.
- `src/game/Runtime.{h,cpp}`: `NpcSystem` por sessão, recriado no Play, liberado
  no Stop, executado dentro do passo fixo após `syncScene` e antes do passo
  Bullet.
- `src/scripting/LuaRuntime.cpp`: namespace `linblux.npc` com validação de
  opções antes de criar qualquer objeto.
- `src/serialization/ProjectIO.cpp`: campo `npc` no objeto JSON, leitura
  estrita e validação; ausência do campo mantém cenas antigas idênticas.

## Exemplos

- `assets/examples/lua/npc_patrulha.lua` — rota em ciclo, espera em cada ponto,
  registro de ponto inacessível.
- `assets/examples/lua/npc_perseguidor.lua` — raio de detecção, replanejamento
  quando o alvo se move, parada ao perder o alvo.
- `assets/examples/lua/npc_monstro.lua` — perseguição, ataque com intervalo,
  dano na vida real do jogador e dano recebido pelo evento `atacar_monstro`.

## Testes e resultados

- `linblux_studio` (`tests/studio_main.cpp`, `LinbluxNpc` + `NpcExamples`):
  1824 verificações, 0 falhas (1,016 s). Cobre criação, múltiplos NPCs, muro
  contornado (nenhum quadro com o corpo dentro do muro), destino inacessível,
  chegada, patrulha, perseguição, reposicionamento de alvo, perda de alvo,
  dano/cura/morte/respawn, destruição, `detach`, serialização JSON e liberação
  no Stop.
- `linblux_lua_player_validation`: 23 casos, incluindo quatro novos
  (`NpcPatrulha.lua`, `NpcDano.lua`, `NpcInvalido.lua`, `NpcSemComponente.lua`)
  executados no binário `linblux_player --validate`. Cena salva preservada.
- TURBO FAST (`20261025T191055Z-9045`): compilação aprovada, 13/13 testes,
  15,3 s de 300 s de orçamento.
- TURBO IMPACT (`20261025T191148Z-10201`): compilação aprovada, 22/22 testes,
  84,6 s de 400 s, cobertura global parcial (declarada pelo TURBO). Inclui
  `linblux_studio`, `linblux_game`, `linblux_tests`, `linblux_experiences`,
  `linblux_lua_player_validation`, `linblux_script_input_ui` e
  `linblux_progress_player_ui`.
- Nenhum FULL. Nenhum teste gráfico novo de UI foi executado por não haver
  alteração de interface.

## Limitações reais da navegação

- Grade XZ, não navmesh triangulada; o mundo plano é amostrado em células
  (2,5 unidades por padrão, ajustadas para caber em 4096).
- Só superfícies e obstáculos **estáticos** entram na grade. Corpos dinâmicos
  (NPCs, caixas móveis) colidem mas não são desviados; mover geometria estática
  exige reconstruir a grade (`linblux.npc.navigation()`).
- Sem saltos, sem saltos entre plataformas, sem ligação por rampa acima do
  limite de normal (0,7) e sem desvio dinâmico de outros agentes.
- Degraus de até 1,2 unidade são vencidos com verificação de espaço livre;
  desníveis maiores viram `unreachable`.
- O agente padrão da grade é uma caixa de raio 1,0 e altura 4,5; NPCs maiores
  que os corredores medidos podem ficar presos e terminam `unreachable`.
- Destinos fora dos limites da grade ou em ilhas isoladas retornam `false` sem
  mover o NPC.
- Vida, destino e waypoints são estado de execução: não são gravados. A posição
  inicial (dados da Part) e as propriedades de NPC são.
- Reposicionamento só existe no respawn configurado; não é usado como
  deslocamento normal.

## Próximo incremento pendente

Fase 2 da continuidade segue EM DESENVOLVIMENTO: rigs/animações para NPCs,
combate com armas e projéteis, e as demais demonstrações exigidas pela fase
(oito demonstrações reais). Áudio (fase 3), imagens/decals (4), rosto de fábrica
(5), UI Lua (6), vida circular (7), menu lateral (8) e integração (9) continuam
pendentes. O destino externo privado do AutoBackup continua não configurado.
