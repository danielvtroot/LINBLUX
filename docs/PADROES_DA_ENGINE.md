# Mundo Padrão da Engine

O Início contém um cartão permanente **Mundo Padrão da Engine → Editar padrão**,
separado das experiências recentes. Ele abre o mundo e o personagem atuais juntos
no editor 3D. O documento de edição fica em `engine-workspaces`, dentro da raiz de
dados do usuário, protegido contra exclusão e arquivamento e sem aparecer como
experiência comum.

Edite objetos, iluminação, céu, spawn, atributos/componentes, scripts, articulações,
materiais, imagens, animações e configurações de física, movimento e câmera que
a engine suporta e serializa. Gravidade e colisões do jogador ficam nas configurações
do jogo; o runtime aplica esses valores. Terreno avançado e malhas externas ainda
não são sistemas implementados nesta base.

## Substituir o personagem

Exclua o personagem atual no Explorador e crie um original no editor de personagem,
ou use **Importar personagem…** para um projeto JSON, manifesto/pasta ou pacote
`.lo` compatível. A importação copia imagens e animações, cria IDs independentes e
preserva o arquivo de origem. Não é um importador de FBX/glTF ou de malhas arbitrárias.

No editor de personagem é possível alterar corpos, nomes, materiais, proporções,
conexões, poses e a hierarquia topológica de articulações. O formato atual preserva
seis corpos/juntas para compatibilidade. **Usar como jogador** associa o rig ao
controlador nativo configurável; ajuste movimento, colisão, câmera e animações nas
configurações do jogo. Scripts do mundo podem personalizar o comportamento.
Não se apresenta um controlador arbitrário de terceiros como já implementado.

## Publicar o padrão

Use o botão do cabeçalho **Salvar como padrão da engine**, ou a mesma ação em
Arquivo. Há uma confirmação explícita. Mundo, personagem selecionado como jogador,
animações, imagens, scripts e configurações são gravados em snapshots antes de
publicar ambos os ponteiros numa única troca atômica do índice. Uma falha não
publica metade da dupla. **Salvar rascunho / Ctrl+S** guarda somente a edição;
não publica o padrão sem confirmação. **Definir como padrão da engine**, no editor
de personagem, publica a dupla usando o personagem selecionado.

### Onde o padrão é gravado

O padrão personalizado do usuário fica em
`$XDG_DATA_HOME/linblux/engine-defaults` ou, sem `XDG_DATA_HOME`, em
`~/.local/share/linblux/engine-defaults`. O Studio nunca grava em
`build-linblux`, `share/linblux/defaults` nem em uma instalação de sistema.
`share/linblux/defaults` (e `assets/defaults` no checkout) é o padrão de fábrica
somente leitura, usado quando ainda não existe um padrão personalizado válido.
A escolha é feita na ordem: padrão personalizado → padrão de fábrica.

O índice v1 `index.json` contém `format: linblux-engine-defaults`, `version: 1`
e caminhos relativos `world`/`character` para snapshots em `revisions/`. Esses
recursos usam manifesto v1, cenas JSON v3 e assets relativos. Assinaturas legadas
permanecem por compatibilidade. Versões futuras e caminhos inseguros são recusados.

Novas experiências recebem cópias independentes. Editar uma experiência comum
não muda o padrão. Projetos existentes nunca são atualizados automaticamente.
Recompilar copia os recursos-fonte; não recria nem sobrescreve o padrão versionado.

### Publicar no código-fonte da engine

Para publicar deliberadamente o padrão nos recursos versionados do LINBLUX,
inicie o Studio com uma raiz de autoria absoluta e explícita:

```bash
LINBLUX_DEFAULTS_ROOT=/caminho/do/checkout/assets/defaults ./build-linblux/linblux
```

O caminho precisa ser absoluto e gravável. O Studio verifica a escrita antes de
criar as revisões, valida os snapshots, grava o índice por troca atômica e guarda
o índice anterior em `backups/`. Sem essa variável, o botão grava somente no
diretório de dados do usuário; não tenta simular publicação no código-fonte.
Revise o diff de `assets/defaults` e faça o checkpoint/commit conscientemente.

## Restaurar e recuperar

**Restaurar padrão de fábrica** exige confirmação e cria primeiro um backup do
índice personalizado em `<dados-do-usuário>/engine-defaults/backups/*-index.json`.
Revisões e suas imagens permanecem em `revisions/`. Se o backup falhar, a
restauração não prossegue. Ela publica os recursos de `factory/` no índice
personalizado, preservando experiências e rascunhos existentes.

Para recuperar uma revisão, feche o Studio e restaure o índice de backup desejado
como `index.json` na raiz personalizada, mantendo as revisões referidas. Esse
backup de índice depende do restante da pasta de padrões; copie/versione a pasta
completa. Não substitui o backup das experiências pessoais nem de preferências
legadas da pasta de dados.

Registre `assets/defaults` no Git somente após usar a raiz de autoria explícita
acima e revisar seus recursos. Instalações usam `share/linblux/defaults` como
fábrica somente leitura. Erros de caminho ou permissão são exibidos sem prometer
uma gravação inexistente. Gerenciadores de teste usam raízes isoladas, evitando
modificar os recursos reais.

## Validação

`linblux_engine_default_lifecycle` cria um original, configura articulações,
animação, imagem, física, câmera e script, publica, reinicia em outro processo,
cria uma experiência e executa o personagem correto em movimento. Também testa
importação, publicação transacional, proteção, backup e preservação do projeto antigo.
`linblux_engine_default_paths` prova que a raiz personalizada é separada da
fábrica somente leitura, que `XDG_DATA_HOME` é respeitado, que a herança é
independente e que um destino bloqueado falha com mensagem clara.
`linblux_engine_default_restart` abre o executável real com dados XDG isolados,
usa o cartão, salva/reinicia o Studio, cria uma experiência pela UI e confirma no
runtime o rig herdado.
