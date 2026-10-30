# LINBLUX Alpha 0.1.0 — notas preparadas para pre-release

**Estado: não publicada.** Tag proposta: `v0.1.0-alpha.1`.
Código e documentação estão em `main`. O binário continua candidato interno
até concluir as obrigações de redistribuição das dependências.

LINBLUX é um ambiente experimental de criação 3D para Linux, com Studio e Player
independentes, Lua 5.4, Bullet e raylib/OpenGL. A interface atual é pt-BR.

## Destaques

- Cena, hierarquia compacta, propriedades e ferramentas de Models.
- Move/Scale por face, Snap e encaixe por superfície, sem sobreposição de grade.
- Lua, NPCs/rigs no escopo atual, áudio, PNG/JPEG, Decals e persistência local.
- RifleDemo com inventário e interação no Player.
- ATLAS e TURBO locais para investigação e validação supervisionada.

## Requisitos e instalação

Linux x86_64 com OpenGL/X11 e dependências listadas no manifesto do candidato.
Compatibilidade em outras distribuições não foi comprovada.
Enquanto o pacote não é liberado, siga o [guia de build](../BUILD.md).
O candidato extraído usa `./bin/linblux` e `./bin/linblux_player --project CAMINHO`.
SHA-256 e evidência de empacotamento estão no [relatório](ALPHA_0_1_0_LANCAMENTO.md).

## Limites e segurança

Alpha experimental; faça cópias dos projetos e execute experiências confiáveis.
Não há multiplayer, contas ou publicação online. Rigs arbitrários, terreno e
áudio 3D avançado ficam para versões futuras. Não houve teste de GPU/áudio físicos
nem suíte FULL. Leia [estado da Alpha](../ALPHA_0_1_0.md) e
[recursos/licenças](../ASSETS_E_LICENCAS.md).

[Changelog](../CHANGELOG.md) · [Créditos](../ASSETS_E_LICENCAS.md) · [MIT](../../LICENSE)
