# LINBLUX Alpha 0.1.0 — lançamento

Data: **30/10/2026**. Escopo: estabilização e apresentação da Alpha Linux.
**Código e documentação publicados em `main`. Release/tag/binário público não publicados.**
O candidato interno passou no smoke; obrigações de redistribuição ainda bloqueiam o binário.

## Código

| Item | Resultado desta missão |
| --- | --- |
| Explorador compacto | PASSOU: linhas de 21 px, recuo de 12 px por nível, menos padding, ícones e texto alinhados; seleção/filtro/rolagem/expansão em 1366×768 e 1920×1080 |
| Nomes e dados autorais | Preservados; nenhum objeto antigo renomeado, nenhum espaço de nome removido, nenhum `.lo`, Part, pasta ou script pessoal apagado |
| Grade visual | REMOVIDA do WIP: desenho, controles, CSS, ações, estado, acessores de teste e documentação ativa; essa funcionalidade ainda não fazia parte do HEAD inicial |
| Snap | PRESERVADO: incrementos Move/Scale, relativo/absoluto, contato, prévia, cancelamento, undo/redo e persistência |
| Move/Scale | Correção por face preservada; não reimplementada. Headless de manipulação aprovado. Não se repetiu a suíte gráfica longa específica de faces |
| Áudio | PASSOU em sink ALSA null: trilho/marcador compartilhados com Câmera, 0/25/50/75/100%, clique, arraste fora do controle, teclado e mudo/desmudo; painel de sons em HD/FHD |
| Decal | PASSOU: regressão de 53 checks com fixture própria e verificação da orientação da logo no RifleDemo; nenhuma inversão de UV nesta missão |
| PNG/JPEG e persistência | PASSOU: `linblux_image_assets`, além dos roundtrips de construção e núcleo; nenhuma imagem pessoal usada como fixture pública |
| Física Edit/Play/Stop | PASSOU nos testes de núcleo e em 152 checks da UI de propriedades: runtime separado, autoria preservada e Stop restaurando o contexto de edição |
| RifleDemo | PASSOU no Player: coleta, equipar, tiro, dano, recarga, soltar e recoleta com identidade preservada |

Os WIPs de áudio existentes foram revisados e integrados após validação. A
regressão de grade foi substituída por verificações de ausência dos controles;
as asserções de construção, Snap, histórico e persistência permaneceram.
Fit proporcional ampliado não foi implementado e segue como melhoria futura.

## Documentação

- README principal refeito em pt-BR e `README.en.md` criado com os mesmos fatos centrais.
- Logo oficial `midia/logo.png` mantida e referenciada por caminho relativo.
- Hero real de construção em `docs/prints/studio-construcao.png`, inspecionado sem
  dados privados. Captura histórica RifleDemo identificada como anterior.
- `docs/README.md` agrupa manuais de uso, programação e manutenção.
- Guias Build/Player, contribuição, testes, arquitetura, compatibilidade, imagens,
  vídeo, licenças, changelog, roadmap e notas Alpha sincronizados.
- Templates de issues/PR e `SECURITY.md` criados. About e nove tópicos atualizados no GitHub.
- Galeria em `docs/MIDIA.md`: uma captura pública, nenhum vídeo. O gerador stdlib
  usa nomes/extensões/tamanhos, preserva o restante do documento, ignora symlinks,
  ocultos/temporários e arquivos vazios/grandes. Limites editoriais: 8 MiB/imagem,
  20 MiB/vídeo; arquivos maiores devem ser hospedados separadamente.
- 118 referências locais do conjunto público verificadas, sem destinos ausentes;
  links locais do pacote também verificados. Registros em `.turbo/alpha-launch-initial/`.
- Nenhum relatório histórico removido ou reescrito. O rascunho anterior de Release
  recebeu um link para as notas atuais. Nenhuma licença/copyright foi removida.
- Os manuais selecionados no pacote usam links públicos para páginas não incluídas;
  o README do pacote tem comandos próprios da pasta extraída.

## Distribuição

Build oficial: `build-linblux/`, **Release**, CMake **0.1.0**.
Executáveis: `build-linblux/linblux` e `build-linblux/linblux_player`.
A compilação incremental completa passou. `build/` conserva caminho antigo
`/home/labubu/projeto/loblox`; `build-no-video/` é uma configuração alternativa.
Nenhuma dessas árvores foi removida ou declarada segura para exclusão sem inspeção de uso.

Candidato final interno:

