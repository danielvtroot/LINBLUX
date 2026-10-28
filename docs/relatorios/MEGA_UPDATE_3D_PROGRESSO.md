# Mega Update 3D — progresso das fases 0 a 4

Estado atual: implementações 0–4 presentes; visuais 2/4 reprovados pelo usuário.
A revisão obrigatória está registrada ao final; não há aprovação visual do usuário.
A execução da fase 4 e seus limites estão ao final deste relatório.

Registro da primeira execução, encerrada em 28/10/2026 (America/Belem). Escopo: baseline,
proteção e guias de geometria. **FASE 0: DONE. FASE 1: DONE no escopo visual
validado abaixo.** Aprovação global/Release não concedida: há falha anterior de
restart registrada. Fases 2–7 não implementadas.

## Proteção e baseline (fase 0)

- HEAD inicial: `2aebae4`, branch `main`, acompanhando `origin/main`.
- WIP anterior preservado integralmente e excluído do commit desta sessão:
  `assets/ui/documents/studio.rml`, `assets/ui/styles/studio.rcss` e os documentos
  não rastreados `AUDITORIA_COMPLETA_MOTOR_3D_LINBLUX.md` e
  `LINBLUX_PLANO_MEGA_UPDATE_3D.md`. Nenhuma experiência pessoal foi usada.
- ATLAS atualizado antes/depois; consultados `locate drawEditorGuides`,
  `locate sphereRadius bounds NavGrid`, `tests-for` e `impact` do renderer,
  cabeçalho e navegação. Relações textuais não foram tratadas como semânticas.
- Versões: `CMakeLists.txt` mantém 0.3.0 desde a fundação `077340a`; a proposta
  de distribuição Alpha 0.1.0 foi acrescentada em `f762dd5`. São históricos
  distintos sem reconciliação de versão no código. Nenhuma versão/tag/Release
  alterada; decisão de produto continua pendente.
- `tests/mega3d_capture.py` gera isoladamente `grey-room` (paredes longas,
  piso, decal e esfera com eixos diferentes), `material-gallery` (seis materiais,
  esferas e transparência) e `editor-snap` (0/15/30/45/90°, Model/pivô distante).
  Decal de calibração original gerado no output; nenhum asset externo/binário
  acrescentado ao Git. Hashes de cada JSON são iguais entre antes/depois.
- Câmera do executável no reset: alvo `(0,2,0)`, distância 42, yaw 0.75,
  pitch 0.48, FOV 45°. Sol fixo `(-.45,-1,.55)`, céu animado desativado,
  intensidade/brilho 1. Mesmo WIP de interface nos dois estados.
- Baseline histórico verificado com o executável anterior à mudança:
  `linblux_tests` **PASSOU** (0.56s); `linblux_engine_default_restart`
  **FALHOU** (48.98s), timeout interno esperando evento `runtime/start` na
  etapa `restarted`, não timeout global TURBO. Sem reexecuções às cegas.
  Log: `.turbo/logs/mega3d-phase01/baseline-restart/tests/linblux_engine_default_restart/tests.log`;
  artefatos: `/tmp/linblux-studio-default-restart-yl7vzaf5`.

## Implementação (fase 1)

- `SceneRenderer.h`: `EditorGeometryView.edges=false`; bounds/normais continuam
  opt-in. Player segue sem chamar guias de autoria ou colisores.
- `EditorUI.cpp`, `EditorActions.cpp`, `StudioFrontend.cpp`, `ribbon.rml`:
  F8 preservado, status explícito e botão **Visualizar → Arestas** com estado
  ativo, `aria-pressed` e tooltip. Interface legacy também expõe o controle.
  Seleção e hover continuam condicionados à interação.
- `SceneRenderer.cpp`: bias somente no passe de guias (`projection.m14 -=
  0.0001`), flush antes/depois, restauração da projeção e depth test mantido.
  `GL_POLYGON_OFFSET_FILL` não resolveria `GL_LINES`. Linhas ocultas por paredes
  continuam ocultas; posições e tamanhos da geometria não são inflados.
- Captura F8 revelou também cordas da esfera wire genérica dentro da superfície
  facetada. Guias agora compartilham os mesmos pontos/7 faixas/12 setores da
  esfera existente. Iluminação e geometria sólida permanecem iguais.
- AABB de depuração da esfera e limites do `NavGrid` usam o raio real
  `min(size)/2`. Tamanho serializado, física e picking foram preservados.
- Contratos manuais de `rendering` e `game` atualizados.

## Evidência visual e build

Capturas depois do **executável real** `build-linblux/linblux`, supervisionadas pelo
runner TURBO, com HOME/XDG isolados. Todas as três fixtures capturadas nas duas
resoluções. Diretório reconstruível, ignorado no Git:
`.turbo/logs/mega3d-phase01/{before,after}/`.

