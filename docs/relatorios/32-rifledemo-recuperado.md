# RifleDemo recuperado — 26/10/2026

Base:4a0be641eefc0103136f46f81f782a5270e5f1ea. WIP desenvolvimento preservado e concluído,
sem recriar inventário, áudio, UI ou navegação. ItemActions controla estado
limitado por ID (intervalo, munição, recarga); efeitos/dano/recoil/HUD são Lua.
RifleDemo contém15 objetos autorais,5 sons originais,3 alvos com rig e saúde,
guia em patrulha e decal oficial. Instâncias/E/Delete/slots preservam identidade.

Correções reais: busca em descendants para alcançar BocaDoCano dentro de Cano;
peça do traçante aumentada para respeitar mínimo0,2; raycast anterior aos efeitos
e filtro Lua limitado dos próprios efeitos; som por ID estável, evitando quebra
de paths após relocação/hash na importação; efeitos pertencem ao sistema do
mundo, com descarte mesmo após destruir o item. Inteiros Lua acima do limite
são recusados antes de conversão32bits, com preservação do estado.

Regressão NPC: Runtime anexava NPCs autorados após inicializar a VM e apagava
rotas/vida já configuradas pelos Scripts. attachAuthored agora preserva o
controlador existente. Teste verifica deslocamento real e clipe walk do guia.

Código principal: game/ItemActions, Runtime, Npc, LuaRuntime, exemplos/rifledemo,
tools/examples/build_rifledemo.py, tests/item_actions_main.cpp,
tests/rifledemo_main.cpp e tests/rifledemo_player.py, CMake. API em INVENTARIO.md.

Build Studio/Player e testes aprovado (último lote36,52 s, correçãoNPC15,69 s).
FAST20261026T231845Z-823:17/17 em6,4 s, global desconhecido; IMPACT
20261026T231931Z-1300:23/23 em50,1 s, global parcial. Não são aprovação FULL.
Integração nativa final3/3 em3,22 s. Inventário no Studio39,79 s aprovado.
Player real com E/1/tiro/dano/R/Delete/recoleta e guia animado17,583 s aprovado;
marcador de sucesso só é desenhado após assertivas Lua do fluxo completo.
Reabertura/exportação/importação .lo verificam scripts, sons/protótipo,
saúde/NPCs e imutabilidade da autoria. Captura do Player inspecionada.
Teste final de limpeza de efeitos após descarte foi ampliado ao fechar a unidade;
resultado final registrado no acompanhamento. Sem dispositivo de áudio físico
nesta execução, não se declara reprodução audível aprovada.

Próximo: pacote fora do checkout, migração das expectativas gráficas históricas,
regressão abrangente e revisão de licenças. Não há tag ou Release pública.

Fechamento: rifledemo + rifledemo_player2/2 aprovados em18,310s, incluindo
remoção do item com efeitos ativos, limpeza, guia/walk e roundtrip .lo.
