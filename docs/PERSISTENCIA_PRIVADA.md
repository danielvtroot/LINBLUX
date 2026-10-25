# Persistência privada por experiência

Formato de progresso v1: um arquivo JSON com version=1, owner (escopo atribuído
pelo host) e values (objeto de dados). Não é parte do formato autoral da cena.
Escopos de instalação usam `experience:<installationId>`; JSON avulso usa
`scene:<caminho canônico>`. O nome da pasta é FNV-1a 64 do escopo; o conteúdo
owner é conferido antes da leitura, portanto colisão não mistura dados.
FNV é identificação de arquivo, não criptografia ou proteção por segredo.

Pasta padrão: XDG_STATE_HOME/linblux/progress ou ~/.local/state/linblux/progress.
Código privado em ProgressStore.{h,cpp}; opções são passadas exclusivamente pelo
Studio/Player a RuntimeSession::start. APIs Lua descritas em SCRIPTING_LUA.md.

Gravação substitui progress.json por rename atômico no mesmo diretório, depois
de fsync do temporário e antes de fsync do diretório. Trava de arquivo é mantida
durante a sessão. Caminhos são abertos com descritores relativos e O_NOFOLLOW;
arquivo não regular, versão desconhecida, owner divergente ou corrupção recusam
a abertura, preservando os bytes existentes. Não há reset silencioso.

Gravação é síncrona e pequena, agendada no fim da atualização, com intervalo
mínimo de 2 s; pausa e saída tentam gravar a versão final. Limites de dados e
operações impedem scripts de disparar escrita ilimitada por quadro. Isso não
garante latência de disco em hardware com falha. Erro de I/O é exibido no log/
saída de runtime e deixa pending=true até nova tentativa ou encerramento.

--validate, padrão da engine e cenas sem identidade usam memória, sem arquivos.
Cancelamento de carregamento usa stop(false) para descartar a sessão pendente.
Os assets, o manifesto, o mundo e os scripts não são regravados por esse sistema.

Backup: estado privado não vai ao GitHub nem ao pacote público da experiência.
Inclua a pasta progress e as instalações correspondentes no backup privado
externo. Restaure com o mesmo proprietário/permissões e a experiência fechada.
Instalações preservam identidade ao mover; nova importação gera identidade nova.
Cenas avulsas movidas de caminho usam novo escopo. Arquivo de progresso inválido
deve ser restaurado da cópia privada, não apagado automaticamente pela engine.
