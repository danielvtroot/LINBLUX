# Módulo `game`

**Responsabilidade:** modelo de personagem/rig R6, animações, assets de jogo,
spawn, sessão de runtime, câmera do jogador, input e físicas do personagem.

**Fonte:** `src/game/{Rig,Runtime,Spawn,GameAssetsIO,ImageAssets,JpegImage}.{h,cpp}`.

**Entradas:** cena autoral serializada, `GameAssets`, `PlayerSettings`,
`PlayerInput`, tick de simulação e comandos de Play/Pause/Stop.

**Saídas:** cena de runtime independente, pose animada, transformações do
personagem, câmera, saúde/estado humanoide, mensagens e validação.

**APIs principais:** `Rig`, `AnimationClip`, `Animator`, `GameAssets`,
`PlayerSettings`, `RuntimeSession`, `PlayerCamera`, `SpawnPoint`.

**Invariantes:** o runtime recebe uma cópia e não escreve no mundo autoral;
física é autoritativa para o corpo e a câmera usa uma amostra de apresentação;
poses são replayáveis e não incrementais; o rig padrão tem seis corpos e seis
juntas em ordem topológica; IDs de animação/rig são estáveis.

**Dependências:** `Scene`, `PhysicsWorld`, `scripting::LuaRuntime`, `objects`,
raylib (vetores/cores), serialização via chamador. Não depende de ImGui.

**Ciclo de vida e erros:** `RuntimeSession::start` cria cena/física/cápsula;
`stop` destrói na ordem documentada; `pause/resume/respawn` alteram estado sem
vazar corpos. Erros retornam por `std::string& error` ou exceção na carga.

**Testes:** `linblux_game`, `linblux_spawn`, `linblux_engine_default_lifecycle`,
`linblux_game_ui`, `linblux_engine_default_restart` e testes de rig/câmera.

**Como estender:** adicione validação ao `Rig`/`AnimationClip`, mantenha a cópia
de runtime e cubra o ciclo Play → Stop → Play. Mudanças de física devem passar
por `PhysicsWorld`, não por acesso direto ao Bullet.

**Limites:** rigs arbitrários (FBX/glTF) e esqueletos fora do R6 não estão
implementados; multiplayer/Server não existe; a posição da câmera pode divergir
do corpo em um frame por design.

PlayerSettings.cameraFov é opcional no JSON antigo (padrão 60), validado em
1..170 graus. PlayerCamera permite pose roteirizada/transição por quaternion;
mouse/zoom não assumem a câmera durante a pose. release restaura a órbita;
respawn reinicia o controlador, teleporte mantém uma pose roteirizada.
Testes: LinbluxCameraControl/studio, lua_player_validation e script_input_ui
(RmlUi real, pausa/cursor/retorno). Poses são transitórias; scripts e padrões
persistem no formato existente.
ProgressStore mantém dados privados em escopo fornecido pelo host, fora da cena
e dos assets. RuntimeSession salva ao pausar/parar e descarta início cancelado.
--validate e padrão da engine usam memória. Não confundir progress.json com mundo
autoral. Testes: LinbluxProgress/studio, lua_player_validation, progress_player_ui
e script_input_ui com reabertura de Play. Ver PERSISTENCIA_PRIVADA.md.

NPCs (25/10/2026): `src/game/Npc.{h,cpp}` e `src/game/Navigation.{h,cpp}`.
`NpcProperties` vive na Part (dados serializados em `npc`); o controlador de
runtime é um `std::map<InstanceId,Controller>` em `RuntimeSession::npcs_`,
recriado a cada Play e liberado no Stop, sem ponteiros persistidos. O corpo é a
própria Part dinâmica: `PhysicsWorld` trava a rotação de Parts com NPC,
`setBodyYaw` gira sem tocar velocidade e `isSpaceClear` valida o degrau de até
1,2. A navegação é uma grade XZ de Parts estáticas sólidas (máx. 4096 células,
construída sob demanda) com A* 8-vizinhos, sem cortar cantos. Limites: sem
navmesh, saltos, ilhas ligadas por rampa íngreme ou desvio dinâmico; corpos
maiores que o corredor amostrado podem ficar presos e resultam em `unreachable`.
Testes: `LinbluxNpc` e `NpcExamples` em `tests/studio_main.cpp` (muro, rota,
destino inacessível, morte/respawn, destruição, detach, serialização) e quatro
casos `Npc*.lua` em `tests/lua_player_validation.py`.

Imagens: DescribeImageUsage é a fonte única para usos em atributos conhecidos,
Surface, Body.image, RigDecal e manifesto. Descritores preservam IDs/propriedade
e recusam edição obsoleta. ImageInUse/ReplaceImageReferences delegam à consulta.
Nomes amigáveis são metadados versionados no projeto, não caminhos novos.

**Inventário da sessão:** Inventory possui tipos/instâncias identificados pelos
IDs de Modelos existentes. Propriedades autorais marcam protótipos; catálogo
carregado antes da VM desativa scripts de protótipo e ativa apenas os clones.
Bolsa/equipamento são temporários; RuntimeSession mantém isolamento da autoria.
Descarte usa limites reais da geometria, raycast e teste de espaço Bullet.
InventoryHidden é a regra central para desenho/consultas/contatos, incluindo
Surfaces. Eventos inventory_changed usam o scheduler/listeners Lua existentes.
Testes: InventoryLifecycle/studio, inventario.lua/Player, .lo/experiences,
protótipo nas Propriedades/models_ui e pixels/decal_render. E/Delete/hotbar
ainda requerem a camada de interação e apresentação; não são parte desta API.

**Interação/controles:** InteractionSystem registra Parts/Modelos, testa
alcance geométrico/linha de visão e conclui hold com cancelamento e latch.
Runtime sincroniza itens world, coleta, slots1–9, páginas e Delete.
interaction_completed usa o scheduler seguro de eventos. ExperienceOverlay
é a apresentação compartilhada Studio/Player; não possui lógica de itens.
Teste nativo InteractionControls e gráfico linblux_item_controls_ui.

**Uso de itens:** ItemActions mantém intervalo, munição/recarga e transições por
ID da instância; Runtime controla tick, cancelamento e descarte. O efeito/dano
é Lua (RifleDemo), não uma classe de arma. Configure valida inteiros antes de
narrowing, limites e estado; filas são limitadas. Raycast pode ignorar a subárvore
do Modelo e ignora representações guardadas/protótipos. Testes: item_actions,
rifledemo (inclui .lo) e rifledemo_player. attachAuthored não reinicializa NPC
já configurado pelo Script durante startup; a demo verifica rota e clipe walk.
