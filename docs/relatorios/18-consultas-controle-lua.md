# Missão integral — volumes Bullet e controle Lua

Implementado overlap_box/overlap_sphere com contactTest real, caixas orientadas,
filtros de sensores/peça/jogador, resultados estáveis e cota host-only de consultas
por quadro (padrão 8). Coleta nativa termina antes de construir os handles Lua.
Truncamento é conservador: pode haver candidato broadphase não examinado, sem
garantia de outra interseção exata. Limites documentados em SCRIPTING_LUA.md.

Jogador recebe intenção horizontal world/camera, salto pelo controlador existente,
velocidade, força e impulso na cápsula real. Intenção dura um quadro e retorna à
entrada normal se não for renovada. FixedUpdate pode definir intenção antes do
passo corrente. Não há navegação/NPCs ou eventos contínuos de região anunciados.

Arquivos: Runtime.{h,cpp}, LuaRuntime.{h,cpp}, consultas_controle.lua, testes
studio/player, guia e contrato physics. ATLAS localizou Runtime, Bullet e
consumidores; impacto cobre Studio/Player e testes de runtime.

Build focado Studio/Player/studio_tests: 114 s, aprovado. FAST
20261025T174655Z-26450: compilação adicional dos consumidores em 62,6 s;
13/13 testes em 13,6 s, total 79 s, sem falhas/timeouts. O exemplo real é salvo,
reaberto e executado; verifica sensores, exclusão de jogador, peça ignorada,
limite de resultados, caixa orientada sem falso positivo por AABB, impulso,
movimento e salto. Player também recusa nove consultas no quadro e movimento
vertical inválido, preservando os bytes das cenas.

IMPACT 20261025T174825Z-27825: 13/13 em 11,6 s (12,2 s totais); cobertura global
parcial, gráficos gerais omitidos. Integração gráfica pertinente script_input_ui
aprovada, 1/1 em 20,792 s, supervisor com timeout 45 s; log
.turbo/logs/spatial-control-input.log. Não houve FULL nem alegação de conclusão
integral. AutoBackup confirmado ativo, checkpoint/push 17e2422d0925, sem arquivos
cobertos pendentes. Dados privados continuam fora do backup público de código.

Próximo incremento: propriedades físicas persistentes das peças, depois NPCs
e dependências de áudio/UI; fases globais permanecem abertas.
