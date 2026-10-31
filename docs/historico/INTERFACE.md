# Histórico — Interface e apresentação do gameplay

Registros de 25–27/10/2026, reunidos por assunto. São evidências das revisões
originais; não representam validação da versão atual. Consulte o
[índice atual](../README.md) para os manuais e comandos de uso.

<a id="registro-30"></a>

## Registro 30

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/30-hud-circular.md).

### HUD circular — 26/10/2026

IMPLEMENTADO/TESTADO: um renderizador compartilhado no Play do Studio e Player,
saúde real no centro e arco vermelho proporcional, interpolação exponencial,
zero imediato na morte e máximo correto no respawn. Não cria textura por frame.
A barra RmlUi antiga fica oculta e a barra nativa foi retirada. O indicador
sobre a cabeça, quando configurado, usa vermelho. `linblux.ui.health_hud(false)`
oculta o HUD padrão; `true` restaura. Novo Play restaura a preferência padrão.

ATLAS localizou Runtime, ExperienceOverlay, GamePanels e StudioFrontend.
Build incremental dos hosts/testes: 137,82 s + 28,25 s. TURBO FAST
20261026T223856Z-8205: 16/16, 9,4 s, cobertura global desconhecida (sem rebuild
adicional; os targets afetados foram compilados antes).
Supervisão TURBO: health_hud + lua_ui_render + lua_ui_player, 3/3 em 28,948 s.
Health_hud verifica dano/cura/morte/respawn, interpolação, ocultação Lua,
pausa/reinício e pixels em 1280×720,1600×900,1920×1080; captura 720p inspecionada.
Logs/capturas em .turbo/logs e build-linblux/.turbo/logs, ignorados.

Limites: a regressão integral ainda precisa atualizar os seletores de testes
históricos que exigiam a antiga barra RML. Não executar esses testes como se a
expectativa visual antiga ainda fosse requisito do produto. Release ainda
bloqueada por menu/Demo/pacote e validação final. Trabalho de combate
preservado fora do commit deste HUD, snapshot em .turbo/recovery-final e
AutoBackup remoto eed462f8f1d3 confirmado no início.

IMPACT 20261026T223928Z-8644: 22/22, 56,0 s, cobertura global parcial.

<a id="registro-31"></a>

## Registro 31

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/31-menu-lateral.md).

### Menu lateral — 26/10/2026

IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER: Studio RmlUi e Player independente
usam painel à esquerda, entrada/saída de 180 ms, marca oficial e navegação
vertical. Continuar, jogador/respawn, câmera, controles, áudio, vídeo, ajuda e
saída preservam as ações reais. Pausa bloqueia movimento e captura; primeira
pessoa/trava recuperam o mouse ao continuar. Volume controla SetMasterVolume;
sem dispositivo, a UI informa indisponibilidade em vez de simular reprodução.
Player usa tela cheia sem bordas e restaura o tamanho anterior.

ATLAS consultado para Runtime/Audio/StudioFrontend. Build dos hosts aprovado
(53,55 + 5,02 + 57,55 s; últimas recompilações focadas 17,54/3,47/11,98 s).
`linblux_player_menu_ui`: aprovado em43,36 s, 133 verificações; testa layout
720p/1080p, captura, movimento bloqueado, câmera/sensibilidade/Shift, páginas,
respawn, fechar e parar. `linblux_player_menu`: aprovado em16,42 s no executável
real, Esc, mouse, áudio/vídeo, tela cheia/restauração e saída, duas resoluções.
Capturas do Studio720p e Player1080p inspecionadas.

Falhas corrigidas: texto anônimo em botões flex RmlUi não aparecia (usar span),
barra superior sobrepunha a marca (ocultar durante menu), fullscreen exclusivo
não expandia no Xvfb (usar sem bordas). Pulsos X11 curtos eram perdidos durante
renderizações de software; o harness mantém tecla/botão por250 ms. A primeira
rodada RmlUi excedeu75 s; o teste agora concentra funções em720p e confere
layout em1080p. Não foram repetidos testes aprovados sem alteração relevante.

Limites: não foi ouvido áudio neste ambiente sem dispositivo. O fallback
ImGui histórico de depuração ainda usa seu menu anterior; o Studio Alpha usa
RmlUi. Seletores históricos de regressão que exigiam o menu central e a barra
RML ainda precisam migrar antes do fechamento de regressão/Release.

FAST final20261026T225715Z-20329:17/17 em8,8 s, cobertura global desconhecida.
A rodada anterior encontrou o defeito real do RifleDemo em andamento (resolvido
no exemplo; não incluído no commit deste menu). Núcleo recompilado: studio,
core e UI Lua3/3 aprovados; não há aprovação global/FULL implícita.

IMPACT20261026T225754Z-20749:23/23 em59,0s, cobertura global parcial.

<a id="registro-35"></a>

## Registro 35

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/35-precisao-encaixe.md).

### 35 — Encaixe geométrico do Studio — 27/10/2026

