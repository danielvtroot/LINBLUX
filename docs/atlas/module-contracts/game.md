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
