# Modelos — pivô, transformação de conjunto e Lua

IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER: ModelFrame tipado; seleção múltipla
com Ctrl; agrupar/desagrupar preservando poses; pivô sem mover geometria; mover,
girar e escalar uniformemente nas Propriedades; Modelos aninhados, clonagem com
IDs novos, scripts por Modelo e API Lua com conversão local/mundial. Modelo não
cria corpo ou solda automática. Gizmos de conjunto e undo composto seguem pendentes.

Arquivos principais: Instance, Scene, LuaRuntime, ProjectIO, EditorActions,
StudioFrontend, EditorUI/StudioPanels, ribbon, CMake e testes studio/player/
experiences/rml; exemplo modelos.lua e guia MODELOS.md. ATLAS consultado antes
de implementar; hierarquia/clonagem/runtime existentes foram ampliados.

Duas regressões encontradas: duplicateTree invalidava a lista de filhos ao
clonar Parts (GDB em .turbo/logs/models-gdb.log); corrigido com snapshot da lista.
Rotação de 90° perdia precisão no ciclo Euler. Cenas com pivô explícito adotam
v4: pivô e orientação das Parts persistem quaternions. Leitura v1/v2/v3 mantém
as regras anteriores; projetos sem pivô novo mantêm sua versão. Testes conferem
a orientação composta dos filhos após reabrir, em vez de reduzir a exigência.

Cota nativa de 32 operações custosas de Modelos por quadro compartilhada pelos
scripts; exemplo e teste de excesso executados no Player real. Validação de
tamanho/posição acontece antes de modificar filhos; escala não uniforme não é
suportada. Grupos recusam objetos protegidos/repetidos e seleção pai+filho.

FAST final 20261026T004226Z-7953: 17/17, build286,1 s, testes12,4 s, total299 s.
Cobertura global desconhecida por CMake/teste novo. Build final focado Studio,
Player e rml_ui_tests: 2,37 s. IMPACT 20261026T020406Z-15199: 20/20 em53,8 s
(54,3 s totais), cobertura global parcial, sem FULL. Falhas anteriores de Studio
(crash e precisão) foram corrigidas; não são apresentadas como aprovações.

linblux_models_ui: 1/1 em29,644 s; supervisor65 s e CTest60 s. X11 seleciona
duas Parts, agrupa, altera posição/rotação/escala/pivô, salva/reabre, executa
Lua em Play, para e desagrupa sem alterar as posições. Captura inspecionada:
/tmp/loblox-rml-ui-pHAiUX/model-properties.png. Log .turbo/logs/models-ui.log.
Headless também verifica raycast Bullet após mover, clonagem, seleção após
remoção, JSON inválido sem alteração, exportação/importação .lo e scripts.

Próximo: gizmos/undo de conjunto e Attachments/vínculos físicos; depois inventário,
E/Delete/hotbar e demais requisitos da missão. Não há inventário, RifleDemo ou
multiplayer implícito nesta entrega. Diffs alheios preservados.
