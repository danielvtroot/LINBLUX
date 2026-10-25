# Missão de conclusão — Fase 0: proteção automática

Implementação: serviço independente do Studio/agente, executado como labubu.
Systemd de usuário é suportado; neste host SysV, o backend cron instala @reboot
e watchdog por minuto, preservando outras tarefas. Monitor ativo, polling 15 s,
checkpoints 120 s, repositório/índice isolados e push normal para auto-backup.
Comando de status instalado em ~/.local/bin/backup-status. Restart identificado
por UID/cmdline, com encerramento do ciclo antes de nova instância.

Corrigidos: instalador que não iniciava sem systemctl; ciclo avulso sem trava;
leitura/cópia de bytes diferentes sob edição concorrente; links intermediários;
inicialização em ambiente novo com backup remoto existente; recusa de main/master
e estado dentro do projeto. Scanner inclui tokens com prefixos de projeto e
tokens GitHub granulares. O status distingue cobertura local/remota e política.
Não é uma garantia de detecção universal de segredos.

Arquivos: tools/backup, tests/backup, packaging/systemd, docs/backup,
docs/BACKUP_E_RESTAURACAO.md, contrato backup, CMakeLists, duas linhas de seleção
no TURBO e tests/turbo/test_backup_selection.py. Alterações preexistentes do
TURBO e do editor permanecem fora do commit.

ATLAS localizou os arquivos existentes; impact do runner Python não mapeou teste.
Registrado CTest linblux_backup e seleção explícita no TURBO, com regressão própria.
Build Studio pelo TURBO: 18,30 s. FAST inicial falhou por indentação introduzida
no store; corrigido. FAST final 20261025T151827Z-14019: 4/4, zero falhas/timeouts,
build 2,5 s, testes 10,2 s, total 12,9 s. Cobertura global desconhecida por CMake
e diffs alheios; não é aprovação integral da engine.

IMPACT 20261025T151957Z-16019: 17/17 selecionados, zero falhas/timeouts em 54 s.
Sem recompilar executáveis inalterados; CMake já reconfigurado e testes Python
registrados. Gráficos excluídos por escopo de ferramenta (não houve alteração
visual); cobertura global parcial. A regressão final de segurança/restart contém
20 testes offline, passou pelo supervisor TURBO em 3,217 s (timeout 45 s).
IMPACT final, após scanner/restart: 20261025T152516Z-20625, 17/17 sem falhas ou
timeouts, 65,6 s em testes/66,7 s totais, mesma cobertura parcial sem gráficos.

Prova online supervisionada: checkpoint 36917d6c4474fb1358924785ff2b23b21881b27f,
315 arquivos restaurados em clone temporário e conferidos por SHA-256. Marcador:
e4874dbed0e5503ba1cc579f0ab3b261974b837fc5a523ffa85cddd94ce35430.
Main cf53b09 e hash do índice permaneceram idênticos ao baseline anterior ao
início do monitor. Prova TURBO em 2,117 s; log .turbo/logs/phase0-remote-recovery.log.
O marcador existia apenas localmente quando o monitor o enviou: não foi push
manual de desenvolvimento. Encerramento do PID identificado 11534 e recuperação
pelo cron com PID 13052 em 20,005 s. Versão final reiniciada e ativa (PID 19995
na verificação posterior); PIDs são evidências históricas, não identificadores
permanentes. Não foi reiniciado o computador para testar o boot.

Dados privados: projects/, ignorados e pastas pessoais não vão ao GitHub. A logo
oficial entra no backup; o vídeo oficial permanece versionado na main e fora
dos checkpoints leves. Estratégia privada separada documentada. Destino externo
ainda não informado; cópia privada externa NÃO configurada/comprovada. Isso é
pendência explícita, não suprida por uma pasta no mesmo notebook.

Missão integral pendente: próxima fase é completar Lua/scripting sem reimplementar
as APIs existentes. Ver CONCLUSAO_ENGINE.md para o inventário e a continuidade.
