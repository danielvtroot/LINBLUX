# Loblox Experiences 1.0

Abrir `build/loblox` sem argumentos mostra Home. `--project caminho` continua
abrindo um JSON legado diretamente e também aceita uma pasta de experiência,
`experience.json` ou um pacote `.lo`. Importar JSON pela Home cria uma cópia;
o arquivo original não é alterado. Nenhum script roda na importação.

Cada instalação local tem uma pasta com ID independente do nome. O manifest
`experience.json` v1 contém identidade, apresentação, datas, template e o caminho
da revisão principal. `places/<id>.loblox.json` contém o mundo no formato
ProjectIO existente: objetos, hierarquia, Lua, rigs, clips e configuração de
player continuam juntos nessa fonte de dados. `assets/` guarda PNG/JPEG locais
por conteúdo. `.cache/` é descartável e não é exportada.

Salvar grava uma revisão imutável antes de trocar atomicamente o manifest.
São mantidas a revisão ativa e duas anteriores. A biblioteca fica em
`$XDG_DATA_HOME/loblox`, ou `~/.local/share/loblox`; sua pasta de experiências
pode ser escolhida na Home. Não há mudança do CWD do processo.

`File` no Studio oferece exportação, retorno à Home e snapshots My Default.
Factory templates são arquivos versionados em `assets/templates/`, copiados
para os recursos de distribuição pelo CMake. My Default e My Default Character
ficam nos dados do usuário; atualizações não os substituem. Edit My Default
abre uma cópia de trabalho cujo Save atualiza o snapshot padrão.

Delete é recuperável: move a entrada para Archive, preservando a pasta e os
assets. Restore reativa a experiência. Não existe exclusão permanente automática.
Alterações não salvas exigem Save, Discard ou Cancel ao sair para Home.

## Pacote `.lo`

LOEXP v1 é um contêiner binário próprio, sem compressão. Não é ZIP nem JSON
renomeado. PNG/JPEG já possuem compressão; o leitor não necessita de executável
externo. Todos os inteiros são unsigned little-endian.

| Campo | Bytes |
|---|---:|
| Assinatura `LOEXP`, versão `01`, CR, LF | 8 |
| Quantidade de entradas | 4 |
| Para cada entrada: tamanho do caminho UTF-8 | 2 |
| Tamanho do conteúdo | 8 |
| CRC32 do conteúdo | 4 |
| Caminho, sem terminador | variável |
| Conteúdo | variável |

Só são aceitos `experience.json`, `places/...` e `assets/...`. Entradas são
arquivos regulares; não há representação de symlink. Caminhos absolutos,
`..`, `.`, NUL, controles, barras invertidas, duplicatas e dados extras são
rejeitados. Limites: 2048 entradas, 64 MiB por arquivo, 256 MiB de payload e
256 MiB somados de imagens decodificadas. Versões desconhecidas são rejeitadas.
CRC verifica integridade; o pacote não possui assinatura de autoria.

A importação valida numa pasta temporária privada, inclusive ProjectIO e
imagens, antes de publicar. IDs internos do mundo e da experiência permanecem;
o ID de instalação é novo para evitar sobrescrever projetos com o mesmo nome.
Exportação usa arquivo temporário, flush/fsync e rename. Scripts só executam
ao iniciar Play, sob os contratos Lua existentes.

## Imagens e diálogos

PNG usa o decoder raylib existente. JPEG usa somente o módulo JPEG de stb_image,
com licença vendorizada em `third_party/stb/`. Preflight limita arquivo a 32 MiB,
dimensão máxima a 16384 e área a 16 megapixels (até 64 MiB em RGBA). Não há
dimensão obrigatória: 512×512 e 1920×1080 são recomendações de apresentação.
O upload 3D consulta `GL_MAX_TEXTURE_SIZE` e reduz somente a cópia GPU quando
necessário. Miniaturas usam crop 16:9 em cache; os originais não são alterados.

O chooser Linux utiliza zenity, yad ou kdialog, localizado no PATH. Ele roda
como processo transitório, sem shell e sem novo contexto OpenGL. Pipe e waitpid
são consultados pelo loop normal, sem bloquear RmlUi. Na ausência de um helper,
o formulário permite informar o caminho e apresenta uma mensagem específica.

Loading apresenta identidade da experiência antes de preparar o runtime numa
tarefa própria. O mundo de autoria fica congelado; o loop continua processando
eventos. A thread prepara uma cópia independente e é sincronizada por future
antes de qualquer leitura do runtime pela UI. Uma transição curta encerra a
apresentação quando os sistemas estão prontos; não há espera falsa de carregamento.
As imagens da apresentação são liberadas ao entrar no jogo ou cancelar.

A biblioteca oferece busca e paginação de 24 cards para limitar texturas
residentes. Entradas movidas permanecem no histórico com Locate; elas nunca
são recriadas silenciosamente. Restore em Archive recupera a mesma instalação.

Instalação relocável: `cmake --install build --prefix DESTINO --component Loblox`.
O binário procura recursos em `share/loblox` junto ao build ou acima de `bin`.

Referências conceituais oficiais:
[ícones](https://create.roblox.com/docs/production/publishing/experience-icons),
[thumbnails](https://create.roblox.com/docs/production/publishing/thumbnails),
[loading](https://create.roblox.com/docs/players/loading-screens),
[place files](https://create.roblox.com/docs/projects/place-files).
O Loblox usa formato, código e assets próprios e funciona localmente.