As primeiras capturas foram feitas no executável real anterior. A inspeção
completa encontrou `modelPivot.scale` inválido na fixture editor-snap (o Studio
preservou corretamente a cena padrão). O gerador foi corrigido e agora valida
**cada fixture no Player antes de capturar**, recusando fallback. Também passou
a expor o decal na face voltada para a câmera. Para comparar todas as fixtures
corrigidas contra o mesmo código anterior, renderer/header e EditorUI originais
de `2aebae4` foram recompilados em arquivos/archives isolados em
`.turbo/logs/mega3d-phase01/baseline-build/`, sem reverter fonte nem substituir o
executável final. As capturas `before` definitivas usam esse baseline; `after`
usa `build-linblux/linblux`. Bibliotecas engine/UI e WIP são compartilhados;
não se trata de baseline histórico de desempenho da engine inteira.
As três cópias temporárias de fonte foram removidas após o build para não
duplicar símbolos no ATLAS; hashes em `baseline-build/source-hashes.json`.
Variações de ícone/localização da UI no binário isolado não entram na comparação
3D. O código de renderer e a câmera/luz são os objetos da comparação visual.

| Fixture | 1366×768 antes / depois | 1920×1080 antes / depois |
|---|---|---|
| grey-room | `before/grey-room-1366.png` / `after/grey-room-1366.png` | `before/grey-room-1920.png` / `after/grey-room-1920.png` |
| material-gallery | `before/material-gallery-1366.png` / `after/material-gallery-1366.png` | `before/material-gallery-1920.png` / `after/material-gallery-1920.png` |
| editor-snap | `before/editor-snap-1366.png` / `after/editor-snap-1366.png` | `before/editor-snap-1920.png` / `after/editor-snap-1920.png` |

F8 ON: `after/f8-on-1366.png` e `after/f8-on-1920.png`. OFF por padrão:
superfícies limpas, sem contorno escuro/tracejado. F8 ON: arestas contínuas e
esfera com guias aderentes. A galeria ainda tem transparência coplanar no piso;
esse comportamento anterior pertence à fase 4, sem alteração nesta execução.

Build incremental passou com `cmake --build build-linblux -j2 --target linblux
linblux_player linblux_tests linblux_gizmo_visual_tests linblux_rml_ui_tests
linblux_decal_render_tests`; FAST também recompilou os consumidores selecionados
(96.9s). Avisos preexistentes não foram ampliados para outro escopo.

## Validação focada e limites

TURBO `test --mode fast --changed --no-cache --budget 180`: **PASSED, 19/19**, sem
cache, 10.6s de testes, 110.5s totais; run `20261028T030527Z-27390`.
Integrações gráficas pelo `run_supervised_tests` do TURBO: **4/4 PASSED**, 63.6s,
orçamento global 180s e limite individual 65s; logs completos em
`.turbo/logs/mega3d-phase01/final-focused/tests.log`:

| Teste | Resultado / evidência |
|---|---|
| `linblux_gizmo_visual` | 300 casos, 8.981 verificações, zero falhas; aresta de caixa **94/94** e esfera **94/94**; hover oculto zero pixels; AABB projetada confere |
| `linblux_editor_precision_ui` | 82 verificações, zero falhas; picking, Alt+clique, seleção de Model, F8 e botão Arestas via X11 real |
| `linblux_decal_render` | 9 verificações, zero falhas; imagem opaca, transparência, Decal/Texture e inventário; capturas `/tmp/loblox-decal-render-4ty3Oc` |
| `linblux_player_menu` | Player real aprovado; runtime, menu, teclado, fullscreen e encerramento; capturas 720p/1080p em `/tmp/linblux-player-menu-7kduee6r` |

`linblux_tests` inclui nova compatibilidade de esfera 40×4×60 rotacionada,
round-trip JSON preservando size, picking e grade 5×5 pelo raio, sem mutação
do mundo. FAST inclui experiências/.lo e ciclo de defaults; não é promessa de
cobertura exaustiva de todos os projetos do usuário.

Uma primeira versão da regressão visual acusou o seletor de pixels roxos, que
também aceitava o céu azul. O seletor foi corrigido para isolar o AABB; resultado
final: `(199,79)–(440,320)` contra projeção `(199.3,79.3)–(440.7,320.7)`.
Esse diagnóstico e a tesselação divergente da esfera justificaram o segundo
ciclo focado. Não houve terceira tentativa do mesmo teste.

IMPACT foi apenas planejado: inclui restart já vermelho e integrações de Lua/rig
fora da correção. Não foi executada bateria adicional nem FULL, pois build,
regressão específica, Editor/seleção/decal/Player e capturas já deram evidência
suficiente para esta fase. **Sem aprovação global de regressão ou de Release.**

## Medição e reprodução

Mesa llvmpipe (LLVM 19.1.7, 256 bits), Xvfb, `LIBGL_ALWAYS_SOFTWARE=1`,
`LP_NUM_THREADS=2`. `before/measurements.json` e `after/measurements.json`
registram resolução, RSS, hash do projeto e amostras da instrumentação existente
(`LOBLOX_UI_TIMINGS`). Os draw calls são da **UI RmlUi**, não da cena 3D.
Algumas janelas de 1366 não completaram 30 frames e estão como `unavailable`.
Resizes/F8 podem compartilhar a janela de amostragem: dados observacionais,
**não benchmark estável de GPU ou comprovação de ganho de desempenho**.
RSS final observado: antes 220.6–287.7 MiB; depois 226.0–291.1 MiB (três
fixtures, duas resoluções). Capturas definitivas: 25.9s antes + 32.0s depois.

