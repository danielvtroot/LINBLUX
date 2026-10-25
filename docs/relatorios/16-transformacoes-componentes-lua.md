# Missão integral — transformações e componentes Lua

Implementado: transformação mundial atômica, conversão explícita para referencial
de outra peça, conversão de pontos/direções, escala positiva e interpolação com
quaternion. Criação de Script filho, listagem/remoção de componentes reais, cotas
e descarregamento no scheduler existente. Sem formato novo ou alteração da autoria.

Arquivos: LuaRuntime.cpp, studio_main.cpp, lua_player_validation.py,
transformacoes.lua, guia Lua, contrato de scripting e acompanhamento.

ATLAS localizou Part::transform, RigidTransform/Compose/Inverse, Scene e scripts.
Primeiro FAST: 11/13; exemplo tentava colocar peça como filha de peça, operação
recusada pelo contrato real da cena. Corrigido para grupo e referência espacial
explícita, preservando o formato e os projetos antigos; sem ampliar parenting
às cegas. A API não promete vínculo espacial automático de filhos.

FAST final 20261025T164433Z-20435: 13/13, zero falhas/timeouts, build 19,6 s,
testes 9,8 s, total 29,8 s. IMPACT 20261025T164543Z-21632: 13/13, zero falhas,
build 24,6 s/testes 15,4 s/total 42,2 s; cobertura global parcial sem gráficos.
Validação adicional pertinente do exemplo com raycast Bullet após transformar:
studio + Player, 2/2 em 0,819 s pelo supervisor TURBO, timeout 20 s.

Testado: transformações ida/volta e rotação de 170 a -170 graus; escala efetiva
no corpo Bullet (raycast real encontra a superfície transformada); Script gerado
executa em seu objeto e é removido; salvar/reabrir e Player independente usam
o arquivo de exemplo real. Tamanho inválido não move parcialmente a peça;
referência não espacial, escala negativa, remoção de componente alheio, NUL no
nome e fonte acima de 256 KiB são recusados. Componente alheio continua executando.

Limites: referenciais rígidos usam posição/rotação, size é dimensão física, não
herda escala; modelos/grupos continuam organizacionais. Componentes gerados em
Play são transitórios, reconstruídos pelo Script salvo. Decals são listáveis/
removíveis, mas sua edição/importação Lua pertence à fase de imagens. A missão
e a fase de Lua permanecem incompletas; persistência controlada é a próxima unidade.
