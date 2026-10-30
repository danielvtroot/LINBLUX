# LINBLUX ATLAS 2.0 — progresso

Registro de checkpoint para outro agente continuar sem reler o projeto.
Documento de referência: `docs/relatorios/AUDITORIA_ATLAS_2_0.md`.

## Fases concluídas

### Fase 1 — indexação e velocidade (P0)

- `.turbo/`, `prototype/`, `node_modules/`, `.atlas/`, caches e builds excluídos
  da indexação (`tools/atlas/atlas_lib/config.py`).
- `update` usa caminho rápido por `size + mtime_ns + ctime_ns`: arquivo inalterado
  não é lido nem sofre SHA-256 (`tools/atlas/atlas_lib/indexer.py`).
- Alteração com `mtime` preservado continua detectada porque `ctime` muda; o
  hash confirma o conteúdo antes de reindexar.
- `update` resposta inclui `bytes_read`, `rehash`, `added/updated/deleted/unchanged`.
- Esquema versionado em 2 (`tools/atlas/atlas_lib/db.py`); índice incompatível é
  reconstruído.

### Fase 2 — ranking estável (P1)

- Pool de candidatos FTS fixo (`CANDIDATE_POOL = 240`), independente de
  `--limit`; o top-1 não muda mais entre limites.
- Evidência textual sempre antes de inferência de glossário
  (`INFERRED_SCORE_CAP`), com `match_types` e `score_breakdown` por arquivo.
- `files[]` do `locate` traz `line`, `symbols`, `tests`, `textual`, `inferred`.

### Fase 3 — `tests-for` e `impact` (P2)

- `tests-for` (`tools/atlas/atlas_lib/graph.py`) ordena evidência:
  1. fonte do teste;
  2. `include` — fonte de teste que inclui o arquivo ou um header do seu fecho
     (profundidade 3, headers genéricos com fan-out > 4 descartados);
  3. mesmo target;
  4. cadeia de targets (estrutural, `inferred`, limitada a 8);
  5. módulo do arquivo (aproximação, `inferred`).
- Cada teste retorna `confidence` e `reasons`; `harness` continua marcado quando
  uma fonte é compartilhada por mais de 3 testes.
- `impact` percorre include reverso + grafo de targets e lista testes com motivo.

### Fase 4 — `diagnose` / `fix-plan` (P3)

- `atlas.py diagnose "sintoma"` e `atlas.py fix-plan "sintoma"` em
  `tools/atlas/atlas_lib/diagnose.py`, reutilizando `locate`, `tests_for` e
  `impact` na mesma conexão SQLite.
- Saída humana curta por padrão, truncada em limite de linha com marcador
  explícito; `--detail` mostra tudo, `--max-chars` ajusta o orçamento.
- `--brief`/`--json` retornam payload reduzido e determinístico.
- Hipóteses são rotuladas como hipótese; nada é declarado causa confirmada.
- `fix-plan` é somente leitura (`edits_code: false`).

### Fase 6 — contrato universal (P5, parcial)

- Envelope de erro uniforme: todo `--json` com falha traz `ok: false` e `error`
  no topo (`tools/atlas/atlas_lib/cli.py`).
- Cartão curto no `AGENTS.md` e comandos documentados em `docs/atlas/README.md`
  e `docs/atlas/json.md`.

## Medições (30 de outubro de 2026, índice real de 423 arquivos)

| Operação | Antes (auditoria) | Depois |
| --- | ---: | ---: |
| `update` sem alterações | ~930 ms | 98–101 ms |
| `locate` | 60–150 ms | 98–110 ms |
| `symbol` | ~70 ms | 65–69 ms |
| `tests-for` | ~70 ms | 66–67 ms |
| `impact` | ~90 ms | 68–74 ms |
| `diagnose` (novo) | inexistente | 115–227 ms |
| `doctor` | ~200 ms | 237 ms |
| `verify` | ~250 ms | 265–276 ms |
| Pico de RSS (`bench`) | ~36 MB | 35,9 MB |
| Banco SQLite | 4,1–7,5 MB | 7,5 MB |

