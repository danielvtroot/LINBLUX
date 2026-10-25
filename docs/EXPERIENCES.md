# Experiências LINBLUX

Cada experiência tem uma pasta própria de installationId, `experience.json`,
`places/` com cenas e `assets/` com imagens relativas. O ID lógico identifica o
conteúdo; o installationId distingue cópias locais. Renomear não muda a pasta.

O manifesto v1 aceita a assinatura legada `loblox-experience`, preservada por
compatibilidade. Ícone quadrado e banner horizontal são os dois recursos de
apresentação atuais. `loadingBackground` antigo ainda é lido, mas não é um campo
novo da interface. Nenhuma terceira imagem é exigida.

Criação copia o mundo e personagem atuais da engine. Salvar grava uma revisão
antes de trocar o manifesto; preserva a atual e duas revisões anteriores. Isso
não substitui backup de disco. Importação `.lo` usa staging, limites, CRC e
validação de caminhos; não executa código durante extração. Lua só roda no teste.
O formato `.lo` mantém a assinatura LOEXP v1; é um contêiner próprio, não ZIP.

Pastas podem ser abertas diretamente. JSON legado é importado para cópia própria.
A Biblioteca permite localizar entradas movidas; remover uma entrada ausente dos
recentes não exclui arquivos. Arquivamento e exclusão permanente são distintos.
Leia [exclusão segura](EXCLUSAO_SEGURA.md), [padrões](PADROES_DA_ENGINE.md) e
[backup](BACKUP_E_RESTAURACAO.md).
