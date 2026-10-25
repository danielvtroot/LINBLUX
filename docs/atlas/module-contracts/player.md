# Módulo `player`

**Responsabilidade:** executável de jogo `linblux_player`, somente leitura por
padrão: interpreta argumentos e executa uma experiência em runtime.

**Fonte:** `src/player/main.cpp`.

**Entradas:** `--project CAMINHO`, `--validate CAMINHO`, `--frames N` e ambiente
gráfico. O caminho pode ser diretório, `experience.json` ou pacote `.lo`.

**Saídas:** execução da cena em `RuntimeSession`; em validação, código 0/1 e
mensagem; ajuda e erros de argumento em stderr.

**APIs principais:** `main`, `game::RuntimeSession`, `ExperienceManager` e
`projectio` carregados sem gravar no manifesto.

**Invariantes:** não linka Studio/ImGui/RmlUi; não regrava o projeto ao abrir;
um pacote `.lo` é importado para uma cópia local; argumentos inválidos retornam
2; falha de abertura/execução retorna 1.

**Dependências:** `linblux_engine`, `linblux_renderer`, `linblux_presentation`,
raylib/OpenGL e o runtime de jogo.

**Ciclo de vida e erros:** carrega cena → inicia sessão → loop de frames ou
validação → `stop` antes de sair. Erros de janela e projeto são reportados sem
alterar o arquivo de origem.

**Testes:** `linblux_player_help`, `linblux_player_validate`,
`linblux_engine_default_lifecycle` e testes de runtime que usam o mesmo
`RuntimeSession`.

**Como estender:** mantenha argumentos mínimos e compatíveis; toda nova opção
deve ter validação e teste de ajuda/erro. Não adicione dependência do Studio.

**Limites:** não há multiplayer/Server, editor embutido ou escrita de projeto.
O caminho gráfico exige janela/OpenGL; validação headless cobre só parte do
runtime.
