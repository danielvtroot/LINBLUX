# Backup automático do LINBLUX

Checkpoints contínuos do projeto, em um branch separado (`auto-backup`), com
repositório e índice isolados. A `main` nunca recebe commits de backup e o
índice Git do desenvolvedor nunca é alterado.

## Garantias

- **Isolamento:** o estado vive fora do projeto, em
  `~/.local/state/linblux-backup/` (repositório `work/`, `state.json` e
  `backup.lock`). O projeto é apenas lido (`git ls-files`).
- **Cadência:** verifica alterações a cada 15 s e cria checkpoint a cada
  120 s quando há mudanças (valores em `tools/backup/config.json`).
- **Somente lista explícita:** os arquivos vêm de `include` (lista explícita)
  e passam por `exclude`, limites de tamanho e varredura de credenciais.
  `git add -A` não é usado: cada caminho alterado é adicionado nominalmente.
- **Offline:** sem rede o checkpoint fica no repositório isolado e é reenviado
  automaticamente depois; `backup-status` mostra a pendência.
- **Sem force push:** um push rejeitado é reconciliado com `fetch` + `rebase`
  no repositório isolado. O histórico remoto nunca é reescrito.
- **Sem dados privados:** `.env`, chaves, `*.pem`, `projects/`, `referencias/`,
  builds, caches, logs, mídia pesada e arquivos acima de 4 MiB ficam de fora.
  Arquivos com credenciais reconhecíveis são ignorados e listados no estado.
  A varredura identifica padrões conhecidos; não prova ausência de todo segredo.
- **Snapshot consistente por arquivo:** os bytes verificados são os mesmos
  enviados. Links em qualquer componente do caminho são recusados. O conjunto
  não é uma transação de todos os editores: arquivos em edição podem representar
  instantes diferentes, identificados pelo checkpoint; valide antes de usar como release.
- **Concorrência:** daemon e `run --once` compartilham a trava. O estado é fora
  do projeto e branches main/master são recusados como destino de backup.

## Comandos

```bash
python3 tools/backup/backup.py status        # último checkpoint, push e pendências
python3 tools/backup/backup.py run           # daemon de monitoramento
python3 tools/backup/backup.py run --once    # um único ciclo (script/CI)
python3 tools/backup/backup.py verify        # clonagem limpa + conferência de hash
python3 tools/backup/backup.py init          # prepara o repositório isolado
```

`backup-status` é instalado em `~/.local/bin` e é o mesmo comando de `status`.

## Serviço systemd de usuário

```bash
tools/backup/install-service.sh
```

O instalador grava `~/.config/systemd/user/linblux-backup.service`
(`Restart=on-failure`, `RestartSec=20`, `WantedBy=default.target`), habilita o
serviço e instala o comando `backup-status`. O daemon roda com `python3` do
sistema e não depende de OpenCode, desenvolvimento, desenvolvimento nem do Studio aberto.

Sem um gerenciador systemd de usuário disponível, o instalador usa o **cron do
próprio usuário**: uma entrada `@reboot` e um watchdog a cada minuto. Ele preserva
as demais entradas do crontab. O monitor verifica mudanças a cada 15 s e faz
checkpoints a cada 120 s; o watchdog só inicia o processo se a trava indicar que
nenhum monitor está ativo. Uma falha é recuperada em até um minuto mais o tempo
de início. Não depende de login gráfico. O daemon cron do sistema precisa estar
ativo. Não se instala systemd sobre outro init nem se altera a main.

```bash
python3 tools/backup/backup.py service install --backend auto
python3 tools/backup/backup.py service start   # idempotente
python3 tools/backup/backup.py service restart # carregar código/configuração nova
crontab -l                                  # confira as duas linhas LINBLUX
python3 tools/backup/backup.py status --json
```

Execute a instalação **como o dono do projeto**. Estado root e estado labubu são
independentes. Aqui o serviço ativo pertence a labubu, com estado em
`/home/labubu/.local/state/linblux-backup`. Se consultar como outro usuário,
informe explicitamente `--state-dir`. Logs do backend cron: `daemon.log` e
`watchdog.log` nesse diretório, com permissões privadas.