```text
linblux-alpha-0.1.0-candidate-2db7969105f6-linux-x86_64.tar.gz
c910eebdc6cdc01163f8d09d18e42ebf070635506dba9ccaf31fae1b2777d45d
```

Gerado por `tools/package_linux.py --build-dir build-linblux --output .turbo/alpha-launch-dist`.
Manifesto: **87 arquivos verificados**, Linux x86_64, origem
`2db7969105f65a6989a735c67ebe9286374c698e`, `source_dirty=true`.
Esse indicador registra os WIPs locais de Scene preservados; o candidato é um
snapshot do worktree e não um binário reproduzível de uma tag limpa.
O arquivo e seu `.sha256` ficam ignorados; nenhum binário entrou no Git.

Extração fora do checkout em `/tmp/linblux-alpha-smoke-plw4rjjx`:
Player validou a cena, executou Lua/renderização por 180 quadros sem erros e
Studio abriu a demo com RmlUi e encerrou por solicitação X11 normal. Captura real
inspecionada. O teste foi supervisionado pelo runner TURBO, timeout 65 s;
logs em `.turbo/logs/alpha-package-smoke-final/tests.log`.

O pacote inclui recursos oficiais selecionados, exemplos, avisos e raylib com
SONAME. Dependências do sistema estão no manifesto. Vídeo de fundo, experiências
privadas, imagens pessoais, caches e logs não entraram no pacote final.
Foi necessário repetir o empacotamento após uma falha de permissão no manifesto
de instalação e após corrigir links para manuais não incluídos; não houve build limpo.

### Bloqueios da publicação binária

