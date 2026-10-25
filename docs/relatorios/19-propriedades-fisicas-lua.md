# Propriedades físicas persistentes — entrega e continuidade

IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER: BodyProperties tipado com densidade,
atrito, restituição e amortecimento linear/angular; configuração em Propriedades
RmlUi e ImGui; API Lua body, mass, angular_velocity, apply_torque e
apply_angular_impulse. Densidade atualiza massa/inércia no Bullet sem reconstruir
o corpo nem zerar velocidade; ancoragem mantém o comportamento anterior.

Persistência: JSON body opcional (cenas antigas usam os valores anteriores),
clonagem, cena declarativa Lua e pacote .lo. Valores inválidos são recusados sem
alterar parcialmente propriedades ou substituir a cena carregada. Exemplo real:
assets/examples/lua/corpos_rigidos.lua, executado no Studio e Player independente.

Arquivos: Part.{h,cpp}, PhysicsWorld.{h,cpp}, Runtime.{h,cpp}, Scene.cpp,
LuaRuntime.cpp, ProjectIO.cpp, SceneLua.cpp, EditorUI.cpp (somente o hunk próprio),
EditorActions.cpp, StudioFrontend.cpp, cinco traduções próprias no locale,
CMakeLists.txt, testes studio/player/experiences/rml, exemplo e documentação.
Alterações preexistentes de TURBO, editor e traduções foram preservadas.

ATLAS localizou Part, PhysicsWorld, ProjectIO, os dois inspetores e consumidores.
Índice atualizado. TURBO compilou Studio/Player/studio_tests em 129,97 s; FAST
20261025T175451Z-31589 recompilou consumidores em 164,2 s e executou 13/13 testes
em 12,3 s (176,9 s totais). Cobertura global desconhecida devido ao CMake alterado.
IMPACT 20261025T175800Z-1719: 18/18 em 56,4 s (56,9 s totais); cobertura global
parcial, sem FULL. Nenhuma alegação de conclusão integral.

Regressões verificam massa/inércia, impulso, atrito e amortecimento efetivos,
quique real, mudança de densidade, ancoragem, rotação, validação atômica, cena
antiga sem body, clonagem e persistência. O pacote .lo recupera os cinco valores.

Primeiro teste gráfico falhou por xdotool interpretar -1 como opção e atingiu
45 s. Corrigido com -- e menos quadros redundantes no caminho focado. Rebuild
rml_ui_tests/experience_tests em 25,65 s. Validação final supervisionada:
linblux_body_properties_ui + linblux_experiences, 2/2 em 37,405 s, sem falhas;
log .turbo/logs/body-properties-ui-final.log. Os cinco campos foram editados por
X11, densidade negativa recusada, cena salva/reaberta e Play executou Lua com
massa 60 e impulso real. Screenshot geral inspecionado em
/tmp/loblox-rml-ui-70d5VY/body-properties.png; os campos físicos ficam abaixo
do trecho visível quando o filtro é limpo. Não há redesign visual nesta unidade.

A fase Lua inteira continua EM DESENVOLVIMENTO: áudio/UI e dependências restantes
não foram implementados nesta unidade. NPCs e navegação ainda não implementados;
somente localização inicial pelo ATLAS, sem código de pathfinding. Próximo passo:
planejar e implementar navegação física limitada ao terreno efetivamente
suportado, controle/rig/vida de NPCs e exemplos usando APIs públicas, mantendo
as dependências de áudio e UI explícitas. Não recriar runtime ou Bullet.

Interrupção solicitada pelo usuário por limite de créditos: nenhuma fase nova
iniciada. Commit/push desta unidade registrados no relatório central e resposta
final após confirmação remota. Dados privados externos continuam sem destino de
backup configurado; o monitor de código permanece ativo.

PUBLICADO: 4ce62e2b980bbb92ee7d9174ff56ec57a7bcf5f5; push e hash remoto main confirmados.
