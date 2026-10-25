# Fase 2 — incremento 2E: entrada roteada e interação por Lua

Implementado linblux.input com estados e bordas de teclado, mouse e controle,
posição relativa ao viewport, movimento/roda do mouse e eixos do controle 0.
Eventos key/mouse/gamepad_pressed/released usam o scheduler existente, com cotas
e cancelamento por script. Escape e teclas de função ficam reservados à engine.
Lua não faz polling dos dispositivos nem altera a posse decidida pelo frontend.

Studio respeita o roteador existente de gameplay, foco, widgets e menus; mouse
exige viewport sob o ponteiro ou captura. Player exige foco e ausência de pausa.
As amostras são transitórias, não mudam formatos serializados. O exemplo original
interacao.lua usa E ou botão sul para emitir o evento das portas já implementadas.

Arquivos: ScriptInput.h, Runtime.{h,cpp}, LuaRuntime.cpp, duas linhas de integração
em GamePanels.cpp, main.cpp do Player, studio_main.cpp, rml_ui_main.cpp, registro
do teste em CMakeLists, exemplo, guia Lua e contrato de scripting. As traduções
preexistentes em GamePanels foram preservadas fora do índice/commit.

ATLAS encontrou PlayerInput, drawGameplayViewport e o contrato game. A inspeção
dirigida confirmou a posse do teclado e as regras de captura. As strings de eventos
são alocadas dentro de chamada Lua protegida, evitando panic nativo por cota de
memória ao receber uma tecla.

TURBO FAST 20261025T081458Z-12225: 12/12 aprovados, zero falhas/timeouts,
build 72,9 s, testes 9,9 s, total 84 s. Resultado global PASSED_PARTIAL/cobertura
desconhecida por CMake e mudanças locais fora deste incremento; isso não é FULL.
Compilação final de Studio, Player e ambos os testes em 4,72 s. Validação final
pelo supervisor TURBO: studio + script_input_ui, 2/2 em 9,625 s, timeout 45 s.
Studio: 1643 verificações; integração RmlUi/X11: 33; zero falhas.

Verificado com janela real em Xvfb/software: tecla mantida só dispara uma vez,
clique no viewport chega ao Lua, coordenadas relativas válidas, pausa não dispara
a ação, continuar restaura a entrada e Shift+F5 encerra Play preservando a autoria.
Teste headless inclui controle/eixos por amostra injetada, desconexão, bordas,
liberação ao parar e execução do arquivo interacao.lua. Não houve teste com
controle físico nem medição de GPU integrada; não houve FULL ou redesign visual.

AutoBackup foi consultado: monitor parado, sem checkpoint/push próprio. Commits
destes incrementos são enviados manualmente a origin/main; não se afirma que
arquivos locais ignorados, privados ou fora dos commits estejam protegidos.

Fase 2 continua: áudio e UI por Lua ainda faltam. NPCs/navegação, gerenciador de
assets, personagem sem rosto obrigatório e nova interface do Player permanecem
nas próximas fases. Home, vídeo e editor 3D não foram redesenhados neste incremento.
