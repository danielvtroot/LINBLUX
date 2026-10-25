# Exclusão e arquivamento

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
