# Recuperação — imagens, associações, rosto e superfícies

Base remota e4c7c5d; 93fe7e5 de NPCs já publicado. AutoBackup ativo e push
d674ad9f7067 confirmado no início. Diffs iniciais guardados em .turbo/recovery-images;
18 arquivos alheios (TURBO, .opencode, AGENTS, locales e editor) comparados por
SHA-256 e preservados sem alterações. A implementação reaproveita e completa os
diffs locais de imagens, não substitui o repositório nem experiências.

Correções reais:
- ImageInUse e ReplaceImageReferences usam DescribeImageUsage, cobrindo Surface,
  atributos de imagem, membros/adesivos de rig e manifesto; descritores identificam
  objeto/rig/membro/decal/propriedade e recusam edição obsoleta. Textos livres e
  arquivos externos homônimos não são confundidos com imagens gerenciadas.
- Nome amigável em image-names.json; importações recebem nome original; snapshots
  e pacotes preservam os nomes. IDs/caminhos continuam estáveis.
- Exclusão lista usos com Localizar/Remover associação, inclusive body.image
  legado; permite editar esse vínculo no Explorador/Propriedades sem duplicá-lo.
  Salva a cena antes de apagar; gerenciador também consulta a revisão persistida.
- Renderizador deixa de impor rosto/olhos quando nenhuma face foi configurada.
  Face explícita antiga continua funcionando. Rig/juntas/controlador preservados.
- Surface existia mas não era serializado/renderizado completamente. Agora
  Decal/Texture persistem no v3 e são restaurados após seus pais; codec compartilhado
  com RigDecal preserva rotação, tint, transparência e UVs. Renderização efetiva
  com composição por ordem dos filhos e rotação UV contínua.
- Miniatura aplicada usava caminho relativo ao documento RML: corrigida para
  absoluto gerenciado. Seletor pesquisável com rolagem e altura limitada ao conteúdo;
  destaque do tema gerado, campos duplicados removidos e confirmação bloqueada
  quando há referências. Mudanças restritas aos recursos e suas propriedades.

ATLAS localizou ImageAssets, Rig, renderer, ProjectIO e os inspetores; impacto
verificou consumidores. Estilos: studio.rcss é fonte manual; theme.rcss é gerado
por scripts/generate-theme.py e carregado depois. Gerador corrigido e --check passou.

TURBO: build inicial 172,59 s. FAST 20261025T235258Z-9735: 17/17, build183,7 s,
testes18,5 s. Após corrigir o codec: FAST 20261026T000330Z-16928: 17/17,
build117,6 s, testes12,6 s (130,8 s totais). IMPACT final
20261026T000837Z-21049: 17/17 em12,9 s (13,4 s totais), cobertura global parcial.
Sem FULL. Builds finais Studio/Player/testes gráficos aprovados.

Falhas tratadas: primeira fixture gráfica foi reduzida indevidamente a um quadro
por evento, falhou e voltou aos dois quadros necessários; guardas impedem acessar
vetor vazio. A segunda detectou Surface sem persistência (e miniaturas relativas),
ambos corrigidos no código real. Não são apresentados como testes aprovados.

Integração gráfica final .turbo/logs/image-recovery-final.log: 3/3 em74,874 s,
supervisão TURBO timeout150 s; CTest individuais45/60/120 s. Testes de pixels
verificam Surface visível/transparente, cabeça sem face automática e face explícita
legada, todas as faces do rig. Fluxo X11 verifica importação, miniatura, rename,
localização de body.image, remoção, exclusão após salvar e Surface nas Propriedades.
Capturas inspecionadas: /tmp/loblox-rig-image-YLIdzS/rig-no-face.png e
/tmp/loblox-rml-ui-TRyA3r/{image-references,surface-properties,rig-image-open}.png.
Ajuste final de altura validado por linblux_rig_decals_ui, 1/1 em67,334 s,
log .turbo/logs/image-layout-final.log. Captura final inspecionada em
/tmp/loblox-rml-ui-WhuyAY/surface-properties.png, sem bloco vazio ou sobreposição.

Persistência headless: salvar/reabrir, propriedades de Surface e RigDecal, padrão
da engine → nova experiência, exportar/importar .lo com imagem/nome; JSON inválido
não troca a cena; falha ao salvar mantém manifesto e cena anteriores. Player
independente carrega componente e remove só esse componente pela API Lua existente.

Limitações: inferência de dependências dinâmicas em scripts não é completa.
Backups de dados privados externos seguem sem destino. Modelos transformáveis,
multisseleção, Attachments, inventário/E/Delete/hotbar, RifleDemo, UI Lua, HUD e
menu lateral ainda pendentes. Áudio/NPCs existentes não foram reimplementados.

Backend PUBLICADO: 4bb803c16365d79ad62f752fd25991c15772f398, push main confirmado.
Frontend/renderizador/testes/documentação compõem o commit que inclui este relatório.