1. `avcodec_license()` e `avformat_license()` locais retornam `GPL version 2 or later`.
   Falta concluir obrigações e registrar configuração/fontes correspondentes da build
   vinculada. Referência primária: [FFmpeg](https://www.ffmpeg.org/legal.html).
2. O aviso do header raylib está incluído, mas faltam inventário e avisos completos
   dos componentes embutidos efetivamente redistribuídos.
3. Outra instalação Linux, GPU física e som audível não foram validados. O candidato
   depende das bibliotecas do ambiente e não é anunciado como pacote universal.

MIT do projeto e avisos de ImGui, rlImGui, stb, JSON e RmlUi foram preservados.
Bullet/Lua e demais bibliotecas são dependências do sistema; fontes não foram copiadas.
Isso não constitui liberação jurídica de distribuição. Nenhuma tag ou Release
existia nas consultas desta missão; **nenhuma foi criada**. Notas preparadas em
[ALPHA_0_1_0_NOTAS.md](ALPHA_0_1_0_NOTAS.md).

## Git e proteção dos WIPs

HEAD inicial local/remoto: `ef53733b5766b0ead44146ae9355bccb5051c406`.
Estado inicial: 22 arquivos rastreados modificados e quatro não rastreados.
Registro de status, HEAD e patch binário em `.turbo/alpha-launch-initial/`.
A branch `backup/wip-atlas21` foi confirmada como **local**, sem presumir backup remoto.

| Incremento | Commit |
| --- | --- |
| Explorador, sliders, regressões e versão | `eaf38203174394057e040fff3ed00c2e992b3eb4` |
| README, manuais, galeria e pacote | `1bbe03745e7e18f9b95b334ee00bcfccf288f614` |
| Links dos manuais do pacote | `2db7969105f65a6989a735c67ebe9286374c698e` |
| Relatório e fechamento | Commit que contém a versão final deste arquivo; consulte o comando abaixo |

Os três incrementos acima foram enviados por push normal. HEAD final da entrega
é identificado sem autorreferência de hash pelo commit que atualiza este relatório:

```sh
git log -1 --format=%H -- docs/releases/ALPHA_0_1_0_LANCAMENTO.md
git rev-parse HEAD
git ls-remote origin refs/heads/main
```

A confirmação do push desse fechamento e seu hash exato são informados na entrega
final e guardados em `.turbo/alpha-launch-initial/final-git.json` (local).

Permanecem sem commit os WIPs originais `src/scene/Scene.cpp` e `Scene.h`
(normal de picking) e três relatórios não relacionados. Os três relatórios e a
fixture ALSA foram comparados com a branch de backup e permanecem iguais; a
fixture ALSA agora integra o commit validado. A grade removida continua recuperável
no patch inicial/backup. Não houve reset, clean, force push, sudo ou exclusão de
projetos/experiências. Metadados de build/índice com dono root foram substituídos
apenas onde impediam o trabalho, preservando conteúdo e originais quando necessário.
Um diretório de notas antigo foi preservado em `docs/releases.alpha-preserved/`,
ignorado localmente; os caminhos públicos originais continuam presentes.

## Testes

| Verificação | Resultado |
| --- | --- |
| Build completo incremental `cmake --build build-linblux -j2` | PASSOU |
| `linblux_experiences`, `studio`, `game`, `tests`, `manipulation`, `defaults`, `engine_default_lifecycle` | 7/7 PASSOU |
| `linblux_audio_panel_ui` / `audio_panel_hd_ui` | PASSOU (21,61 s / 37,16 s) |
| `linblux_player_audio_slider_ui` / `player_audio_slider_hd_ui` | PASSOU (23,49 s / 39,35 s) |
| `linblux_native_audio_slider_ui` | PASSOU (16,33 s) |
| `linblux_construction_ui` | PASSOU (55,33 s, 218 checks) |
| `linblux_body_properties_ui` | PASSOU (34,51 s, 152 checks) |
| `linblux_decal_render` | PASSOU (2,42 s, 53 checks) |
| `linblux_explorer_ui` final | PASSOU (31,05 s, HD/FHD) |
| `linblux_rifledemo_player` final | PASSOU (12,58 s) |
| `linblux_player_menu` | PASSOU (41,21 s, HD/FHD) |
| `linblux_image_assets` / `linblux_rifledemo` | PASSOU (0,17 s / 0,52 s) |
| Gerador de mídia stdlib | 4/4 PASSOU; idempotência, escapes, limites, symlinks e preservação do documento |
| Pacote final, hashes, Player e Studio isolados | PASSOU sob TURBO |
| Índice ATLAS ao fim das alterações | Atualizado; nenhuma reescrita de ferramentas ATLAS/TURBO |
| FULL / manipulação gráfica longa repetida / maximizar-F10-F11 abrangente | NÃO EXECUTADO; mudanças não justificaram repetir toda a cobertura histórica |
| GPU física / alto-falantes reais / outra instalação Linux | NÃO EXECUTADO |
| Release binária | BLOQUEADA pelos itens de redistribuição acima |

Falhas registradas: duas tentativas da fixture Explorador (coordenada de expansão
e espera de layout), uma do RifleDemo (recoleta por duração fixa), primeiras
operações de build/ATLAS/instalação por permissões herdadas. Causas investigadas,
sem reduzir as verificações do produto; as repetições focadas finais passaram.

Testes gráficos executados somente pelo runner supervisionado do TURBO, com
orçamento de 420 s para o conjunto focado e limites individuais de até 130 s
(respeitando limites menores do CTest). Repetições foram específicas das falhas.
Logs completos em `.turbo/logs/alpha-launch-focused/`, `alpha-launch-explorer-retest/`,
`alpha-launch-rifle-retest/`, `alpha-launch-final/` e `alpha-package-smoke-final/`.
São validações selecionadas em Xvfb/llvmpipe; não equivalem a aprovação global ou
confirmação manual do usuário. As suítes ATLAS/TURBO não foram repetidas porque
seu código não mudou; benchmarks de armazenamento não foram executados.

## Atualização de apresentação (mesmo dia)

O README principal e o inglês receberam a captura principal de construção e, abaixo
dela, as duas capturas do Explorador com serviços de armazenamento, com largura
controlada e link para a imagem completa. A galeria passou a ter três capturas
revisadas com títulos públicos (`LINBLUX Studio — Construção 3D`, `Explorador e
serviços`, `Serviços de armazenamento organizados`); o gerador continua usando
apenas metadados, preserva o restante do documento e segue idempotente.
A demonstração foi publicada no YouTube
([vídeo](https://www.youtube.com/watch?v=jVck4UuYlWQ), canal Abismroot), com link no
README e na galeria; o GitHub não reproduz MP4 do repositório dentro de uma página.
`docs/videos/linblux.mp4` (~126 MiB) **não entrou no Git**: excede o limite de 20 MiB
da galeria e o limite de 100 MiB por arquivo do GitHub. Nada foi recodificado ou
duplicado e nenhuma URL foi inventada. O estado do binário não mudou: a Release
binária continua bloqueada pelos itens de redistribuição acima.

## Próximas versões

Concluir obrigações de redistribuição e ampliar validação Linux. Depois consolidar
Studio/renderização, personagens, importação 3D e API Lua conforme o roadmap.
Multiplayer, contas e publicação online permanecem objetivos futuros sem datas.