```sh
# Gerar fixtures apenas, sem executar engine:
python3 tests/mega3d_capture.py --output .turbo/logs/mega3d-phase01
# Para capturar, executar xvfb-run + tests/mega3d_capture.py --binary
# build-linblux/linblux --stage before|after pelo run_command do TURBO,
# com timeout 100s e tela -screen 0 1920x1080x24 -noreset.
```

Tempo de execução da sessão: aproximadamente 25 minutos, incluindo orientação,
compilação, diagnóstico das fixtures, capturas e validação. Sem medir GPU real.

## Publicação e retomada

Commit atômico: `fix(rendering): make editor guides opt-in and depth-correct`.
Hash da primeira entrega: `044fc7b9e7ace88741e394c040f932b84fa154ec`.
Push normal para `origin/main`;
comparação `git rev-parse HEAD` / `git ls-remote origin refs/heads/main` registrada
no encerramento em `.turbo/logs/mega3d-phase01/publication.json`.
WIP e documentos anteriores continuam locais. Nenhum cache/build/imagem no Git.

Próxima execução: **somente FASE 2**, mediante pedido separado. Começar com
`python3 tools/atlas/atlas.py update` e `python3 tools/atlas/atlas.py locate
'SnapSurfaces GizmoMath'`, lendo a fase 2 do plano. Nenhuma continuação automática.


## Continuação autorizada — fases 1, 2 e 3 (28/10/2026)

O pedido posterior "fazer a 1, 2, 3" amplia o escopo anterior. Fase 1 mantida
em `044fc7b`, sem reimplementação. WIP de studio.rml/studio.rcss e documentos
anteriores continua fora dos commits. Não se implementam fases 4–7.

### FASE 2 — DONE

- Grade absoluta e incremento relativo explícitos em Modelo → Construção;
  incremento é o padrão compatível. Shift continua liberando a grade.
- Face/borda/vértice por raycast OBB sob o cursor, com desempate estável e
  alvo anterior em empates de profundidade. Ctrl/Encaixe é colocação livre 3D;
  fora desse modo o eixo/plano permanece restrito.
- Alinhar Y à normal opt-in, prévia translúcida, veto SAT com peça/motivo/ação
  no status. Bloqueio mantém a última pose aceita. Sem caixa fictícia de esfera:
  seleção/alvo esférico exibe indisponibilidade. Histórico/cancelamento composto
  de Model inclui rotação durante Move e respeita pivôs distantes.
- Seis setas mantidas, eixos identificados, readout numérico; layout e picking
  continuam compartilhados com tamanho em pixels/fallback existentes.
- `CursorConstruction`: 0/15/30/45/90°, .2/4/192 unidades, concorrência, bloqueio,
  face/borda/vértice, pivô distante, 100 reposicionamentos; recinto com quatro
  paredes e piso montado por contato sem preencher XYZ, sem penetração, JSON
  reaberto. Model/undo/cancel/zoom também cobertos pelas regressões existentes.
- Build real `build-linblux/linblux` aprovado. TURBO FAST **11/11 PASSED**, sem
  cache, run `20261028T032533Z-8671`, 78.4s (4.9s de testes).
- TURBO focado **4/4 PASSED**, 66s/180s: studio (3.891 checks), gizmo_visual
  (300 casos/8.981 checks), editor_precision_ui (93 checks), models_ui (168
  checks). Logs: `.turbo/logs/mega3d-phase2/focused/tests.log`.
- Capturas reais antes/depois das três fixtures em 1366×768 e 1920×1080:
  `.turbo/logs/mega3d-phase2/{before,after}/`; controles e contato reais:
  `/tmp/loblox-rml-ui-kbRWVv/{construction-controls,precision-contact}.png`.
- `docs/EDITOR_PRECISAO.md` e contrato editor atualizados; ATLAS atualizado.
  O corte anterior do botão Imagens no ribbon estreito continua sendo fase 7;
  os controles de construção aparecem e são acessíveis em 1366×768.
- Commit: `79703f2` — `feat(editor): add cursor surface placement and absolute grid`.
  Hash/push/HEAD remoto registrados no encerramento da sessão. A falha histórica
  de engine_default_restart continua registrada; não é aprovação global.


### FASE 3 — implementação e aprovação técnica focada

- `ModernMaterials`/`RenderSettings`: shaders GLSL 330, iluminação por pixel,
  normal por inversa transposta, especular e esfera suave, compartilhados entre
  Studio e Player. Compatível preserva o renderer anterior; falha de shader
  retorna ao caminho CPU. Baixo simplifica a luz, Médio é o padrão e Alto usa
  GGX/Schlick/ambiente aproximado: **PBR leve, sem IBL/BRDF LUT/PBR completo**.
- Material3D opcional em Part: baseColor, metallic, roughness, emissive e força,
  normalScale, textura de cor/normal/metallic-roughness/occlusion e UV com
  unidades por repetição, deslocamento e rotação. Presets Plástico, Concreto,
  Madeira, Metal, Vidro simples e Emissivo. Cor do albedo é decodificada de sRGB;
  mapas de dados ficam lineares; tonemap/saída sRGB uma vez. Céu e decals mantêm
  seus próprios passes sem dupla conversão. Texturas premultiplicadas são
  recuperadas antes da iluminação, com composição alfa testada.