```bash
systemctl --user status linblux-backup.service
journalctl --user -u linblux-backup.service -f
```

## Restauração

```bash
git clone --branch auto-backup https://github.com/danielvtroot/LINBLUX.git linblux-restore
# ou apenas um arquivo:
git show auto-backup:src/editor/EditorUI.cpp > EditorUI.cpp
```

Para conferir sem restaurar de verdade:

```bash
python3 tools/backup/backup.py verify
```

O branch de backup tem somente a allowlist: não é uma distribuição completa.
Para recompilar, clone também main num diretório novo, siga o README e compare
com o checkpoint antes de trazer as alterações desejadas. Main preserva o
histórico e recursos públicos fora da política, como o vídeo oficial versionado.
Não copie o checkpoint sobre a única cópia de uma experiência em uso. Um ambiente
novo de backup busca o histórico remoto existente antes do primeiro checkpoint.

A prova online optativa `tests/backup/remote_recovery.py` clona GitHub num diretório
temporário, confere todo o manifesto e o marcador público, e verifica que main e
o índice original não mudaram. Exige baseline/estado explícitos e supervisor
TURBO; não roda na suíte offline nem publica arquivos por conta própria.

## Dados privados e recursos fora do Git

`protected_files` conta a cobertura local da allowlist; `remote_protected_files`
conta bytes do manifesto atual confirmados em um checkpoint enviado. Um push
pendente não protege contra perda do notebook. Nenhum desses números cobre
experiências em projects/, arquivos ignorados, credenciais ou pastas pessoais.

O vídeo e a logo oficiais estão versionados na main; só a logo entra na allowlist
do AutoBackup. Vídeos pessoais e assets ignorados não entram em nenhum envio
automático público. Para estes dados, mantenha os diretórios completos de cada
experiência e exportações `.lo` em cópia privada num disco externo/NAS, com versões
por data. Inclua recursos importados, scripts e manifestos; abra uma cópia e
importe seu `.lo` em uma biblioteca separada para testar a recuperação.

O destino privado externo ainda não foi informado nesta execução. Não há cópia
externa privada configurada ou comprovada; uma pasta no mesmo notebook não
substitui essa proteção. Não se envia dados privados ao GitHub para preencher
essa lacuna. Após definir o destino, registre origem, periodicidade, retenção,
permissões/criptografia do armazenamento e uma restauração real nesse destino.

## Configuração

`tools/backup/config.json` define `include`, `exclude`, limites e cadência.
Um ajuste local pode ser colocado em `~/.config/linblux/backup.json` (mesmas
chaves, mescladas sobre o arquivo do projeto) ou apontado por
`LINBLUX_BACKUP_CONFIG`. A configuração é lida na inicialização: depois de
editá-la, reinicie o daemon (`systemctl --user restart linblux-backup`).

Caminhos úteis: `LINBLUX_BACKUP_STATE` (estado), `LINBLUX_BACKUP_PROJECT`
(projeto) e `--state-dir`, `--project`, `--config`.

## Verificação contínua

O daemon grava cada ciclo no log do systemd (ou em
`~/.local/state/linblux-backup/daemon.log` quando iniciado manualmente). Uma
instância por usuário é garantida por trava exclusiva em `backup.lock`: se
outro processo já monitora, o novo encerra sem duplicar checkpoints.

## Testes

```bash
python3 -m unittest discover -s tests/backup -p 'test_*.py'
```

Os testes cobrem allowlist e exclusões, limites de tamanho, bloqueio de
credenciais, checkpoint offline com envio posterior, push rejeitado sem
reescrita de histórico, `backup-status`, trava de instância única e
recuperação por clonagem limpa.

## Limitações

- O branch `auto-backup` é órfão: guarda o conteúdo, não o histórico da `main`.
- O primeiro checkpoint envia o conjunto atual de arquivos; checkpoints
  seguintes são incrementais.
- Binários grandes e arquivos ignorados pelo Git ficam fora por padrão; para
  incluí-los é preciso ajustar `include`/limites de forma explícita.
