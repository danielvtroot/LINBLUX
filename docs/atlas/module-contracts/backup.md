# Módulo backup

Fonte: tools/backup/linblux_backup/{selection,store,runner,service,cli,state,config}.py.
Entrada: allowlist revisada, working tree somente leitura, remoto origin,
configuração do usuário. Saída: commits na branch auto-backup, estado/logs no
diretório privado do usuário, status e recuperação por clonagem temporária.

Invariantes: não altera branch/main/index da engine; não usa push forçado;
snapshot contém exatamente os bytes analisados; recusa caminhos com links e
estado dentro da área de trabalho; main/master não são destinos de backup;
trava comum para daemon e ciclo avulso. Exceções e segredos não devem ser
silenciados como sucesso. Estado é escrito por rename atômico e fsync.

Serviço: systemd de usuário quando disponível, cron por usuário como alternativa
real em Linux com outro init. @reboot e watchdog por minuto. Não depende do
Studio nem do agente; mantém commits offline para envio posterior.

Testes: linblux_backup (offline, Git real em diretórios temporários, processos,
concorrência, links, política, restauração). linblux_turbo inclui seleção de
backup nos modos FAST/IMPACT. tests/backup/remote_recovery.py é prova online
optativa supervisionada, não executada automaticamente na suíte offline.

Limites: padrões conhecidos não detectam todo segredo; allowlist não inclui
arquivos privados/ignorados; estado/checkpoint local não protege contra perda
física do notebook. Git main e recursos omitidos devem ser preservados à parte.
Veja [o manual de backup](../../backup/README.md) e
[o histórico técnico](../../historico/README.md).
