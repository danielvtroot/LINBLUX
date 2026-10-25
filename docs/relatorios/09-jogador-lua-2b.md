# Fase 2 — incremento 2B: jogador, câmera e encerramento seguro

Implementado em Runtime/LuaRuntime: jogador real por linblux.player, dano, cura,
vida, velocidade de caminhada, salto, rotação automática, respawn, teleporte com
consulta de obstrução e checkpoint de sessão com fallback aos spawns normais.
Animações são listadas do projeto e atribuídas ao controlador nativo. Câmera é
consultável/configurável sem alterar a câmera do editor. Eventos nativos novos:
spawn, health_changed e death. Sinais legados continuam preservados.

Uma falha no teste novo esperava 100 de vida após respawn com máximo 150. A
implementação existente restaura o máximo; a expectativa foi corrigida para 150,
preservando o comportamento nativo. FAST inicial: 11/12; regressão específica
corrigida passou pelo supervisor TURBO em 1,67 s, com timeout de 5 s. Studio,
Player e teste Studio compilaram em 15,09 s na rodada final. Não houve FULL.

A inspeção de segurança identificou finalizadores __gc como caminho sem hook de
instruções no Lua. setmetatable agora recusa __gc inclusive false, preservando
metatabelas normais. O evento stop é a alternativa de cleanup com cota. Chamadas
nativas durante a liberação final são bloqueadas; testes verificam recusa,
metatabelas comuns, liberação e encerramento sem tarefas pendentes. Referência
primária e limites estão em SCRIPTING_LUA.md. Isso não constitui sandbox de SO.

Câmera real em primeira pessoa, alteração de vida, eventos, checkpoint/respawn,
recusa de teleporte obstruído e preservação da autoria foram testados. Alterações
locais de outras tarefas permanecem fora deste commit. A Fase 2 continua em
incrementos: contatos, passo fixo, entrada, áudio e UI ainda precisam de integração.