- Material não substitui decal: o teste de Decal/Texture continua aprovado.
  F8 permanece opcional: guias modernas esparsas seguem subsegmentos das
  arestas/UVs nativas da própria malha, incluindo rotação, com depth test/bias.
- Propriedades expõem presets/parâmetros/quatro mapas; Visualizar oferece
  Qualidade 3D. Player aceita `--quality legacy|low|medium|high`; variável
  `LINBLUX_RENDER_QUALITY` tem os mesmos valores. Perfil por sessão, sem
  acrescentar configuração global obrigatória ao projeto.
- JSON **v7 somente quando houver Material3D**; cenas sem ele continuam usando a
  versão mínima v1–v6. Dados antigos e PartMaterial permanecem para Compatível.
  Validação transacional, duplicação e referências de imagens foram integradas.
  Exportação/importação `.lo` reloca os quatro mapas e foi testada em outro HOME.
  Nenhuma experiência do usuário foi convertida nem regravada.
- Build real Studio/Player aprovado; logs de compilação incremental em
  `.turbo/logs/mega3d-phase3/build*.log`. Não foram trocados raylib/Bullet,
  adicionadas dependências, tags, Releases ou alterada a versão CMake 0.3.0.
- Regressões focadas aprovadas: `linblux_tests` (inclui presets, JSON v7 e
  transação, remoção/renomeio de mapa e retorno ao formato antigo),
  `linblux_experiences` (181 checks, inclui quatro mapas no .lo),
  `linblux_decal_render` (9 checks), `linblux_gizmo_visual` (300 casos/8.981
  checks no contrato Compatível), `linblux_editor_precision_ui` (115 checks,
  edição real de rugosidade/preset/qualidade) e `linblux_player_menu` real.
- `linblux_material_render`: **31 checks**, shader ativo, esfera com **88 cores
  distintas em 160 amostras**, quatro mapas, alfa (meia opacidade confere com
  a composição), pixels idênticos Studio/Player sem overlays com câmera idêntica,
  retorno exato a Legacy e fallback forçado. Cobre ainda esfera girada com guias
  e restauração do estado. Capturas dos quatro perfis nas duas resoluções em
  `.turbo/logs/mega3d-phase3/materials/`. Log final:
  `.turbo/logs/mega3d-phase3/native-uv-final/tests.log`.
- Logs complementares em `material-focused/`, `material-final/`, `guides-final/`
  e `ui-focused/` sob `.turbo/logs/mega3d-phase3/`. A primeira amostragem de
  gradiente estava no hemisfério sem luz direta; a calibração foi corrigida para
  luz frontal fixa, com todos os demais checks mantidos. A revisão F8 motivou
  guias esparsas por UV nativo em vez de mostrar todos os triângulos.
- Capturas reais das três fixtures em 1366×768 e 1920×1080, câmera/luz/JSON iguais:
  `.turbo/logs/mega3d-phase3/after/`. O `before/` reutiliza os arquivos de `phase2/after/`,
  mantendo os mesmos hashes das cenas; não há reconstrução de baseline nesta fase.
  O capturador agora aguarda o primeiro frame, pois duas imagens iniciais ficaram
  pretas ao competir com compilação. Capturas definitivas foram inspecionadas.
  UI de material: `/tmp/loblox-rml-ui-EVTM8T/material3d-properties.png`;
  Player: `/tmp/linblux-player-menu-5e42ie08/`.
- FAST final executado: `20261028T034840Z-25052`, **19/19 executados sem falhas**,
  build 28.6s + testes 8.1s, total 37.7s. O estado do TURBO é
  **PASSED_PARTIAL / coverage unknown**, porque CMake e cabeçalhos públicos
  mudaram. Isso **não é aprovação plena/global**. Os novos módulos foram
  cobertos explicitamente pelas regressões focadas acima; o último ajuste
  privado das guias foi compilado e validado no teste gráfico e capturas finais.
  Não se executou FULL decorativo nem se repetiu o restart já vermelho antes
  desta implementação. A falha histórica de restart continua pendente.
- Medição: apenas llvmpipe/Xvfb, RSS/tempos observacionais em `measurements.json`;
  alguns intervalos são `unavailable`. Sem benchmark de GPU integrada, sem
  promessa de desempenho em hardware real. Sombras, SSAO, IBL, ordenação de
  transparência e otimizações de mapas grandes ficam para fases 4/5. Rigs e
  marcador decorativo de SpawnLocation ainda usam o passe legado compartilhado.
- Estado: **DONE no escopo funcional da fase 3; regressão global PARTIAL**.
  Fases 1, 2 e 3 atendidas nesta sequência; nenhuma continuação para fase 4.
  Commit desta fase: `feat(rendering): add GL33 materials and per-pixel lighting`;
  hash/push/HEAD remoto em `.turbo/logs/mega3d-phase3/publication.json` e na entrega.

Tempo desta continuação: aproximadamente 37 minutos, incluindo duas fases, builds e revisão visual.
Próxima execução: somente fase 4, mediante pedido separado; começar por
`python3 tools/atlas/atlas.py update` e `python3 tools/atlas/atlas.py locate "Lighting shadow map"`.

## Continuação — fase 4: profundidade, transparência e AA (28/10/2026)