Estado: IMPLEMENTADO / TESTADO; publicação registrada no histórico Git.

Ctrl durante Mover aproxima caixas por faces, respeitando eixo/plano do gizmo.
Dimensões, rotação e geometria real dos filhos do Modelo substituem heurísticas
de distância entre centros. SAT verifica contato e recusa correções que deixem
outras caixas penetrando obstáculos. Shift mantém movimento sem grade.
Marcador de contato, dica no viewport, histórico e cancelamento existentes.
Guia: `docs/EDITOR_PRECISAO.md`.

Arquivos: Gizmo/GizmoMath, EditorUI, testes studio/gizmo_visual, contrato editor.
Sem alteração dos arquivos preexistentes de áudio/CMake/UI. Snapshot inicial
ignorado: `.turbo/recovery-20261027/preexisting.diff`; AutoBackup remoto inicial
`5d38a76d8306`, 433 arquivos, zero descobertos naquele instante.

Validação:
- Build Studio/Player/testes via TURBO: 92,65s; build após testes: 46,02s.
- `linblux_studio`: passou em 6,526s (runner); testes numéricos incluem caixas
  0,2–2000, rotações 0/15/30/45/90°, contato, restrições, obstáculos, pivô
  deslocado/filho rotacionado, undo/redo e cancelamento.
- `linblux_gizmo_visual`: passou em 23,002s; capturas 1280×720/1920×1080
  inspecionadas em `/tmp/linblux-surface-snap-PBPIoz/`.
- FAST `20261027T205255Z-29378`: 17/17, testes 10,4s; cobertura desconhecida.
- IMPACT `20261027T205414Z-30819`: 23/23, 68,2s; cobertura parcial, sem gráficos
  automáticos. Binários novos compilados pelo FAST; demais testes não foram
  recompilados por esse IMPACT sem build.
- `linblux_models_ui` passou em 52,087s, mas seu executável não foi recompilado
  neste incremento: evidência auxiliar do baseline, não aprovação da integração
  nova. O gizmo visual e o teste studio foram recompilados.
- Falhas iniciais: setter/tipo incorretos corrigidos no teste; fixture de
  obstáculo corrigida; poses restauradas comparadas com tolerância, não por
  igualdade textual de floats normalizados.

Limites: sem encaixe de esfera/malha, giro automático por normal, modo dedicado
de vértice/aresta ou transformação conjunta de multisseleção sem Modelo.
Sem bloqueio global de sobreposição nem conclusão integral da missão.
Próximo: contraste/contornos no editor e seleção de peças sobrepostas.

<a id="registro-36"></a>

## Registro 36

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/36-contornos-selecao-nomes.md).

### 36 — Construção legível, seleção e nomes — 27/10/2026

Estado: IMPLEMENTADO / TESTADO. Encaixe anterior PUBLICADO em
`0142aa98daf5c9edf818c9b34298088be7a9311a` (remoto confirmado).

- Contornos opcionais com profundidade no Editor; hover discreto, AABB e normais
  de caixas selecionadas. F8 / Shift+F8 / Ctrl+F8; Player não chama este passe.
- Alt+clique percorre Parts sob o raio; Shift+clique escolhe o Modelo ancestral
  mais próximo. Geometria com alfa zero não captura cliques. Gizmo tem prioridade.
- Sobreposição volumétrica/coincidência entre caixas selecionadas e outras
  caixas gera diagnóstico no viewport. Não muda nem apaga a geometria.
- Novas peças da UI: Peça/Esfera. Duplicatas usam `_002`, `_003`, preservando
  nomes antigos, espaços, IDs e limites UTF-8. APIs/identificadores legados mantidos.
- Marcador de encaixe desenhado após o gizmo, com fundo para leitura.

Fontes: Scene, SceneRenderer, EditorUI, Gizmo; testes studio/gizmo_visual/rml_ui.
Guia: EDITOR_PRECISAO.md. Capturas próprias com câmera/material iguais em
`docs/screenshots/editor-precision/{antes,depois}.png`.

Evidências:
- Build Studio/Player/gizmo/studio/rml_ui: passou, 56,54s após correção de include.
- FAST `20261027T210048Z-4136`: 19/19, testes 9,7s, build 180,7s; cobertura
  desconhecida. Cabeçalho Scene afeta muitos consumidores. WIP de áudio presente.
- Numérico+gizmo gráfico: 2/2 em15,766s; comparação de pixels 720p/1080p,
  materiais preservados. Capturas antes/depois inspecionadas.
- Models UI recompilado: passou em39,268s; inclui agrupar, arrastar, undo/redo,
  propriedades, salvar e Play/Stop. `.turbo/logs/editor-models-ui-current.log`.
- Fixture curta nova `LINBLUX_EDITOR_PRECISION_ONLY=1`, X11/RmlUi real:
  63 verificações/0 falhas, 23,558s. Seleção por Alt, Modelo por Shift, F8,
  arraste com Ctrl, contato sem penetração e undo. Screenshot inspecionada.
  Supervisão TURBO, timeout55s, log `.turbo/logs/editor-precision-x11.log`.
