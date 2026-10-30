# Explorador e armazenamento — Alpha 0.1.0

## Entrega

- **Nomes de Parts: concluído para criação e renomeação.** Validação de espaços Unicode, controles, comprimento e colisões entre irmãs. Duplicação gera nomes válidos; filhos com nomes válidos mantêm seus nomes no novo Modelo. IDs antigos e nomes carregados são preservados.
- **Conversão em lote: pendente.** Exige prévia, análise das referências Lua e recuperação; não há substituição automática de strings nem alteração de experiências pessoais.
- **Explorador: concluído no frontend RmlUi.** Linhas compactas, serviços ordenados, ícones distintos, seleção, filtro e expansão. Serviços existentes são reutilizados, protegidos contra exclusão e reparenting. Cenas antigas não recebem serviços novos ao carregar; a criação é explícita e idempotente.
- **ServerScriptService: concluído para o runtime local.** Script e LocalScript habilitados executam em Play; ModuleScript exige require. Edit não executa scripts; Stop/Play restaura a cena autoral.
- **ServerStorage e ReplicatedStorage: concluídos para armazenamento local.** Hierarquias persistem, mas suas Parts não renderizam, não têm corpos Bullet ativos e não entram em picking, navegação ou consultas de mundo. Scripts armazenados ficam inativos. Mover de volta restaura a participação no mundo.
- **Itens: concluído com o inventário existente.** A ação “Protótipos de itens” organiza a pasta convencional ServerStorage/Itens. Spawn clona para Workspace com novos IDs, preservando o original; scripts do clone seguem o ciclo atual. Coleta, equipar e soltar foram exercitados.
- **StarterItems: pendente.** Não foi criada uma concessão automática sem implementação.

ReplicatedStorage não implementa replicação de rede. A Alpha continua local. Undo/Redo de transformações foi preservado; histórico geral de renomeação e reparenting não foi acrescentado. Versões anteriores podem rejeitar hierarquias de Parts armazenadas; não houve alteração do formato de arquivo nesta entrega.

## Validação executada

- **PASSOU:** build incremental oficial de Studio, Player e alvos de teste afetados, com dois jobs.
- **PASSOU:** oito testes headless distintos: linblux_experiences, linblux_studio, linblux_game, linblux_tests, linblux_defaults, linblux_engine_default_lifecycle, linblux_item_actions e linblux_rifledemo. Studio/defaults foram repetidos após corrigir expectativas das fixtures.
- **PASSOU:** testes gráficos pelo executor supervisionado do TURBO, com orçamento de 180 s por seleção e timeout do CTest de 45–75 s por teste: Explorador, armazenamento, Decals e RifleDemo no Player.
- Explorador: 142 verificações; armazenamento: 80 verificações, em 1366×768 e 1920×1080. Cobrem serviços, renomeação, propriedade de protótipo, salvar/reabrir, Play/Stop e spawn.
- Decals: armazenamento invisível nos dois serviços e clone visível; orientação não foi invertida.
- RifleDemo: passou após a fixture observar o evento real reload_end antes de soltar o item, em vez de esperar 1,8 s fixos. Mantidos os limites de tempo e as verificações de munição, dano, identidade e orientação da imagem.
- Headless inclui física de Parts dinâmicas entrando/saindo do armazenamento, scripts habilitados/desabilitados, IDs, persistência, exportação/importação .lo, inventário e restauração da autoria.
- **PASSOU:** 217 referências locais em Markdown verificadas sem caminhos ausentes; git diff --check; índice ATLAS atualizado ao final do código.
- **NÃO EXECUTADO:** FULL, benchmark do HDD, suíte longa de manipulação, teste físico de áudio e testes próprios do ATLAS/TURBO (ferramentas não alteradas).

Captura real de uma cena sintética: [Explorador e protótipo armazenado](../prints/explorador-servicos.png). Não foram usadas experiências pessoais como fixtures.

## Publicação e proteção

HEAD inicial: `9e1e0991084dbf96641c45f7897885135fb01a97`.

Incrementos publicados antes deste documento:

1. `cab7bad` — nomes de Parts.
2. `3486563` — serviços e Explorador.
3. `0f0090a` — semântica central de armazenamento.

O quarto incremento contém a integração de protótipos, testes, captura e este documento. O hash final e a confirmação remota são informados na entrega. Esta tarefa não publica um pacote binário nem resolve pendências anteriores de redistribuição.

Os dois hunks preexistentes de picking em Scene.cpp/Scene.h permanecem fora dos commits. Os três relatórios não rastreados do início foram preservados e conferidos por hash. Nenhum .lo pessoal foi convertido.

O vídeo não rastreado gravacao.mp4 existia no início e deixou de existir naquele caminho durante a sessão, sem operação desta tarefa sobre ele. Foi solicitada confirmação sobre movimentação externa; a causa permanece sem confirmação.