**FASE 4: DONE no escopo funcional/visual descrito abaixo.** Somente fase 4
nesta execução, sobre `926f1a5` em `main`; fases 5–7 não iniciadas. Aprovação
local de implementação, build, regressões e capturas; não é aprovação global,
Release, medição de GPU integrada ou confirmação manual do usuário.
Os quatro WIPs originais continuam preservados e fora do commit; hashes dos
arquivos de UI coincidem com `.turbo/logs/mega3d-phase01/wip-preserved.json`.
Não houve alteração em raylib/Bullet, experiências pessoais ou versão do projeto.

### Implementação

- Shadow map direcional **1024²**, PCF **3×3**, frustum ajustado aos casters
  próximos e estabilizado em texels, alcance/fade, normal bias e depth bias
  proporcionais ao texel, profundidade do frustum e inclinação da superfície.
  Parts e corpos de rigs projetam sombras reais em Parts/solo; vidro não
  projeta sombra opaca. Contato vem desse passe; nenhum SSAO/quad de escurecimento.
- Médio/Alto têm sombras; Baixo/Compatível não alocam nem executam o passe.
  Lighting expõe habilitação, alcance e intensidade. Persistência aditiva e
  opcional, sem mudar a versão: padrão omitido, campos inválidos rejeitados
  antes de modificar a cena. Projetos anteriores continuam carregando.
- Passes separados: opacos/rigs, decals opacos, Parts translúcidas ordenadas
  por distância da câmera com seus decals, seleção. Alfa/decals sem gravação
  de depth; superfície frontal evita aplicar a transparência duas vezes.
  F8 continua opcional e mantém a oclusão pela profundidade da cena.
- Antialiasing compartilhado entre Studio e Player, só na cena: Auto usa
  nenhum no Baixo/Compatível, FXAA no Médio e **MSAA 4× real** no Alto, com
  fallback MSAA → FXAA → nenhum. Controles em Visualizar e variável
  `LINBLUX_ANTIALIASING=auto|off|fxaa|msaa4`. Removido o FXAA separado do
  Editor; RmlUi mantém seu passe próprio e não recebe AA duplicado.
  Cor/gama de áreas planas permanece igual após o resolve.
- FBOs de leitura/escrita, viewport, matrizes e estados de profundidade,
  culling, scissor, sRGB e multisample restaurados em passes aninhados.
  O desligamento libera recursos sem perder o FBO do chamador.

### Validação focada e evidência

- Build incremental real **PASSOU**: `cmake --build build-linblux -j2`, inclusive
  `build-linblux/linblux` e Player. Log final `build-accepted.log`.
- TURBO supervisionou sete regressões específicas (orçamento 200s, timeout
  gráfico individual 60s): `linblux_tests`, `linblux_decal_render`,
  `linblux_depth_quality`, `linblux_editor_precision_ui`, `linblux_gizmo_visual`,
  `linblux_material_render`, `linblux_player_menu`.
  A primeira execução detectou bugs novos de restauração de FBO; a regressão
  também expôs acne e classificação alfa incorreta no fallback. Foram corrigidos
  antes da aprovação. Logs iniciais mantidos em `validation/`.
- Após as correções, os três afetados foram reexecutados no build final:
  **3/3 PASSARAM**, sem timeout/pendência, em **54.61s**. Depth quality:
  **152 verificações**; materiais **31**; Editor real **126**, todas sem falhas.
  As quatro demais regressões passaram na bateria anterior (Player: 19.16s,
  decals: 6.03s, gizmo: 13.30s, headless: 0.26s).
  Relatórios: `final-validation.json`, `regression.json`, `validation.json`.
- Teste novo verifica sombra de contato (soma RGB **634 → 354** no receptor),
  mudança de direção/alcance/intensidade, piso vazio **0 pixels de acne**,
  Baixo sem shadow map ativo, ordenação independente da inserção, seleção,
  depth dos guias após FXAA/MSAA, cor plana, fallback forçado e redimensionamento.
  Fallback do material comparado ao Compatível: **0 pixels diferentes**.
- ATLAS atualizado, `verify` **OK** e `linblux_atlas` **PASSOU** (31.04s).
  IMPACT `--plan` foi consultado, sem disparar a bateria estrutural inteira.
  Ele reporta cobertura **partial**, headers/CMake sem mapeamento seguro e
  testes fora da seleção. Isto **não equivale a FULL/global aprovado**.
  O restart já falho antes destas fases permanece registrado no baseline;
  não foi repetido sem evidência nova sobre esse problema.
- Capturas antes/depois do executável real: três fixtures × **1366×768 e
  1920×1080**, câmera/luz fixas, HOME/XDG isolados. F8 ligado também capturado
  nas duas resoluções; fixtures validadas no Player, hashes iguais antes/depois.
  Pasta `.turbo/logs/mega3d-phase4/{before,after}/`; supervisão nos logs
  `before-supervisor.log` e `after-supervisor.log`. Inspeção visual confirma
  sombras de paredes/objetos, piso limpo, vidro e decal preservados.

### Custo e limites

Mini benchmark de **3 frames aquecidos**, sete Parts, llvmpipe com 2 threads,
sem UI; não é benchmark de hardware integrado nem estatística de produção:

