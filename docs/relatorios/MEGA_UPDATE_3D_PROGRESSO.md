# Mega Update 3D — progresso das fases 0 a 3

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
Seu hash é o commit que adiciona este relatório (`git log -1 --format=%H --
docs/relatorios/MEGA_UPDATE_3D_PROGRESSO.md`). Push normal para `origin/main`;
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
- Commit: `feat(editor): add cursor surface placement and absolute grid`.
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
  a composição), mesmas pixels Studio/Player sem overlays com câmera idêntica,
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
