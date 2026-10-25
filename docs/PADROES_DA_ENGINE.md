# Mundo Padrão da Engine

O Início contém um cartão permanente **Mundo Padrão da Engine → Editar padrão**,
separado das experiências recentes. Ele abre o mundo e o personagem atuais juntos
no editor 3D. O documento de edição fica em `data/engine-workspaces`, protegido
contra exclusão e arquivamento, sem aparecer como experiência comum.

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

Em desenvolvimento no checkout, o destino é `assets/defaults`. O índice v1
`index.json` contém `format: linblux-engine-defaults`, `version: 1` e caminhos
relativos `world`/`character` para snapshots em `revisions/`. Esses recursos usam
manifesto v1, cenas JSON v3 e assets relativos. Assinaturas legadas permanecem
por compatibilidade. Versões futuras e caminhos inseguros são recusados.

Novas experiências recebem cópias independentes. Editar uma experiência comum
não muda o padrão. Projetos existentes nunca são atualizados automaticamente.
Recompilar copia os recursos-fonte; não recria nem sobrescreve o padrão versionado.

## Restaurar e recuperar

**Restaurar padrão de fábrica** exige confirmação e cria primeiro um backup do
índice em `assets/defaults/backups/*-index.json`. Revisões e suas imagens permanecem
em `revisions/`. Se o backup falhar, a restauração não prossegue. Ela publica os
recursos de `factory/`, preservando experiências e rascunhos existentes.

Para recuperar uma revisão, feche o Studio e restaure o índice de backup desejado
como `index.json`, mantendo as revisões referidas. Esse backup de índice depende
do restante da pasta defaults; copie/versione a pasta completa. Não substitui o
backup das experiências pessoais nem de preferências legadas da pasta de dados.

Registre `assets/defaults` no Git após revisar seus recursos e execute o fluxo de
checkpoint para protegê-los no GitHub. Instalações usam `share/linblux/defaults`;
`LINBLUX_DEFAULTS_ROOT=/caminho/absoluto/versionado` permite uma raiz de autoria
gravável. Erros de permissão são exibidos sem prometer uma gravação inexistente.
Gerenciadores de teste usam raízes isoladas, evitando modificar os recursos reais.

## Validação

`linblux_engine_default_lifecycle` cria um original, configura articulações,
animação, imagem, física, câmera e script, publica, reinicia em outro processo,
cria uma experiência e executa o personagem correto em movimento. Também testa
importação, publicação transacional, proteção, backup e preservação do projeto antigo.
`linblux_engine_default_restart` abre o executável real, usa o cartão, fecha e
reinicia o Studio, cria uma experiência pela UI e confirma no runtime o rig herdado.