| Resolução | Baixo / sem AA e sombras | Médio / FXAA + sombras | Alto / MSAA 4× + sombras |
|---|---:|---:|---:|
| 1366×768 | 55.06 ms | 124.74 ms | 203.64 ms |
| 1920×1080 | 102.17 ms | 171.03 ms | 335.24 ms |
| retorno 1366×768 | 50.66 ms | 96.64 ms | 216.06 ms |

Baixo conserva caminho mais barato, com zero shadow map/resolve no automático.
Não há justificativa de orçamento para acrescentar SSAO nesta etapa.
Limites explícitos: uma luz direcional, resolução fixa e sombras limitadas por
alcance, sem cascatas/OIT/refração/transmissão. Interseções de transparências,
superfícies grandes e rigs transparentes ainda têm limites de ordenação por
objeto. Rigs/SpawnLocation e decals conservam iluminação CPU e não recebem
shadow map, embora corpos opacos dos rigs projetem sombra em Parts. Mapas alfa
usam cutoff 0.5 no passe de sombra. Custo/artefatos em GPU integrada real e
aprovação manual do usuário permanecem não verificados. Nenhuma fase posterior
foi iniciada para tentar cobrir essas limitações.

## Revisão visual obrigatória — gizmos (28/10/2026)

O usuário reprovou os visuais anteriores das fases 2 e 4. Os registros de DONE
acima são históricos e **não constituem aprovação visual do usuário**.
Esta primeira correção trata só dos gizmos; sombras seguem para commit separado.

- Precision Tools substitui os círculos de escala por seis quadrados de 12 px
  próximos ao pivô. Extensão base 64–80 px, independente de Part .2/191 unidades;
  linhas-guia até faces distantes removidas. Escala por face/Alt centrada mantidas.
- Mover usa três hastes curtas e setas, arrastáveis nos dois sentidos, mais
  pequenos planos quando apropriados. Girar usa arcos finos com marcadores
  pertencentes ao anel, sem botões circulares desconectados. Acento ativo vermelho,
  base escura e eixos discretos. Face de escala destacada; distância/L-A-P/graus
  apresentados durante arraste. Layout único para desenho e picking.
- GizmoMath não foi alterado. A projeção do arraste da escala passa pelo pivô
  compacto, conservando os cálculos existentes de face oposta/escala centrada.
- Studio e Player reais compilados. Regressões focadas aprovadas: `linblux_tests`,
  `linblux_manipulation` (1.123 checks), `linblux_studio` (3.891),
  `linblux_gizmo_visual` (375 casos/9.336 checks) e `linblux_editor_precision_ui`
  (126). Cobrem local/mundo, snap, cancelamento, histórico, Models e seleção.
  A leitura de traços finos considera apenas a vizinhança 3×3 do segmento real.
- TURBO FAST **9/9 PASSED**, sem timeout/pendência, 8.5s de testes/87.3s total;
  run `20261028T073043Z-13365`. Sem FULL. Build incremental final também aprovado
  após ajuste de rótulos e remoção das antigas âncoras do layout.
- Capturas do Studio real em `.turbo/logs/visual-correction/tools/{before,after}`:
  Mover/Escalar/Girar, perto/longe, 1366×768 e 1920×1080, mesmos dados/câmera/luz.
  Antes preservado do executável `0b88cd5`; depois no binário recompilado.
  Inspeção visual mostra substituição substancial do layout, não só mudança de cor.
- Cena construída sobre cópia do mundo **e rig reais distribuídos** com a engine,
  com parede 191, parede adjacente, peça girada e decal. A cena original da captura
  do usuário não foi fornecida/localizada: não se alega comparação com esse arquivo
  original. Originais não foram alterados; manifests e hashes ficam nos artefatos.
- WIPs anteriores preservados. Fase 5 não iniciada. Aceitação final do usuário
  permanece aberta; sombras ainda não aprovadas nesta revisão.

## Revisão visual obrigatória — sombras (28/10/2026)

Correção restrita ao segundo problema reprovado; **fase 5 não iniciada**.
Os visuais foram inspecionados em capturas reais, além dos testes automatizados.
Aceitação final do usuário e reprodução no arquivo original continuam abertas:
nenhum caminho/imagem original foi fornecido, mesmo após solicitação.

### Diagnóstico e correção

- No rig real distribuído com LINBLUX, o código anterior cobria **111,5 unidades
  em 1024²**, texel de **0,108887 unidade**. Médio e Alto usavam o mesmo mapa e
  nove comparações pontuais em grade 3×3. Os grandes degraus foram reproduzidos
  em câmera próxima/oblíqua; MSAA/FXAA da cena não removiam a quantização interna.
  O perfil da captura original do usuário é desconhecido; a reprodução fixa
  explicitamente os perfis e registra parâmetros reais lidos dos uniforms GL.
- Ajustado o fit à região visível dos receptores, sem descartar casters fora da
  câmera que projetam sombra nessa região. Centro em texels e extensão quantizada
  com histerese reduzem instabilidade sob pequenos movimentos.
- Comparação bilinear de profundidade + disco simétrico fixo, com 4/8/16 amostras;
  sem aleatoriedade por pixel/frame. Derivadas do plano receptor corrigem a
  profundidade de cada amostra, com bias residual menor e contato preservado.
