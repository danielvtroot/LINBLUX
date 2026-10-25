# Missão integral — câmera Lua

Implementado: FOV vertical persistido em PlayerSettings (1..170 graus, 60 quando
ausente no JSON antigo), edição nos padrões de câmera do Studio, configure Lua,
pose por posição/alvo/up ou Euler em graus, transição suave com quaternion,
consultas reais e release para a órbita anterior. Mouse/zoom não alteram poses;
cursor fica livre mesmo com câmera travada nos padrões. ESC continua protegido.
Pausa congela transição; teleporte mantém pose; respawn reinicia a câmera nativa.

Arquivos: Runtime.{h,cpp}, Rig.{h,cpp}, GameAssetsIO.cpp, LuaRuntime.cpp,
GamePanels.cpp (somente dois trechos), Player/main.cpp, chave de localização,
studio_main.cpp, rml_ui_main.cpp, lua_player_validation.py, camera.lua e contratos.
Outros diffs locais do editor/locales/TURBO preservados fora do commit.

ATLAS localizou Runtime, PlayerSettings, GameAssetsIO e bindings; confirmou os
consumidores do header. FAST 20261025T162427Z-8590: 13/13, sem falhas/timeouts,
build 122,5 s (consumidores dos headers), testes 13,8 s, total 137,7 s. Compilação
final Studio/Player/studio_tests/rml_ui_tests: 5,03 s após ajuste direcionado.
Integração supervisionada camera-lua-integration: studio + lua_player_validation
+ script_input_ui, 3/3 em 13,693 s, timeout global 60 s e timeout gráfico CTest 45 s.

IMPACT 20261025T163049Z-12921: 13/13 selecionados em 10,7 s (11,5 s totais),
sem rebuild redundante. Cobertura global parcial: gráficos gerais omitidos e o
teste gráfico pertinente foi executado separadamente pelo supervisor TURBO.
Não houve FULL; não se declara validação integral de todos os sistemas.

Verificado: salvar/reabrir FOV; aceitar JSON antigo sem campo e recusar FOV inválido
sem mutar cena; transição/rotação oposta sem vetor degenerado; pausa; release;
cena autoral intacta. Exemplo real camera.lua executado pelo Player; pose inválida
recusada. No Studio gráfico, FOV real, cursor/RMB, ESC/retomada e release testados.

Limites explícitos: pose é estado de execução reconstruído por Script salvo;
configure altera padrões sem cancelar pose; release é explícito. Poses não fazem
desvio automático de obstáculos e movimento usa a referência de órbita anterior.
Não há câmera do editor exposta ao Lua. A Fase A/Lua continua em desenvolvimento;
próxima unidade: transformações relativas e componentes programáveis existentes.
