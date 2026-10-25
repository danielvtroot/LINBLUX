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
