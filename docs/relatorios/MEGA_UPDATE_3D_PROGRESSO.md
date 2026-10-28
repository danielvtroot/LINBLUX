# Mega Update 3D — progresso das fases 0 e 1

Execução encerrada em 28/10/2026 (America/Belem). Escopo exclusivo: baseline,
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
