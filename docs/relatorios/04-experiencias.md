# Etapa 4 — experiências

Implementado: exclusão permanente real pelo gerenciador e pela confirmação em
pt-BR; nome explícito, aviso de remoção do disco, cancelamento, atualização da
biblioteca e status real. Arquivamento permanece separado. Percurso POSIX por
descritores protege a biblioteca e não segue links para diretórios externos.
Ícone e banner são os únicos campos visuais novos; metadados antigos continuam
legíveis sem exigir uma terceira imagem. Criação, importação `.lo`, abertura e
salvamento usam os mecanismos transacionais existentes.

Arquivos: ExperienceManager.*, ExperienceUI.cpp, tests/experiences_main.cpp,
tests/home_video_main.cpp, tests/experience_ui_main.cpp e EXCLUSAO_SEGURA.md.
Testes adicionados: ID adulterado, raiz externa, biblioteca como alvo, raiz
simbólica, destino externo de link interno preservado, vizinho preservado,
repetição de exclusão, fechamento do projeto ativo e índice sem entrada fantasma.
O teste gráfico cobre cancelar e confirmar exclusão pelo controlador real da UI.

Testes da rodada anterior: experiências e Home passaram (2/2). A rodada final
inclui a nova confirmação de exclusão e está registrada no log de validação.
Commit: consulte git log --grep='feat(experiences)'. Push normal para main.
Próxima etapa: recursos versionados de mundo e personagem padrão.

Rodada final: linblux_experiences e linblux_home_video passaram (2/2,
21,43 s), incluindo confirmação com nome, cancelamento e exclusão real.