- Médio automático: **1024², 50 unidades, texel 0,0488281, 8 taps** nesta cena.
  Alto automático: **2048², 66 unidades, texel 0,0322266, 16 taps**.
  Há melhoria de aproveitamento/filtragem antes do aumento de resolução:
  2,23× mais densidade no Médio; 3,38× no Alto, comparados ao mapa anterior.
- Visualizar → Sombras oferece Automático, Desligadas, Econômicas, Suaves e Alta.
  Também há `LINBLUX_SHADOW_QUALITY=auto|off|economy|soft|high` e setter do renderer.
  Baixo/Compatível não alocam nem executam shadow map, mesmo com override alto.
  Alta indisponível cai para 1024²/8 taps; falha básica desliga sombras.
  Sem blur global, SSAO, mudanças em materiais/BRDF, física, Lua ou backend.

### Evidência e validação

- Dois executáveis reais recompilados: `build-linblux/linblux` e `linblux_player`.
  Raylib/Bullet/Lua não foram reconstruídos. O build foi incremental sobre os
  consumidores dos cabeçalhos afetados; nenhuma suíte FULL/RmlUi geral executada.
- TURBO FAST **14/14 PASSED**, sem cache, timeout ou pendências; testes 10.1s,
  build 128.9s, total 145.3s; run `20261028T080609Z-32428`.
- Regressões focadas **PASSARAM**: depth quality (**190 checks**, qualidade,
  fallback real para 1024², translação de câmera, estados GL/depth, AA e contato),
  materiais (**31**, fallback com zero diferenças), decals (**9**), controle de
  sombras por UI real (**32**, teste curto isolado) e Player/menu real (**19.60s**).
  Piso vazio: **0 pixels de acne**; ponto de contato: soma RGB **634 → 354**.
  Matemática/gizmos/Studio também foram validados na primeira correção e no FAST.
- Capturas antes/depois de 1366×768 e 1920×1080 com JSON/câmera/luz idênticos:
  `.turbo/logs/visual-correction/shadows/{before,after}` (rig real, perfis e oito
  frames de movimento) e `shadows/{studio-before,studio-after}` (Studio real).
  Os manifests do Studio têm o mesmo hash de projeto. Antes usa o executável
  preservado de `0b88cd5`. Depois usa o recompilado. Não houve pós-blur das imagens.
- A inspeção dos PNGs do Studio nas duas resoluções e do renderer confirma uma
  borda de sombra significativamente mais contínua, com os grandes degraus/bandas
  anteriores removidos. Corpo, chão, decal e UI permanecem nítidos no seu passe.
  O movimento pode ser comparado no visualizador local, sem interpolar os frames:
  `.turbo/logs/visual-correction/index.html` (gizmos, sombras e Studio).
- IMPACT foi consultado; testes relevantes foram selecionados pelo risco de
  renderer/passe GL, consumidores de materiais/decals e runtime Player. O restante
  do plano estrutural não foi executado decorativamente. Isto não é aprovação
  global/Release. O restart historicamente falho não foi repetido sem relação.

### Custo medido e limites

Somente llvmpipe, 2 threads, rig entregue pela engine + chão, três frames de
preparo e mediana de cinco frames. O host apresentou carga/variação de outros
processos; estas amostras curtas **não** permitem concluir desempenho de GPU
física nem uma comparação estatística de velocidade com o baseline:

| Resolução | Baixo / sem sombra e AA | Médio / 1024² + 8 taps + FXAA | Alto / 2048² + 16 taps + MSAA 4× |
|---|---:|---:|---:|
| 1366×768 | 60,31 ms | 281,10 ms | 596,06 ms |
| 1920×1080 | 233,81 ms | 459,61 ms | 933,50 ms |

Alto é caro em software e usa 4× os texels do mapa Médio. Para menor custo,
selecionar Econômicas (1024²/4 taps), Suaves, Desligadas ou perfil Baixo; MSAA
continua configurável separadamente. Os valores incluem o perfil de material/AA,
não apenas o passe de sombra. Não se afirma que Alto é adequado a hardware fraco.

Limites explícitos: mapa único, alcance efetivo adaptativo de 36/48 até 64 unidades
(abaixo do máximo de Lighting); regiões distantes podem ficar sem sombra. Mudanças
grandes de câmera/volume/luz ainda podem causar atualização perceptível do fit;
não há cascatas nem filtro temporal. Rigs/decals continuam com iluminação CPU como
antes, embora o rig projete sombra em Parts. O arquivo original do usuário não foi
alterado nem testado porque não foi identificado. WIPs originais preservados.


## Correção de interação dos gizmos — 2026-10-28

O usuário reprovou os controles pequenos agrupados no pivô. Depois de examinar
esta revisão, aprovou o Scale por face e pediu que Move adotasse a mesma
interação. Esta orientação substitui o pedido anterior de três setas no pivô
como padrão do Studio. Nenhuma fase 5 foi iniciada.

### Resultado implementado

- **Selecionar:** silhueta discreta, sem gizmo, cruzes ou readout técnico.
- **Escalar:** a face inteira é clicável; uma única seta 3D aparece na face em
  hover/arraste. Parede de 191 unidades usa uma âncora na região visível, sem
  exigir acesso ao centro da parede. Escala por face mantém o lado oposto;
  opção Centrada/Alt mantém o centro. Prévia e dimensões em um único indicador.
