# Backup e restauração

Repositório oficial: https://github.com/danielvtroot/LINBLUX (main).
O remoto `legacy` preserva o endereço anterior. A união dos históricos é um merge
normal: nenhum commit anterior foi descartado e não se utiliza push forçado.

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
