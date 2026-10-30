# LINBLUX em imagens

Capturas reais do Studio e Player. A interface oficial atual é em português brasileiro.
As capturas usam renderização por software; não representam desempenho em GPU física.

<!-- LINBLUX:MEDIA:START -->

### Capturas

<a href="prints/studio-construcao.png"><img src="prints/studio-construcao.png" width="480" alt="studio-construcao"/></a>
<p>studio-construcao</p>

### Vídeos

Nenhum vídeo público adicionado. As demonstrações disponíveis são as capturas acima.

<!-- LINBLUX:MEDIA:END -->

## Atualizar a galeria

1. Confira se a captura mostra somente LINBLUX, sem dados pessoais ou conteúdo sem autorização.
2. Coloque PNG/JPEG/WebP em `docs/prints/`, ou MP4/WebM em `docs/videos/`.
3. Execute na raiz: `python3 tools/update_readme_media.py`.
4. Revise `git diff -- docs/MIDIA.md` e adicione somente os arquivos públicos escolhidos.

O README aponta para esta página. O script altera somente o bloco entre marcadores,
não abre o conteúdo dos vídeos, não transcodifica e não adiciona arquivos ao Git.
Nomes ocultos, temporários, symlinks, arquivos vazios e formatos não aceitos são ignorados.
Imagens acima de 8 MiB e vídeos acima de 20 MiB são omitidos com aviso.
Esses limites editoriais evitam aumentar o custo de clonar o código.

Uma imagem com o mesmo nome-base do vídeo vira sua capa (por exemplo,
`prints/construcao.png` e `videos/construcao.webm`). Sem capa, haverá um link textual.
O GitHub pode abrir ou baixar o arquivo; não se promete reprodução embutida no README.

Vídeos maiores devem ser publicados como assets de uma GitHub Release ou em um
serviço de vídeo apropriado, com direitos confirmados. Adicione o link manualmente
fora do bloco gerado. Não mova mídia pessoal para estas pastas só para aparecer na galeria.
`midia/careditfullhd.mp4` não é coletado nem incluído automaticamente.