- **Mover:** mesma aquisição pela face e mesma alça 3D; desloca a Part inteira,
  sem mudar suas dimensões. Local usa a normal local; Mundo escolhe o eixo
  mundial mais próximo da normal. Planos de movimento opcionais restauram
  eixos no pivô com XY/XZ/YZ; ferramentas de rig preservam seus eixos no pivô.
- **Girar:** arcos 3D sem quadradinhos/letras, destaque e graus durante o gesto.
  Iluminação dos controles no desenho CPU melhora contraste sobre peças cinza.
- Desenho/picking compartilham triângulos; faces usam recorte de frustum e
  interseção raio/plano. Oclusão por outra Part opaca impede iniciar o gesto.
  Matemática de transformação, sessões, snap e undo/redo existentes preservados.
  Nenhuma alteração de física, shaders, sombras, áudio ou Lua.

### Evidências

- Studio real recompilado: `build-linblux/linblux`.
- ATLAS: locate/impact consultados; update/verify aprovados.
- Headless focado: `linblux_tests`, `linblux_manipulation`, `linblux_studio`,
  **3/3 PASS**. Inclui Move direto local/mundo em peças pequenas/191 unidades,
  dimensão imutável, oclusão, cancelamento e opção de escala centrada.
- TURBO FAST: **12/12 PASS**, sem cache; run `20261028T092803Z-10975`.
  Build 90,9 s; testes 9,5 s. FULL não executado.
- TURBO gráfico: **379 casos, 4882 verificações, zero falhas**;
  `.turbo/logs/direct-face/graphic-final.log`. A regressão adicional comprova
  que a alça do Move por face aparece também no sentido positivo do eixo.
  A inspeção real detectou e corrigiu essa alça antes da publicação.
- Capturas antes/depois: `.turbo/logs/direct-face/captures/index.html`,
  PNGs de Selecionar/Mover/Escalar/Girar, hover, arraste, resultado, undo/redo
  e ESC nas resoluções 1366×768 e 1920×1080. Baseline preservado de `752b3a4`.
  O script `tests/direct_face_capture.py` usa cópia isolada da mesma cena/câmera,
  verifica JSON salvo após gestos reais e preserva o arquivo-fonte.

Limites: a experiência original do usuário não foi identificada; usamos cena
isolada equivalente, com parede de 191 unidades, piso, parede adjacente e peça
pequena girada com decal. Validação gráfica em llvmpipe, sem extrapolar para GPU
física. FAST é cobertura parcial do produto; não se declara aprovação global.
Os quatro WIPs anteriores foram preservados byte a byte e excluídos do commit.

Validação final no Studio: **PASS, 124,6 s**, supervisionada pelo TURBO;
`.turbo/logs/direct-face/after-final.log`. Move e Scale passaram nas duas
resoluções com undo/redo/ESC. PNGs inspecionados em tamanho original. Manifests
confirmam a mesma cena-fonte antes/depois e o hash do executável final capturado.

## Continuação — encaixe de construção por prévia (28/10/2026)

Incremento seguinte à correção visual de Move/Scale; não altera os commits
publicados anteriores. **Encaixar à superfície** no ribbon Modelo cria uma
proposta de colocação com Face/Borda/Vértice, alinhamento à normal, afastamento
e grade opcional. Enquanto o cursor apenas propõe uma pose, cena, física,
histórico e arquivo salvo não mudam. Clique confirma como undo único; Esc
cancela. Mudança concorrente invalida a proposta. Models usam caixas reais dos
filhos, inclusive com pivô distante. A montagem duplica ao lado/acima pela
extensão OBB e permite repetir o último deslocamento confirmado.

### Validação registrada

- Build TURBO incremental compilou `linblux`, Studio e os alvos FAST selecionados
  (run `20261028T214817Z-23523`, `build.log`).
- `linblux_construction_ui` supervisionado: **216 checks, 0 falhas**, com preview,
  cancelamento, salvar antes do commit, recinto de quatro paredes/piso, undo/redo
  e reabertura em **1366×768 e 1920×1080** (`.turbo/logs/construction-final-focused/`).
- `linblux_studio` passou com `ConstructionPreviewWorkflow`; `linblux_models_ui`
  passou na repetição final com **175 checks** (`.turbo/logs/model-face-retest5/`).
  Uma tentativa intermediária de Models falhou/time-out na interação antiga de
  eixo; o teste foi adaptado à aquisição atual Move por face e depois aprovado.
- TURBO FAST executou **11/11** selecionados sem falhas, mas marcou
  `PASSED_PARTIAL / coverage unknown`; não é aprovação global. Não se repetiu
  IMPACT/FULL após esta etapa nem o restart histórico sem relação.
- ATLAS atualizado/verificado OK após mudanças de código. Capturas/testes usam
  llvmpipe/Xvfb; não houve confirmação manual do usuário, teste no arquivo
  original ou benchmark em GPU física.
- Documentação do Editor e contrato atualizado. O ramo `main` e `origin/main`
  coincidiam em `2386f8f`; este incremento ainda é WIP local e **não publicado**.
  A grade visual de construção segue como incremento separado; fases 5–7 não
  iniciadas nesta etapa.
