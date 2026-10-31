# Backup e restauração

Repositório oficial: https://github.com/danielvtroot/LINBLUX (main).
O remoto `legacy` preserva o endereço anterior. A união dos históricos é um merge
normal. Em 31/10/2026, mensagens de commits foram revisadas com preservação
dos arquivos, autores e datas. Essa revisão mudou hashes; referências antigas
em relatórios históricos podem apontar para os identificadores anteriores.

Antes da migração em 24/10/2026 foi criado e verificado com `git bundle verify`
um backup em `/tmp/linblux-backup-20261024-205533`: `history.bundle`, cópia do
working tree (inclusive arquivos ainda não registrados), patch binário e SHA256SUMS.
**/tmp não é armazenamento permanente**. Uma segunda cópia local foi preservada
em `/home/labubu/projeto/linblux-backups/`, incluindo o snapshot complementar
`linblux-resume-20261024-212921`. Copie esses conjuntos para outro dispositivo;
nenhuma pasta no mesmo notebook protege contra perda física do equipamento.
O GitHub protege somente os arquivos efetivamente enviados. Builds, credenciais,
referências pessoais, logs e experiências pessoais não fazem parte desse backup remoto.
Faça cópias externas dessas experiências e de seus pacotes `.lo`.

## Verificação em 31/10/2026

A comparação com a API de árvores do GitHub confirmou os mesmos 497 arquivos
versionados e os mesmos IDs de conteúdo da main local. Builds são gerados e
precisam ser recompilados depois de clonar. Arquivos ignorados ou não rastreados
não estão incluídos nessa confirmação.

O repositório isolado do AutoBackup tinha objetos vazios. Sete objetos foram
recuperados da cópia principal, validando cada SHA original. A árvore e os
462 arquivos do checkpoint anterior voltaram a ser legíveis. Após reiniciar o
monitor, o checkpoint `a74da78cde730d6a25e6089438a8f55c106d0727` foi enviado,
sem erro registrado. Essa reparação não equivale a uma auditoria do HDD nem a
uma cópia externa dos dados privados.

Os três relatórios não rastreados, `docs/videos/linblux.mp4`, experiências em
`projects/`, imagens pessoais ignoradas e referências locais continuam fora da
main. Preserve cópias privadas em outro dispositivo ou armazenamento externo.

## Rotina de sincronização

1. Revise `git status`, `git diff` e arquivos novos; nunca adicione credenciais.
2. Compile e valide uma mudança funcional.
3. `git add <arquivos revisados>` e `git commit -m 'tipo: mudança concreta'`.
4. Execute `scripts/sync-safe.sh`. O script verifica o remoto, exige árvore limpa
   e faz push normal para main. Uma falha preserva os commits locais. Resolva
   divergências por fetch e merge revisado, preservando os dois lados.
5. Confira `git ls-remote origin main` contra `git rev-parse HEAD`.

## Recuperar outro computador

```sh
git clone https://github.com/danielvtroot/LINBLUX.git
cd LINBLUX
git fsck --full
# Siga as dependências e a compilação do README.
```

Offline: `git clone /caminho/history.bundle LINBLUX`, confira os hashes e extraia
`working-tree.tar.gz` em um diretório separado antes de reconciliar os arquivos.
As experiências do usuário devem ser restauradas de uma cópia própria ou importadas
de `.lo`. Não confunda o repositório da engine com backup automático dos mundos.

O comando `python3 scripts/checkpoint.py --help` documenta o checkpoint de uma
mudança revisada: arquivos explícitos, compilação, CTest, commit e push normal.
O script recusa pastas genéricas, links, arquivos ignorados, segredos conhecidos
e arquivos maiores que 50 MiB. Não substitui a revisão humana do conteúdo.

## AutoBackup independente do desenvolvimento

O serviço do usuário mantém checkpoints em `auto-backup`, com repositório e
índice próprios. Consulte [backup/README.md](backup/README.md) para instalação,
systemd/cron, política de arquivos e prova de recuperação. Na sessão de labubu:
`~/.local/bin/backup-status` ou `python3 tools/backup/backup.py status`.
Ao consultar como outro usuário, selecione explicitamente o diretório de estado.

Em 25/10/2026, o checkpoint remoto 36917d6c4474fb1358924785ff2b23b21881b27f
foi recuperado numa clonagem temporária: 315 arquivos conferidos por SHA-256,
incluindo o marcador público. Main e índice de desenvolvimento permaneceram
intactos. O watchdog cron reiniciou o processo encerrado intencionalmente em
20,005 segundos. Isso comprova o caminho de recuperação de código dessa execução,
não uma cópia externa de experiências pessoais/arquivos ignorados. O destino
privado externo ainda precisa ser definido e validado pelo proprietário.