- Build após legibilidade do marcador:5,17s. Revalidação final: gizmo gráfico
  aprovado em22,505s e X11 63/0 em26,246s; captura final inspecionada. Logs
  `editor-contact-final.log` e `editor-precision-x11-final.log`.

Não há alegação de ganho de FPS, sombras de contato, PBR ou correção universal
de coplanaridade. Diagnóstico refere-se a caixas com interseção volumétrica;
malhas arbitrárias e superfícies meramente tangentes não são classificadas.
WIP preexistente de áudio/frontend/CMake preservado fora dos commits.
Próxima correção investigável: giro de NPC calculado por Euler Y (asin), que
pode limitar/oscilar o heading depois de90°. Reproduzir antes de corrigir.

<a id="registro-37"></a>

## Registro 37

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/37-npc-giro-atrito.md).

### 37 — NPC: giro completo e atrito do controlador — 27/10/2026

Implementação real em `Npc.cpp` e `RuntimeSession::partMotion`; sem mudar cenas,
propriedades autorais ou formato. Testes em `NpcHeadingRegression` (studio).

Reprodução antes da correção:11 falhas. O heading135° alinhava0,7275 e180°
alinhava0,0292, pois Euler Y (asin) perdia o quadrante e oscilava perto de90°.
Agora o heading vem de atan2 do vetor frontal, respeitando o limite turnSpeed.
Reprodução após corrigir somente giro:8 falhas, todas de deslocamento. Em todas
as direções, velocidade2 percorria1,9059 unidade em2s no piso de atrito0,8.

O NPC usa corpo Bullet dinâmico conduzido por velocidade, como o jogador,
não um bloco solto: atrito de contato zero enquanto recebe velocidade horizontal
do controlador, restaurado ao parar. Densidade, massa, damping e propriedades
da Part continuam intactos. `halt` também restaura quando a velocidade já é zero.
Parts comuns não recebem esta política. Não é navegação/movimento universal.

Testes verificam8 direções, giro por passo <= turnSpeed×dt, deslocamento3,8–4,1
unidades em2s com speed2, parada, rigidez vertical, material preservado e
invariantes do runtime. Rodam em Bullet real, não só matemática de quaternions.

Evidências:
- Antes: `.turbo/logs/npc-heading-before.log`,11 falhas; heading corrigido:
  `npc-heading-only.log`,8 falhas com medição do arrasto.
- Depois: studio+Player RifleDemo2/2 em19,792s. Player real inclui patrulha/clip,
  coleta/equipar/disparar/recarga/drop/recoleta. `npc-movement-final.log`.
- Build Studio/Player/studio_tests:35,37s. FAST `20261027T211050Z-12170`:
  17/17, testes12,1s, build25,1s; cobertura desconhecida.
- IMPACT `20261027T211213Z-13562`:23/23 em64,7s, sem timeout; cobertura
  parcial, gráficos validados separadamente no Player real. Sem FULL.

As mudanças locais de música/áudio preexistentes não integram o commit: somente
o hunk de partMotion é indexado. Sem claims de colisão arbitrária perfeita,
massas em SI, física de multidões ou fechamento integral da missão.

<a id="registro-38"></a>

## Registro 38

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/38-pulo-queda.md).

### 38 — Apresentação de pulo e queda — 27/10/2026

O estado físico/API legado Freefall continua significando descida no ar.
O clipe de queda agora exige velocidade vertical<-8, tempo no ar>0,3s,
perda de altura desde o ápice>4,5 unidades e ausência de chão próximo
(raio Bullet de meia altura da cápsula+max(3,meia altura)).

Pulos próximos do chão mantêm o clipe de pulo durante a descida; uma queda
significativa fica travada até aterrissar, evitando alternância de clipes.
Entrada de clipe e blend de0,12s existentes são reutilizados. Respawn e Stop
limpam o rastreador. Sem novos estados/clipes serializados ou mudança na API Lua.
Não é uma nova máquina completa JumpStart/JumpAir/Land com clipes próprios.

Fontes: Runtime.h/cpp; testes studio/IndependentFall/SignificantFallPresentation.
Testes a30/60/144FPS: pulos2/7/20, descida física, pouso, degrau2, queda30,
amostra do clipe de queda e respawn durante queda. Primeiro teste apontou3
falhas reais de flag após respawn: corrigido reset no local adequado.

Build Studio/Player/game/studio_tests128,13s; correção final20,39s.
Regressão studio+game+Player RifleDemo3/3 em21,920s, log
`.turbo/logs/significant-fall-final.log`. Sem captura específica de salto;
validação de pose é numérica, Player gráfico verifica integração existente.
FAST `20261027T211806Z-18220`:17/17, testes8,1s, build83,1s; cobertura
desconhecida. IMPACT será executado junto da recuperação do backend de música.
WIP de áudio preservado separado deste commit.

