# Etapa 5 — mundo e personagem padrão

Implementado: recursos de fábrica versionados em assets/defaults; índice v1 e
snapshots imutáveis/atômicos, comandos de editar e salvar mundo e personagem,
configurações no Início, herança independente na criação e confirmação de
restauração. O Sentinela LINBLUX usa proporções, cores e visor originais.
Preservam-se cenas antigas, propriedades e clipes compatíveis. O diretório-fonte
dos padrões de desenvolvimento é mantido fora dos builds gerados.

Arquivos: ExperienceManager.*, ExperienceUI.*, assets/defaults, home.rml,
experiences.rcss, tests/defaults_main.cpp, tests/home_video_main.cpp, CMakeLists.txt.

Testes: defaults, experiências, jogo, spawn e Home passaram (5/5). A suíte nova
verifica edição, salvar/reabrir, herança de mundo/corpo, independência, restauração
sem sobrescrever experiências e recusa de versão futura. O teste gráfico também
exercita os comandos de edição/salvamento e confirmação pelo controlador da UI.

Limite explícito: corpo/esqueleto arbitrário e importação de malha externa ainda
não existem no runtime. O padrão é independente visualmente, com armazenamento
editável; o formato de seis juntas permanece para compatibilidade. Não se declara
concluído o suporte geral a personagens externos.

Commit: git log --grep='feat(defaults)'. Push normal para main.
Próxima etapa: separação Engine/Studio/Player, documentação, licenças e regressões.

Rodada ampliada: 8/8 suítes passaram em 26,08 s, incluindo os comandos de padrões
na UI. Após ajustar IDs de novas inserções e a descoberta da biblioteca legada,
experiências/jogo/padrões passaram novamente (3/3). Novas inserções e o fallback
nativo usam o Sentinela; createR6 permanece disponível para compatibilidade.
