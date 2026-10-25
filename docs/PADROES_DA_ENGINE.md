# Mundo e personagem padrão LINBLUX

No Início, abra **Configurações → Editar mundo padrão** ou **Editar personagem
padrão**. O Studio cria uma cópia de edição na biblioteca. Edite objetos,
iluminação, spawn, propriedades físicas, corpo, juntas, materiais e animações.
No menu Arquivo, use **Salvar como padrão da engine** ou **Salvar personagem
como padrão da engine** (selecione o personagem no Explorador).
Salvar uma cópia aberta pelo comando de edição também atualiza o respectivo padrão.

Durante desenvolvimento no checkout, os padrões são salvos em
`assets/defaults`: `index.json` aponta para snapshots imutáveis em `revisions/`.
Adicione esses arquivos ao Git e use o fluxo de checkpoint para protegê-los no
GitHub. Recompilar não recria nem sobrescreve essa fonte. Novas experiências
recebem cópias dos recursos atuais, não referências mutáveis ao padrão.
Um projeto comum só modifica o padrão mediante o comando explícito.

`factory/world.loblox.json` e `factory/character.loblox.json` são os padrões de
fábrica versionados. A restauração exige confirmação e remove os ponteiros de
personalização; preserva revisões anteriores e experiências existentes.

## Formato

`index.json`: `format: linblux-engine-defaults`, `version: 1`, `world` e
`character` opcionais, caminhos relativos de snapshots. Cada snapshot usa o
manifesto de experiência v1, uma cena JSON v3 e assets relativos independentes.
As assinaturas JSON antigas são mantidas por compatibilidade. Versões futuras,
links e caminhos inválidos são recusados; uma carga inválida não substitui o projeto.
A gravação do snapshot precede a troca atômica do índice. Falhas de permissões
são exibidas; não se promete persistência quando a gravação não ocorreu.

Instalações carregam `share/linblux/defaults`; para desenvolvimento em outro
local use `LINBLUX_DEFAULTS_ROOT=/caminho/absoluto/versionado`. Um diretório de
instalação sem permissão de escrita exige um checkout ou override gravável.
Gerenciadores com diretório de dados explícito (testes/embutidos) gravam os
padrões isoladamente em `data/engine-defaults`, salvo raiz explícita no construtor.

## Personagem original e limites atuais

O padrão de fábrica é o **Sentinela LINBLUX**, com proporções, materiais, cores
e visor próprios. Preserva o formato de seis corpos/juntas para abrir animações
legadas. Controlador, colisão, câmera, movimento e animações fazem parte dos
recursos copiados. Experiências antigas mantêm seus personagens serializados.

O runtime atual ainda usa seis corpos e seis juntas. Importação de malhas e
esqueletos arbitrários NÃO está implementada; necessita evolução versionada do
formato, do renderizador e do animador. O editor permite modificar essa hierarquia
compatível, não qualquer esqueleto externo. Esse limite está no roadmap, sem
apresentar o antigo tipo serializado como uma implementação nova de malhas.
