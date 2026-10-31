> Checkpoint histórico de 24/10/2026. Instruções de execução e pendências abaixo
> valem para aquela revisão. Use hoje [Testes](../TESTING.md) e [Alpha](../ALPHA_0_1_0.md).

# Continuidade — 24/10/2026

Pausa solicitada pelo responsável por limite de créditos. Não declarar a missão
inteira concluída. Código real implementado, Studio/Player compilados em
`build-linblux`; o antigo `build` tem cache de caminho obsoleto e foi preservado.
O Studio foi iniciado na sessão gráfica de labubu. Log: `/tmp/linblux-desktop-launch.log`.

## Primeiro: concluir verificações já iniciadas

As execuções abaixo podem continuar em segundo plano. Confira o rodapé antes de
executar de novo; não interprete teste em andamento como aprovado:

- `/tmp/linblux-rml-final.log`: suíte RmlUi completa após corrigir ribbon e medidas
  de textos traduzidos. A tentativa anterior tinha 22 falhas/1024 verificações.
- `/tmp/linblux-final-suite.log`: outras 32 suítes, `ctest -E '^linblux_rml_ui$' -j2`.
  Mouse lock, Game UI e Experience UI já passaram nesta rodada; restante pendente
  no instante deste registro. Rodadas anteriores também aprovaram núcleo,
  padrões, Player, ciclo de vida, reinício real, gizmos e qualidade gráfica.
- `/tmp/linblux-remote-rebuild.log`: clone novo do GitHub em
  `/tmp/linblux-remote-rebuild-20261024`, fsck/configuração passaram, compilação
  Release ainda em andamento. Esse clone começou no commit 4359a25: depois faça
  `git pull --ff-only` nele para incluir as correções finais, compile novamente
  e teste. Não mexa em alterações locais para resolver divergências.

Se necessário: `cmake --build build-linblux -j2` e
`ctest --test-dir build-linblux --output-on-failure`. Confira falta de espaço em
/tmp (tmpfs 512 MiB); há amplo espaço no disco em /home/labubu/projeto.

## Pendências funcionais e de aprovação

1. Resolver qualquer falha remanescente das suítes e registrar contagens finais.
2. Inspecionar novas capturas de editor padrão e personagem herdado nos diretórios
   `/tmp/linblux-studio-default-restart-*`. O teste real reabre/cria/executa pela UI;
   a autoria minuciosa do personagem da fixture é feita no teste de núcleo.
3. Realizar manualmente todo o roteiro de substituição do personagem no Studio,
   editando rig/animações/controlador, publicando, reiniciando e executando.
4. O formato atual é de seis corpos/juntas: falta suporte a malhas/esqueletos
   arbitrários (FBX/glTF etc.) e controladores externos. Importação implementada
   aceita personagens de experiências JSON/.lo compatíveis, incluindo recursos.
5. Medir vídeo/UI em GPU integrada física. Decoder: ~69 MiB RSS, ~29% de um núcleo;
   Home inteiro em llvmpipe: ~247 MiB RSS e 219–221% CPU. Não prometer fluidez em
   todo notebook a partir dessa medição por software.
6. Revisar tradução dos estados raros e mensagens dinâmicas; catálogo central já
   existe. Manter nomes serializados, APIs Lua e namespace legado compatíveis.
7. Concluir análise de licença do vídeo/logo fornecidos e empacotamento das
   dependências: o FFmpeg instalado declara GPL v2+. Assets pessoais desconhecidos
   ficaram fora da instalação/índice atual, preservados localmente e no histórico.
8. Terreno avançado, malhas genéricas e multiplayer/Server continuam no roadmap.
   Separação Engine/Studio/Player existe; seleção e imagens ainda têm acoplamentos
   documentados em ARCHITECTURE.md.
9. Após testes, atualizar relatórios, commit e push normal; verificar SHA remoto.

## O que preservar

Não substituir este checkout por clone vazio, não usar reset --hard/force push.
Históricos foram unidos por merge. Backups verificados ficam em
`/home/labubu/projeto/linblux-backups` (incluindo originais movidos de /tmp).
GitHub protege código rastreado; experiências pessoais/arquivos ignorados precisam
backup próprio. Logo original não foi alterada. Veja o registro MIGRACAO_LINBLUX_2026_10_24.md,
PADROES_DA_ENGINE.md, BACKUP_E_RESTAURACAO.md e ASSETS_E_LICENCAS.md.

Compilação final rápida Studio+Player: `/tmp/linblux-delivery-build.log`, sucesso.
Correção de logo do Player: 4359a25, push confirmado; arquitetura/padrão protegido:
aaddc8d, também confirmado. Ajustes finais da interface/documentação são registrados
em um checkpoint explícito com validação completa ainda pendente.