`bench --iterations 3`: índice completo 2.924 ms; update de 1 arquivo 311 ms;
update de 5 arquivos 335 ms; consulta média 73,6 ms; máxima 106,6 ms;
`peak_rss_kib` 35.908; banco 7.503.872 bytes.

Índice atual: 423 arquivos, 4.455 símbolos, 1.279 relações, 44 targets,
64 testes.

## Ranking (8 sintomas da auditoria)

| Métrica | Baseline | Atual | Meta |
| --- | ---: | ---: | ---: |
| Arquivo principal top-1 | 1/8 | 4/8 | — |
| Arquivo principal top-3 | 1/8 | 6/8 | ≥ 5/8 (atingida) |
| Arquivo principal top-5 | 5/8 | 6/8 | ≥ 7/8 (**não atingida**) |
| Arquivo principal top-10 | — | 7/8 | — |
| MRR (arquivo) | — | 0,653 | — |
| Módulo top-3 | — | 6/8 | — |
| Estabilidade entre limites | 2/8 | 8/8 | 8/8 (atingida) |

Sintomas ainda fora do top-5: S4 ("Part dinâmica cai durante edição" →
`src/physics/PhysicsWorld.cpp` na posição 12) e S6 ("teste de Mundo Padrão
expira por timeout" → `tests/engine_default_main.cpp` na posição 7).

## `diagnose` — tamanho de saída

Texto humano nos 8 sintomas: 994–1.094 caracteres (limite padrão 1.100,
`--max-chars` configurável, `--detail` sem corte). JSON `--brief` reduzido a
5 arquivos, 3 testes e 2 hipóteses.

## `tests-for` nos 6 arquivos C++ verificados

Todos retornam associação com motivo explícito, por exemplo:

- `src/editor/ConstructionPlacement.cpp` → `linblux_gizmo_visual`
  (`tests/gizmo_visual_main.cpp inclui src/editor/Gizmo.h`).
- `src/rendering/PlayerMenu.cpp` → `linblux_audio_panel_hd_ui`
  (`tests/rml_ui_main.cpp inclui src/rendering/PlayerMenu.h`).

As razões são estruturais (include/CMake), não prova de execução, e a
especificidade varia: headers muito incluídos geram vários testes `confirmed`.
A meta "6/6 com associações justificadas" está atendida no sentido estrutural;
a meta de especificidade fina (teste que realmente exercita o arquivo) continua
aberta.

## Testes

```bash
python3 -m unittest discover -s tests/atlas -v   # 20 testes, OK
python3 tools/atlas/atlas.py verify               # OK, hashes conferem
python3 tools/turbo/turbo.py test --mode fast --changed --plan
```

Novos testes de regressão em `tests/atlas/test_atlas.py`
(`AtlasV2RegressionTest`): diretórios gerados ignorados, segundo `update` sem
leitura, alteração com `mtime` preservado, ranking estável entre limites,
`line`+`match_types` no `locate`, `tests-for` por include, `impact` alcançando
teste dependente, `diagnose` curto com JSON válido e envelope de erro uniforme.

## Missão 1 — integração ATLAS + TURBO (P4)

Arquivos: `tools/turbo/turbo_lib/selection.py`, `report.py`, `runner.py`,
`tests/turbo/test_turbo.py`.

- `selection.py` agora usa evidência de **include** do ATLAS, de forma
  puramente aditiva: para cada arquivo alterado ele parte do próprio arquivo e,
  em `.cpp`, do header irmão (`.h/.hpp/.hh/.hxx` presente no índice), percorre
  dois níveis de include reverso e mapeia fontes de teste para testes.
  - Razões novas: `include direto do ATLAS: <fonte> inclui <header>` (obrigatório)
    e `include transitivo do ATLAS: ...` (adicional, profundidade 2).
  - Nenhuma razão existente é removida; o alvo de testes nunca encolhe.
  - Cada teste selecionado recebe `evidence`: `direct`, `transitive` ou
    `inferred`; o plano publica `evidence_counts` e `atlas_evidence`.
  - Fallback: sem relações no índice, `atlas_evidence = "unavailable"`, aviso
    explícito e seleção mantida pelo grafo de targets.
- `report.py`: `first_error()` extrai o **primeiro** erro acionável e distingue
  compilação, configuração CMake, link e teste. O bloco de falha mostra
  `Tipo`, `Provável causa`, `Arquivo: linha:coluna`, `Target` e `Próxima ação`.
- `runner.py`: `excerpt()` passou a guardar as **primeiras** linhas de erro
  (antes guardava as últimas, que costumam ser consequência do mesmo erro) e a
  truncar pelo início.

### Equivalência medida no repositório real (30 arquivos alterados)

| Modo | Seleção antiga | Seleção nova | Removidos | Adicionados |
| --- | ---: | ---: | ---: | ---: |
| FAST | 18 | 20 | 0 | 2 |
| IMPACT | 63 | 64 | 0 | 1 |

Nenhum teste obrigatório desapareceu. A comparação de *nomes* é o critério de
segurança; as strings de razão de target mudam porque a lista de targets
atingidos é unida no texto (`consumidor do target: a, b, c`).

Evidência no plano FAST: `direct=14, transitive=6, inferred=0`;
IMPACT: `direct=54, transitive=7, inferred=3`.

Testes: `python3 -m unittest discover -s tests/turbo -p 'test_*.py'` → 38 OK,
incluindo aditividade (antigo ⊆ novo), marcação direto/transitivo, fallback sem
relações, primeira falha com arquivo/linha/target e ordem do `excerpt`.

## Missão 2 — ranking S4/S6

Alteração em `tools/atlas/atlas_lib/search.py` (`_area_intent`, aplicado em
`locate`): quando a consulta fala de teste (`teste/testes/test/ctest/timeout/
cobertura/suite`), fontes de teste recebem peso de área 2,0 e fontes de código
0,8. É intenção declarada na própria pergunta, não uma lista de sintomas nem
nome de arquivo; a razão aparece em `evidence`.

| Métrica (limite 8 e 50) | Antes | Depois |
| --- | ---: | ---: |
| Arquivo principal top-1 | 4/8 | 4/8 |
| Arquivo principal top-3 | 6/8 | **7/8** |
| Arquivo principal top-5 | 6/8 | **7/8** |
| Arquivo principal top-10 | 7/8 | 7/8 |
| MRR (arquivo) | 0,653 | 0,677 |
| Módulo top-1 | 6/8 | **7/8** |
| Estabilidade entre `--limit 3,5,8,20,50` | 8/8 | 8/8 |

- S6 ("teste de Mundo Padrão expira por timeout"):
  `tests/engine_default_main.cpp` passou da posição **7 para 3** nos dois limites.
- S1, S2, S3, S5, S7, S8 não mudaram de posição principal.
- S4 continua em 12 (`src/physics/PhysicsWorld.cpp`); `src/app/Application.cpp`
  não aparece nem no top-50. A consulta descreve um comportamento
  ("Part dinâmica cai durante edição") cujo vínculo com o passo de física não é
  textual — exige camada semântica, não um ajuste de peso.

Uma tentativa de ponderação por IDF dos tokens (0,6–2,5 sobre caminho,
glossário e símbolos) foi medida e **descartada**: derrubou o top-3 de 6/8 para
4/8 e o top-5 para 5/8, porque tokens genéricos como `part` também sustentam
acertos corretos (S1, S3, S8). O FTS5 já aplica IDF no bm25; só a intenção de
área ficou.

## Missão 3 — parser C++ e avaliação

### Qualificação C++ (`tools/atlas/atlas_lib/indexer.py`)

- `_namespace_path()` acumula namespaces **aninhados**
  (`namespace loblox { namespace editor { … } }` → `loblox::editor::…`) e
  aceita a forma compacta `namespace loblox::editor {`.
- Definição fora de classe já qualificada (`int loblox::Widget::value()`)
  não recebe o namespace duas vezes.
- `kind` continua `method` para definições `Classe::nome` e para métodos inline;
  funções livres permanecem `function`.
- Sobrecargas continuam como símbolos distintos (linha/assinatura próprias),
  não como um único registro.
- Após reindexar: 1.678 definições C++ em `.cpp`, **239 sem namespace** — todas
  funções globais legítimas (`main`, helpers de teste como `require`), nenhuma
  perda de namespace. Métodos aparecem como
  `loblox::experiences::Stage::publish`, `loblox::game::Inventory::model`.
- Testes novos: `tests/atlas/test_cpp_parser.py` (8 casos: fora de classe,
  namespace aninhado, namespace compacto, já qualificado, função livre,
  sobrecargas, método inline, função global). Inclui um teste que **documenta a
  limitação** conhecida: o scanner é orientado a linha e ignora um namespace
  inteiro escrito em uma única linha.

### Avaliação por arquivo (`tools/atlas/data/eval_questions.json`, `report.py`)

- 40 perguntas, cada uma com `expect_files` verificados contra o índice
  (`missing expected_files: []`), cobrindo editor, gizmos, construção, física,
  rendering, decals, player, áudio, Lua, inventário, NPC, persistência,
  exportação, assets e testes.
- `eval` agora mede **arquivo** além de módulo: `file_r1/r3/r5`, `file_mrr`,
  `file_rank` por pergunta e `file_evaluated` (só perguntas com `expect_files`).
- Resultado medido neste ambiente: 40 perguntas, módulo top-1 29/40 (0,725),
  top-3 36/40 (0,900), módulo presente 39/40 (0,975), símbolo 23/40 (0,575);
  **arquivo R@1 7/40, R@3 20/40, R@5 24/40, MRR 0,362**.
- Limitação declarada no próprio JSON: arquivos esperados são relacionados
  verificados, não causas comprovadas; a amostra é manual.

## Pendências e próxima ação

1. **P4 — TURBO (restante)**: a seleção agora é aditiva e explicável, mas a
   especificidade continua estrutural: `include` prova dependência de
   compilação, não que o teste exercita o comportamento. `coverage` segue
   `unknown` quando o CMake muda ou há caminhos sem mapeamento.
2. **P6 — C++ (restante)**: namespaces aninhados resolvidos; templates,
   macros e callbacks continuam textuais e sem camada semântica.
3. **Avaliação**: 40 perguntas com arquivo medido; R@5 de arquivo é 24/40 e
   MRR 0,362 — há espaço real de ranking, não é falta de amostra.
4. **Ranking**: S6 resolvido; S4 exige camada semântica (não é peso).
5. `locate --json` ainda é grande (28 KB); usar `--brief` ou `diagnose` em fluxo
   de agente.

Nada foi commitado além dos arquivos do ATLAS, dos testes e da documentação.
Alterações preexistentes de outros agentes permanecem intocadas.

## Correção real no Studio (30/10) — Mover pela face

- `src/ui/EditorActions.cpp`: a ação `move-planes` agora altera apenas o
  `rigGizmo_`; o gizmo de Part/Model nunca troca para planos/eixos por um
  controle oculto. O contrato aprovado é face direta.
- `src/editor/EditorUI.cpp` (`setTool`): reafirma `setFaceMove(true)` e
  `setPlanarHandles(false)` ao trocar de ferramenta, para que estado herdado
  (rig, preferência, código antigo) não traga o gizmo de setas de volta.
- Localização: uma consulta `diagnose` (~150 ms medidos antes) apontou
  `src/editor/Gizmo.h`, `TransformSession.h` e `Gizmo.cpp`; o ajuste saiu em
  2 arquivos, sem tocar no frontend.
- Verificação: build incremental OK; `ctest -R '^linblux_tests$'` OK (0,9 s),
  incluindo o bloco que exige deslocamento pela face com `planarHandles` ligado.
- Não executado: `linblux_manipulation_ui` (gráfico, exige supervisão TURBO e
  orçamento próprio) — cobre o caso visual de Mover.
