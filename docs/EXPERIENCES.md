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
Leia as regras de exclusão abaixo, [padrões](PADROES_DA_ENGINE.md) e
[backup](BACKUP_E_RESTAURACAO.md).

## Exclusão e arquivamento

**Arquivar** mantém a pasta no disco e permite restaurar pelo Arquivo.
**Excluir permanentemente** pede confirmação com o nome da experiência e avisa
que os arquivos serão apagados do disco. Cancelar não modifica o diretório.

O gerenciador aceita somente um filho direto da biblioteca cujo nome é seu
installationId validado. Confere o ID lógico e o manifesto antes de excluir.
Recusa a própria biblioteca, raízes externas, manifesto trocado e raiz simbólica.
O percurso usa descritores de diretório POSIX, openat/O_NOFOLLOW, fstatat,
renameat e unlinkat: links internos são removidos sem seguir seus destinos.
A pasta é reservada com nome `.deleting-ID-...`; inode e dispositivo são
verificados após o rename. Os vizinhos não são percorridos.

Erros reais são exibidos. Uma falha durante remoção pode deixar uma pasta
`.deleting-*` com os arquivos restantes: o erro informa esse caminho e não
promete desfazer uma exclusão já iniciada. Faça backup dos projetos importantes.
A atualização do índice ocorre depois da remoção dos arquivos; erros do índice
são informados separadamente. Projetos abertos fora da biblioteca devem ser
importados para uma cópia interna antes da exclusão pela biblioteca.

## Serviços e recursos armazenados

ServerStorage e ReplicatedStorage são contêineres persistidos junto da cena,
inclusive seus Modelos, Parts, scripts, atributos e referências de recursos.
A renderização e a física usam apenas a árvore do Mundo; o armazenamento
continua acessível pela hierarquia e API Lua. Salvar/reabrir preserva pais e IDs.
Play usa uma cópia: mover, clonar ou instanciar durante a execução não modifica
a autoria; Stop descarta essas alterações de partida. Não há replicação em rede.
