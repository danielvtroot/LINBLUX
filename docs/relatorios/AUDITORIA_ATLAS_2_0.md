# AUDITORIA ATLAS 2.0 — velocidade, precisão e integração com ferramentas de desenvolvimento

> Auditoria somente-leitura do ATLAS atual. Nenhuma alteração foi feita no código
> do Studio, no código do ATLAS ou no TURBO. Este documento é autossuficiente e
> serve de entrada para outro desenvolvedor projetar o ATLAS 2.0.

| Item | Valor |
| --- | --- |
| Projeto | `/home/labubu/projeto/linblux` |
| Commit auditado (HEAD local = `origin/main`) | `7dd3aa06601b624fab5a538101ecdcaa0ba65881` |
| Data da auditoria | 30/10/2026 |
| ATLAS | `0.1.0`, `INDEX_SCHEMA_VERSION=1`, `INDEX_VERSION=1` |
| Hardware | AMD Ryzen 5 3500U (8 threads), 5,8 GiB RAM, **HDD** WDC WD10SPZX (`/sys/block/sda/queue/rotational=1`), 776 GB livres |
| Software | Python 3.13.5, SQLite 3.46.1 + FTS5, Linux 6.12.113 |
| Ferramentas semânticas | `clangd`, `clang`, `clang-format`, `ctags`, `rg`, `cscope`, `sqlite3` **ausentes** |

---

## 1. Resumo executivo

O ATLAS atual é um índice estrutural correto na arquitetura e barato em recursos,
mas **não entrega a precisão que justifica o fluxo "descreva o erro e vá direto ao
arquivo"** — que é exatamente o fluxo que ferramentas de terminal consomem.

Fatos medidos nesta máquina (HDD, cache de página aquecido, salvo indicação):

1. **Localização tem Recall@1 de arquivo de 1/8** nos oito sintomas reais testados
   (MRR 0,256 no arquivo principal; 0,475 contando arquivos relacionados).
   Recall@5 = 5/8; Recall@10 = 6/8. Em nível de módulo: R@1 = 1/8, R@3 = 5/8.
2. **O ranking não é determinístico em relação a `--limit`**: em 6 dos 8 sintomas o
   primeiro módulo/arquivo muda entre `--limit 8` (padrão) e `--limit 50`
   (ex.: S1 `scene/Scene.h` → `game/Runtime.h`; S7 `game/Npc.cpp` →
   `scripting/LuaRuntime.cpp`). Isso contradiz `docs/atlas/architecture.md`.
3. **`tests-for` devolve zero testes para todo código-fonte de biblioteca**
   (`PlayerMenu.cpp`, `SceneRenderer.cpp`, `ConstructionPlacement.cpp`,
   `EditorUI.cpp`, `LuaRuntime.cpp`, `Part.cpp`), por consultar a direção errada do
   grafo CMake. A sugestão de testes — o item de maior valor para um agente — está
   efetivamente quebrada fora dos headers.
4. **`.turbo/` é indexado e percorrido**: 9.077 arquivos (197 MB) entram no
   `os.walk`; 21 arquivos de log/script de agentes entram no índice com módulo
   `root` e aparecem em resultados (`locate` S2, S4, S8). Excluir `.turbo` reduz
   `collect_files` de **525 ms para 31 ms** e `skipped` de 5.932 para 43.
5. **`update` lê e calcula SHA-256 de todos os 443 arquivos sempre** (4,22 MB):
   15 ms com cache quente, mas **7.132 ms** com as páginas frias (`fadvise`), 656×.
   Um `update` sem nenhuma alteração custa 928–966 ms.
6. **Consulta com índice frio custa 2.071 ms**; a mesma consulta com cache quente
   custa 42,8 ms. O índice tem 9,96 MB (4,21 MB só de conteúdo FTS).
7. **Definições C++ fora da classe perdem o namespace** em 645 de 1.330 símbolos
   (`Application::initialize` em vez de `loblox::Application::initialize`), e o
   `kind` fica `function` em vez de `method` — o mesmo símbolo aparece duas vezes
   com nomes diferentes.
8. **`impact` não atravessa targets intermediários**: para
   `src/editor/ConstructionPlacement.cpp` devolve `affected_count=0`, embora o
   alvo `linblux_editing` seja consumido por `linblux_core` → `linblux_studio`.
9. **Saída grande para o agente**: `locate` em JSON = 23.934 caracteres;
   `impact src/scene/Scene.h` = 19.801; `trace` = 7.709. Em modo texto, o mesmo
   `locate` ocupa 3.322 caracteres (7× menos), mas perde `score_breakdown`.
10. **A avaliação (`eval`) mede só módulo**, com 12 perguntas escritas no
    vocabulário do glossário: hoje dá 9/12 no top-1 (documentado como 12/12) e
    continua 12/12 em top-3 — ou seja, ela não detecta "módulo certo, arquivo
    errado", que é o modo de falha real (S3, S4, S5, S8).

Prioridades recomendadas (detalhe nas seções 17–20):

| # | Correção | Impacto | Custo |
| --- | --- | --- | --- |
| P1 | Ignorar `.turbo/`, `prototype/` e logs no índice e no walk | alto (velocidade + ruído) | trivial |
| P2 | `tests-for`/`impact` corretos (direção do grafo CMake, BFS por target) | alto (seleção de testes) | baixo |
| P3 | Ranking estável: `--limit` só corta, nunca reordena; inferido nunca acima de evidência textual | alto (precisão) | baixo |
| P4 | `update` incremental por `mtime`+`size` com hash só quando necessário | alto (HDD) | baixo |
| P5 | Saída em camadas (curto/detalhado/diagnóstico) com limite de caracteres | alto (tokens) | baixo |
| P6 | Namespace/kind corretos em definições fora de classe | médio (símbolos) | baixo |

---

## 2. Estado do projeto e Git

Comandos executados no início da auditoria (somente leitura):

```bash
git status -sb
git rev-parse HEAD
git rev-parse origin/main
git log -1 --format='%H %ci %s'
```

- `HEAD` local = `7dd3aa06601b624fab5a538101ecdcaa0ba65881`.
- `origin/main` = o mesmo commit. **Não havia commits locais não publicados.**
- Último commit: `2026-10-29 12:35:02 -0300 fix(editor): preserve face manipulation and repair regression fixtures`.

WIP presente no working tree no início da sessão (25 entradas: 22 modificadas +
3 não rastreadas). **Nada disso foi alterado, formatado ou descartado.**

| Estado | Arquivos |
| --- | --- |
| `M` (22) | `CMakeLists.txt`, `assets/ui/documents/ribbon.rml`, `assets/ui/documents/studio.rml`, `assets/ui/styles/player.rcss`, `assets/ui/styles/studio.rcss`, `docs/EDITOR_PRECISAO.md`, `docs/atlas/module-contracts/{editor,rendering,ui}.md`, `src/editor/ConstructionUI.cpp`, `src/editor/EditorUI.{cpp,h}`, `src/rendering/PlayerMenu.{cpp,h}`, `src/rendering/SceneRenderer.{cpp,h}`, `src/scene/Scene.{cpp,h}`, `src/ui/EditorActions.cpp`, `src/ui/StudioFrontend.cpp`, `tests/player_menu.py`, `tests/rml_ui_main.cpp` |
| `??` (3) | `docs/relatorios/AUDITORIA_COMPLETA_MOTOR_3D_LINBLUX.md`, `docs/relatorios/LINBLUX_PLANO_MEGA_UPDATE_3D.md`, `tests/fixtures/audio/null.conf` |

Efeitos colaterais da auditoria (registrados por transparência):

- `atlas.py update` foi executado (é o fluxo documentado). Ele grava **apenas**
  `.atlas/` e reindexou os 4 arquivos que estavam defasados
  (`CMakeLists.txt`, `docs/atlas/module-contracts/ui.md`, `tests/player_menu.py`,
  `tests/rml_ui_main.cpp`). Nenhum arquivo versionado mudou por causa disso.
- Durante a medição de códigos de saída, o comando `docs` foi executado e
  regenerou `docs/atlas/generated/inventory.md` (arquivo gerado e versionado).
  O arquivo foi **restaurado para o estado do HEAD** (`git checkout --`), e o
  achado ficou registrado: o inventário versionado estava defasado (235 arquivos
  indexados contra 443 reais — ver seção 16, B13).
- Verificação final: `git status -s` voltou às mesmas 25 entradas do início.

---

## 3. Arquitetura do ATLAS

### 3.1 Componentes

```text
                    tools/atlas/atlas.py  (entrada; injeta tools/atlas no sys.path)
                                  │
                                  ▼
                        atlas_lib/cli.py  (argparse, --json, códigos de saída)
        ┌───────────────┬─────────────┼──────────────┬────────────────┐
        ▼               ▼             ▼              ▼                ▼
  indexer.py       search.py      graph.py     diagnostics.py    report.py
  (descoberta,     (FTS5+rank,    (impact,     (status, doctor,  (docs, bench,
   hash, parsers,   symbol,        tests-for,   verify)           eval)
   CMake, grafo)    text)          trace)
        │               │             │              │                │
        └───────────────┴──────┬──────┴──────────────┴────────────────┘
                               ▼
                        db.py (SQLite WAL + FTS5)  ──►  .atlas/index.sqlite
                               ▲
                               │ leitura direta (URI mode=ro)
                     tools/turbo/turbo_lib/inventory.py
```

Dados auxiliares: `data/glossary.json` (20.956 B, 81 termos),
`data/eval_questions.json` (3.086 B, 12 perguntas),
`schemas/output.schema.json` (350 B — **não é validado por nenhum código ou teste**).

### 3.2 Fluxo completo de uma pergunta de agente

```text
PERGUNTA ("uma part dinamica cai durante edicao")
   │
   │ cli.py:main → _normalize_argv → argparse
   ▼
open_db(.atlas/index.sqlite)                        [abre WAL; sem checar staleness]
   │
   ▼
search.locate()
   ├─ query_tokens()      normaliza acentos, remove stopwords, aplica "plural PT"
   ├─ _hint_map()         lê glossary.json do disco (a cada consulta)
   ├─ _candidate_rows()   FTS5 MATCH (OR dos tokens) + bm25(path 2.5, module 1.5, content 1.0)
   │                      pool = max(limit*8, 40) linhas  ← muda com --limit
   ├─ _synthesize_hint_rows() adiciona módulos do glossário SEM evidência textual
   ▼
RANKING por arquivo
   score = fts + 4.0/termo no caminho + glossário(≤18) + símbolos + 1.5/teste(≤4.5)
           + 5.0 se inferido   × peso de área (source 1.0 … tool 0.35)
   │  _symbol_hits() 1 query por arquivo; _test_hits() 1 query por arquivo
   ▼
agrupamento por módulo (máx. 3 arquivos; 1º peso 1.0, seguintes 0.5)
   ▼
EVIDÊNCIAS (até 6 strings por arquivo: caminho, glossário, símbolo, teste, trecho)
   ▼
ARQUIVO/SÍMBOLO (arquivo + módulo + score_breakdown + symbols[] + tests[])
   ▼
TESTE SUGERIDO (nomes vindos de _test_hits → tests JOIN test_files)
   ▼
next[] ("consultar contrato …", "executar ctest -R '^nome$'")
```

### 3.3 Onde o fluxo perde precisão, contexto e velocidade

| Etapa | O que faz | Perda medida |
| --- | --- | --- |
| Tokenização | radical de plural PT + stopwords curtas | "ancorada", "encaixa", "espelhado", "explorador" não têm sinônimo; "porque", "durante" viram termos de busca |
| FTS | OR de tokens citados; o token vale em qualquer lugar do arquivo | documento de 142 KB em `docs/` compete com 1 função em `src/` (S5 e S8: docs ocupam top-1..3) |
| Pool de candidatos | `max(limit*8, 40)` linhas | **muda o resultado**: 6/8 sintomas trocam de top-1 entre `--limit 8` e `50` |
| Glossário | +7 por módulo, +5 por candidato inferido | candidato inferido sem nenhuma palavra no código venceu evidência textual (S1: `scene` 39,0 vs `PlayerMenu.cpp` 31,6) |
| Símbolos | `LIKE '%token%'` no nome/qualificado | casa `part` com `Part` e com `part` de scripts Python; 15 consultas por `locate` |
| Testes | 1 query por arquivo candidato | 67 queries em um único `locate`; e `tests_for` devolve 0 para fontes de biblioteca |
| Agrupamento | corta em 3 arquivos/módulo e ordena por módulo | o arquivo correto pode ficar atrás de um grupo errado (S3: `SceneRenderer.cpp` é o 5º arquivo) |
| Saída | JSON sem limite de tamanho | 23.934 caracteres por `locate` (≈6 mil tokens) |
| Staleness | nenhuma consulta avisa índice velho | agente pode receber resposta de índice desatualizado sem saber |

---

## 4. Inventário de módulos e funções

Nove arquivos Python, 3.332 linhas. Todas as funções abaixo foram confirmadas no
código (linhas verificadas).

### 4.1 `tools/atlas/atlas.py` (14 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Ponto de entrada; insere `tools/atlas` em `sys.path` e delega para `atlas_lib.cli.main` |
| API pública | nenhuma (script) |
| Entradas | `sys.argv` |
| Saídas | código de saída de `main()` |
| Consulta/grava | nada diretamente |
| Algoritmo | import + `raise SystemExit(main())` |
| Custo | ~57–66 ms (é o custo de importar Python + `atlas_lib`) |
| Limitações | nenhuma específica; toda a lógica vive em `atlas_lib` |
| Testes | exercitado por todos os testes de `tests/atlas/test_atlas.py` |

### 4.2 `atlas_lib/cli.py` (371 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Parser argparse, normalização de flags globais, renderização texto/JSON, códigos de saída |
| API pública | `build_parser()`, `main(argv)`, `_normalize_argv()`, `_text_render()` |
| Entradas | argv; `--root`, `--json/-j`, `--quiet/-q`, subcomando |
| Saídas | stdout (texto ou envelope `{ok,command,data}`), stderr em erro, código 0/1/2 |
| Consulta/grava | abre o banco para os comandos de consulta; delega escrita a `indexer`/`report` |
| Algoritmo | `_normalize_argv` move `--json/--root/--quiet` para antes do subcomando; `_text_render` tem um bloco por comando |
| Custo | 57–66 ms de startup; `--version` = 20 caracteres |
| Limitações | envelope de erro inconsistente (`impact` põe `error` dentro de `data`); `doctor --strict` retorna 1 com achados informativos; `trace`/`impact` sem limite de saída |
| Testes | `test_init_status_verify_and_json_contract`, `test_invalid_paths_are_rejected` |

### 4.3 `atlas_lib/config.py` (290 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Regras de exclusão, detecção de linguagem, mapeamento path→módulo/área, normalização de texto, glossário, stopwords |
| API pública | `repo_root`, `cache_dir`, `db_path`, `load_user_config`, `detect_language`, `should_index`, `module_for`, `area_for`, `is_external`, `normalize_text`, `glossary` |
| Entradas | caminho relativo, `Path`, configuração opcional em `.atlas/config.json` |
| Saídas | bool/str/dict |
| Consulta/grava | lê `.atlas/config.json` e `tools/atlas/data/glossary.json` |
| Algoritmo | listas de sufixos/nomes/prefixos; `stat()` para o limite de 2 MiB |
| Custo | chamado 1× por arquivo no walk; sem cache (glossário é relido por consulta) |
| Limitações | **`.turbo` ausente de `IGNORED_DIRS` (linha 61)**; `module_for` devolve `root` para `prototype/`, `.turbo/`, arquivos da raiz; JSON só é indexado sob 5 prefixos; sem padrão para `.conf` |
| Testes | `test_ignored_build_and_third_party_directories`, `test_symlink_outside_root_is_rejected` |

### 4.4 `atlas_lib/db.py` (247 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Esquema SQLite/FTS5, conexão WAL, metadados, transação, integridade |
| API pública | `open_db`, `new_connection`, `set_meta`, `get_meta`, `meta_dict`, `schema_version`, `index_is_compatible`, `table_counts`, `snapshot_files`, `update_meta_stats`, `index_status`, `transaction`, `corrupt_or_missing_reason`, `remove_index`, `ensure_cache_dir` |
| Entradas | `Path` da raiz |
| Saídas | `Connection`, dicts de contagem/estado |
| Consulta/grava | cria/abre `.atlas/index.sqlite` (+`-wal`,`-shm`) |
| Algoritmo | `executescript(SCHEMA)`; `PRAGMA journal_mode=WAL, synchronous=NORMAL, foreign_keys=ON, busy_timeout=30000`; `BEGIN IMMEDIATE` |
| Custo | abrir conexão ~1 ms; `integrity_check` + FTS integrity no `verify` |
| Limitações | `delete` de FTS no índice usa subconsulta após apagar a linha (no-op, mascarado pela limpeza de órfãos); nenhuma migração incremental de esquema (versão incompatível ⇒ recria tudo) |
| Testes | `test_corrupt_index_recovers`, `test_init_status_verify_and_json_contract` |

### 4.5 `atlas_lib/indexer.py` (960 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Descoberta, hash, parsers de linguagem, resolução de includes, parse de CMake, indexação incremental |
| API pública | `parse_text`, `parse_cpp`, `parse_lua`, `parse_python`, `parse_rml`, `parse_markdown`, `parse_cmake`, `collect_files`, `read_text_file`, `index_repository` (+ auxiliares `_blank_cpp`, `_namespace_intervals`, `_class_intervals`, `_matching_brace`, `_resolve_include`, `_normalize_cmake_path`) |
| Entradas | raiz, `force`, `quiet`; `Path` de arquivo |
| Saídas | dict `stats` (arquivos, símbolos, relações, tempo, bytes, contagens) |
| Consulta/grava | lê todos os arquivos indexáveis; grava `files`, `symbols`, `relations`, `file_fts`, `cmake_targets`, `target_sources`, `tests`, `test_files`, `external_deps`, `meta` |
| Algoritmo | `os.walk` → `should_index` → `read_text_file` → SHA-256 → se mudou, `parse_text`; transação única; grafo CMake reconstruído por inteiro; resolução tardia de includes inferidos |
| Custo | `collect_files` 514–525 ms; `parse_cmake` 25,2 ms; parse de todos os arquivos 1.351 ms; arquivo mais lento `LuaRuntime.cpp` 124,8 ms |
| Limitações | lê/hasheia **todos** os arquivos em todo `update` (linha 758); perde namespace em `Class::method` (linhas 336–339); `glossary_terms` gravado como 1; reescreve as tabelas de CMake/testes em toda execução |
| Testes | `test_incremental_add_modify_delete_and_verify`, `test_mtime_only_change_refreshes_status_metadata`, `test_ignored_build_and_third_party_directories` |

### 4.6 `atlas_lib/search.py` (594 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Busca FTS5, ranking explicável, lookup de símbolo, busca textual |
| API pública | `stem_token`, `query_tokens`, `locate`, `symbol`, `text_search`, `overview` (+ `_hint_map`, `_hint_score`, `_symbol_hits`, `_module_symbols`, `_module_rows`, `_synthesize_hint_rows`, `_candidate_rows`, `_test_hits`, `_module_contract`, `_confidence`) |
| Entradas | `Connection`, consulta em texto, `limit`, `module` |
| Saídas | dict com `query`, `tokens`, `confidence`, `candidates[]`, `next[]`, `limitations[]`, `total_files_considered` |
| Consulta/grava | lê `file_fts`, `files`, `symbols`, `tests`, `test_files`; lê `docs/atlas/module-contracts/*.md` e `glossary.json` do disco |
| Algoritmo | FTS BM25 + heurísticas aditivas (caminho, glossário, símbolo, teste, inferido) × peso de área; agrupa por módulo |
| Custo | 66 ms / **513 statements** por `locate` (67× `_test_hits`, 15× `_module_symbols`) |
| Limitações | ranking depende de `--limit`; inferido pode vencer evidência textual; sem stemming real; `symbol` faz `LIKE '%nome%'` (falsos positivos) e lê arquivos do disco para referências |
| Testes | `test_locate_symbol_text_and_tests`, `test_limits_are_deterministic` (compara 2 execuções com o **mesmo** limite — não detecta a instabilidade entre limites), `test_doctor_reports_only_nonfatal_fixture_issues` |

### 4.7 `atlas_lib/graph.py` (244 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Impacto reverso, testes associados, fluxo provável |
| API pública | `safe_project_path`, `impact`, `tests_for`, `trace`, `symbols_in_file` |
| Entradas | caminho relativo ou símbolo; `depth` 1–5; `limit` 1–80 |
| Saídas | `levels[]`, `affected_files[]`, `tests[]`, `risks[]`, `documentation[]` |
| Consulta/grava | lê `relations`, `target_sources`, `cmake_targets`, `tests`, `test_files`, `symbols`; valida caminho dentro da raiz |
| Algoritmo | BFS reverso por include + dependência de target, `MAX_VISIT=120`; `tests_for` combina fonte do teste, include reverso e target |
| Custo | `impact Scene.h` 15 ms / **286 statements** (126× `_target_files`); `trace` 42,5 ms / 440 statements |
| Limitações | BFS não atravessa targets intermediários (para em target sem fontes); `tests_for` não sobe a cadeia target→testes; `trace` duplica `locate` |
| Testes | `test_impact_maps_consumers` (fixture com 1 nível), `test_locate_symbol_text_and_tests` (`tests-for` em header) |

### 4.8 `atlas_lib/diagnostics.py` (347 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | `status`, `doctor`, `verify`, detecção de ferramentas |
| API pública | `status`, `doctor`, `verify` (+ `_quick_stale`, `_tools`, `_fts5_available`) |
| Entradas | raiz, `strict` |
| Saídas | dicts com `ok`, `counts`, `issues[]`, `checks[]`, `tools` |
| Consulta/grava | lê todas as tabelas; **`_quick_stale` chama `collect_files`** (walk completo) |
| Algoritmo | staleness por `size`+`mtime_ns`; `verify` recalcula SHA-256 de todos os arquivos e roda `integrity_check` + FTS integrity + smoke query |
| Custo | `status` 569–925 ms; `doctor` 1.214 ms; `verify` 1.312 ms (lê 443 arquivos) |
| Limitações | `doctor` compara `schema_version` com `INDEX_VERSION` e `verify` também, enquanto `db.index_is_compatible` usa `INDEX_SCHEMA_VERSION` (duas constantes); `doctor --strict` reprova achados de severidade baixa |
| Testes | `test_init_status_verify_and_json_contract`, `test_doctor_reports_only_nonfatal_fixture_issues`, `test_corrupt_index_recovers` |

### 4.9 `atlas_lib/report.py` (261 linhas)

| Campo | Valor |
| --- | --- |
| Responsabilidade | Inventário gerado, benchmark isolado, avaliação do ranking |
| API pública | `write_generated_docs`, `benchmark`, `evaluate` (+ `_copy_indexable`, `_eval_questions`) |
| Entradas | raiz, `iterations` 1–20 |
| Saídas | dict com tempos, bytes, RSS, taxas |
| Consulta/grava | lê o banco; `docs` grava `docs/atlas/generated/inventory.md`; `bench` copia arquivos indexáveis para `.atlas/atlas-bench-*` e apaga ao final |
| Algoritmo | copia → indexa do zero → altera 1 arquivo → mede update → altera 5 → mede → roda as 12 perguntas × N → mede RSS via `getrusage` |
| Custo | `bench --iterations 2` = 7.018 ms; `eval` = 558 ms |
| Limitações | `eval` só mede módulo; `bench` mede só o ranking de módulo e caracteres; `docs` regenera arquivo versionado (pode aparecer como modificação) |
| Testes | exercitado indiretamente; `bench`/`eval` não têm teste de regressão |

### 4.10 Módulos Python — resumo de custo e cobertura

| Arquivo | Linhas | Testes diretos | Papel no caminho crítico |
| --- | ---: | ---: | --- |
| `atlas.py` | 14 | (todos) | entrada |
| `cli.py` | 371 | 2 | contrato de saída |
| `config.py` | 290 | 2 | exclusões/linguagem/módulo |
| `db.py` | 247 | 2 | esquema e transação |
| `indexer.py` | 960 | 3 | indexação e parsers |
| `search.py` | 594 | 3 | localização/ranking |
| `graph.py` | 244 | 2 | impacto/testes |
| `diagnostics.py` | 347 | 3 | saúde do índice |
| `report.py` | 261 | 0 | docs/bench/eval |
| **Total** | **3.332** | **11** | |

---

## 5. CLI e comandos

Sintaxe confirmada com `--help`; tempos medidos em subprocesso isolado, cache de
página aquecido; "Δ saída" é o tamanho em caracteres do stdout.

| Comando | Sintaxe | Implementação | Resultado esperado | Exemplo real medido | Tempo | Δ saída | Altera arquivos |
| --- | --- | --- | --- | --- | ---: | ---: | --- |
| `init` | `init [--force]` | `index_repository(force=True)` | índice completo | fixture: 8 arquivos; real: 443 | 3.974 ms (cópia) | 544 | `.atlas/` |
| `update` | `update` | `index_repository(force=False)` | incremental por SHA-256 | `unchanged=443`, `bytes_read=4224782` | 928–966 ms (no-op) | 541 | `.atlas/` |
| `status` | `status` | `diagnostics.status` | contagens, tamanho, defasagem, ferramentas | `stale_estimate=4` antes do update | 569–925 ms | 2.420 | não |
| `locate` | `locate <termos…> [--limit 8] [--module M]` | `search.locate` | módulos/arquivos/símbolos/testes | S1 devolveu `game` em 1º (limite 8 → `scene`) | 116 ms (1º: 1.174 ms) | 23.934 (JSON) / 3.322 (texto) | não |
| `symbol` | `symbol <nome> [--limit 20]` | `search.symbol` | definições + referências textuais | `EditorCamera` → 2 defs, 4.513 chars | 76–97 ms | 4.513 | não |
| `text` | `text <termos…> [--limit 10]` | `search.text_search` | trechos FTS | `decal_flip_u` → 6 arquivos, docs primeiro | 62 ms | 4.131 | não |
| `impact` | `impact <arquivo\|símbolo> [--depth 2] [--limit 30]` | `graph.impact` | consumidores + testes + riscos | `Scene.h`: 80 afetados | 79–86 ms | 19.801 | não |
| `trace` | `trace <termos…> [--limit 6]` | `graph.trace` | roteiro a partir do locate | `mundo padrao` → 3 módulos | 106 ms | 7.709 | não |
| `tests-for` | `tests-for <arquivo>` | `graph.tests_for` | testes mapeados | `tests/x11_focus.cpp` → 20 testes; `Part.cpp` → 0 | 67–70 ms | 5.541 | não |
| `doctor` | `doctor [--strict]` | `diagnostics.doctor` | inconsistências por severidade | 1 média + 4 baixas; `--strict` rc=1 | 1.214 ms | 2.471 | não |
| `verify` | `verify` | `diagnostics.verify` | integridade, hashes, smoke | `ok=false` com 4 hashes defasados | 1.312 ms | 1.141 | não |
| `docs` | `docs` | `report.write_generated_docs` | regenera inventário | `changed=true` (estava defasado) | ~120 ms | 303 | **`docs/atlas/generated/inventory.md`** |
| `bench` | `bench [--iterations 5]` | `report.benchmark` | tempos/RSS/bytes | `initial=3.974 ms`, `q_avg=42,45 ms` | 7.018 ms (2 iterações) | 1.461 | `.atlas/atlas-bench-*` (temporário) |
| `eval` | `eval` | `report.evaluate` | top-1/top-3 por módulo | `9/12`, `12/12`, `12/12` | 558 ms | 7.015 | não |

### 5.1 Códigos de saída (verificados)

| Situação | Código | Evidência |
| --- | ---: | --- |
| sucesso | 0 | todos os comandos |
| `verify` com hash divergente | 1 | medido antes do `update` (4 arquivos) |
| `doctor --strict` com achados baixos | 1 | medido |
| `impact` sem alvo | 1 | `{"ok":false,"data":{"error":…}}` |
| caminho fora da raiz (`tests-for ../outside`) | 2 | `{"ok":false,"command":"tests-for","error":"caminho fora da raiz do projeto"}` |
| índice ausente (`locate` em raiz sem `.atlas`) | 2 | `{"ok":false,…,"error":"indice ausente em …; execute … init"}` |
| subcomando inválido | 2 | argparse |

`status` com índice defasado devolve **0** com `ok=false` — ou seja, um agente que
só olha o código de saída não percebe índice velho.

### 5.2 Redundância e utilidade

| Observação | Detalhe |
| --- | --- |
| `init` ≡ `update --force` | mesmo `index_repository`; `force` só apaga o banco antes |
| `trace` ≈ `locate` + 3 consultas | 440 statements; 7.709 caracteres repetindo `locate` |
| `text` ⊆ `symbol` | `symbol` já faz referências textuais via FTS |
| `verify` repete trabalho do `update` | ambos leem e hasheiam 443 arquivos |
| `status` paga o walk inteiro | 514–525 ms só para estimar defasagem |
| `eval` não mede arquivo | inútil para o modo de falha real |
| Comandos de manutenção | `locate`, `symbol`, `tests-for`, `impact` (quando corrigidos), `diagnose` (não existe) |

---

## 6. SQLite e indexação

### 6.1 Esquema e ocupação

| Tabela | Linhas | Papel |
| --- | ---: | --- |
| `meta` | 11 | `schema_version`, `atlas_version`, `index_version`, `updated_at`, `file_count`, `glossary_terms`, `target_count`, `test_count`, `counts` |
| `files` | 443 | caminho, módulo, área, linguagem, tamanho, `mtime_ns`, `sha256`, linhas, `indexed_at`, `is_external` |
| `symbols` | 4.443 | nome, qualificado, kind, linha, `end_line`, assinatura, origem, confiança, `is_definition` |
| `relations` | 1.320 | include confirmado (598) e inferido (722) |
| `cmake_targets` | 44 | targets de `CMakeLists.txt` |
| `target_sources` | 181 | 101 fontes + 80 dependências |
| `tests` | 64 | nome, comando, target, `source_path`, timeout, labels, origem |
| `test_files` | — | associação teste→arquivo (fonte declarada + fontes do target) |
| `external_deps` | 14 | `find_package`/`pkg_check_modules`/`FetchContent`/`third_party` |
| `file_fts` (FTS5) | 443 | `path`, `module`, `content` (`unicode61 remove_diacritics 2`) |

Índices: `files_module`, `files_path`, `symbols_name` (NOCASE), `symbols_qualified` (NOCASE), `symbols_file`, `relations_source`, `relations_target`, `target_sources_path`, `test_files_path`.

Tamanhos medidos: banco **9.965.568 B** (2.433 páginas × 4.096 B), conteúdo FTS **4.213.670 B** (97,8% do corpus indexado), WAL zerado no fim de cada `update` (`wal_checkpoint(TRUNCATE)`).

Distribuição por linguagem e por prefixo:

| Linguagem | Arquivos | Bytes | | Prefixo | Arquivos |
| --- | ---: | ---: | --- | --- | ---: |
| cpp | 165 | 2.384.129 | | `src/` | 127 |
| markdown | 142 | 834.411 | | `docs/` | 125 |
| python | 72 | 559.689 | | `tests/` | 54 |
| lua | 34 | 56.041 | | `assets/` | 51 |
| json | 12 | 227.323 | | `tools/` | 38 |
| css | 5 | 80.223 | | **`.turbo/`** | **21** |
| shell | 4 | 4.574 | | `.opencode/` | 14 |
| rml | 3 | 29.374 | | raiz | 5 |
| demais | 6 | — | | `prototype/` | 1 |

### 6.2 O que o índice reconhece

| Pergunta da auditoria | Resposta verificada |
| --- | --- |
| Indexa todos os arquivos relevantes? | **Não.** `.turbo/` entra (21 arquivos de log/script de agentes), `prototype/legacy_main.cpp` entra, `tests/fixtures/audio/null.conf` não (`.conf` sem regra), `compile_commands.json` é ignorado por nome |
| Reconhece arquivos novos não commitados? | **Sim.** `docs/relatorios/AUDITORIA_COMPLETA_MOTOR_3D_LINBLUX.md` (untracked) está indexado e aparece em S2/S3/S4/S5/S6/S8 |
| Reconhece alterações locais não salvas no Git? | **Sim.** O hash é do working tree, não do Git; `stale_estimate=4` antes do update correspondia exatamente aos 4 arquivos que `verify` acusou |
| Quanto custa atualizar? | 928–966 ms sem mudanças; 1.339 ms com 4 arquivos alterados; 3.974 ms do zero (cópia isolada) |
| Reindexa desnecessariamente? | **Reparseia não** (hash igual ⇒ `unchanged`), mas **lê 4,22 MB e recalcula 443 SHA-256 em toda execução**, reescreve `mtime_ns` de 443 linhas e reconstrói `cmake_targets`/`tests`/`target_sources`/`external_deps` |
| Quais arquivos consomem mais tempo? | Parse: `LuaRuntime.cpp` 124,8 ms (153 KB), `StudioFrontend.cpp` 88 ms, `studio_main.cpp` 83,6 ms, `EditorUI.cpp` 80,6 ms, `rml_ui_main.cpp` 60,4 ms; walk: `.turbo` 87 ms / 9.077 entradas |
| Existe gargalo de I/O? | **Sim**, em dois pontos: (a) walk de 9.610 entradas, 94% em `.turbo`; (b) leitura integral do corpus a cada update — 10,9 ms quente vs **7.132 ms** frio no HDD |
| Há consultas SQLite ineficientes? | **Sim**: `_test_hits` e `_symbol_hits` são N+1 por arquivo candidato (67 e 15 consultas em um `locate`); `_target_files` é repetido 126× em `impact` |
| Pode funcionar com cache desatualizado? | **Sim, silenciosamente.** Nenhuma consulta (`locate`, `symbol`, `impact`, `trace`, `tests-for`) verifica staleness |
| Detecta relações que não existem? | **Sim.** 722 includes `inferred` (55% das relações); exemplos: `RmlUi/Core.h`, `GL/gl.h`, `BulletDynamics/…` sem caminho no projeto |
| Ignora relações importantes? | **Sim.** Não há aresta para consumes de target intermediário, nem para testes que dependem de bibliotecas (ver 8.2) |

### 6.3 Algoritmo incremental (como está)

```text
index_repository(root, force)
  ├─ force? remove_index()
  ├─ open_db(create=True) + checagem de schema (se ≠, apaga e recria)
  ├─ collect_files(root)                → 443 arquivos, 5.932 ignorados
  ├─ snapshot_files(conn)               → path,size,mtime_ns,sha256,module,language
  └─ para CADA arquivo:
        stat() → read_text_file() (lê tudo) → sha256
        se sha igual ao anterior: unchanged (mas guarda mtime para UPDATE)
        senão: parse_text() → parsed[]
  └─ transação:
        DELETE de removidos + limpeza de órfãos em file_fts
        UPDATE mtime_ns dos inalterados (443 linhas)
        para cada alterado: UPDATE/INSERT files, DELETE+INSERT symbols, relations, file_fts
        DELETE ALL + reINSERT de cmake_targets/tests/external_deps a partir do disco
        re-resolução de includes ainda inferidos
        meta/contagens
  └─ PRAGMA optimize + wal_checkpoint(TRUNCATE)
```

O ponto de desperdício está na linha 758 de `indexer.py`: `read_text_file(path)`
antes de qualquer comparação de `mtime`.

### 6.4 Desempenho em disco lento (medido neste HDD)

| Operação | Cache quente | Páginas descartadas (`posix_fadvise(DONTNEED)`) | Fator |
| --- | ---: | ---: | ---: |
| ler + SHA-256 dos 443 arquivos (4,22 MB) | 10,9 ms | **7.132,7 ms** | 656× |
| `locate` (índice de 9,96 MB) | 42,8 ms | **2.071,4 ms** | 48× |

Consequência prática: em um agente recém-iniciado, o primeiro `update` e a primeira
consulta pagam o custo frio; em um HDD de 5400 rpm isso significa **~7 s para
atualizar** e **~2 s para localizar**, antes de qualquer trabalho útil. O desenho
atual (ler tudo para hashear) é o que torna esse número inevitável; com
`mtime`+`size` como filtro, o custo frio cairia para o subconjunto realmente
alterado (tipicamente 0–5 arquivos).

---

## 7. Ranking e localização

### 7.1 Fórmula implementada (`search.locate`, linhas 343–511)

```text
score(arquivo) = max(0, -bm25)                     # FTS5, pesos 2.5/1.5/1.0
               + 4.0 × (termos presentes no caminho)
               + min(18, glossário)                # +7 módulo, +3 prefixo de caminho
               + símbolos                          # exato 10, prefixo 6, contém 3.5; glossário 8/4
               + min(4.5, 1.5 × testes associados)
               + 5.0 (se o arquivo veio só do glossário)
               × peso_da_área                      # source 1.0, test .72, docs .55,
                                                   # asset .45, tool .35, script .55, …
```

Agrupamento: até 3 arquivos por módulo (1º integral, seguintes 0,5); módulos
ordenados por score; `confidence` = `alta` se score ≥ 20 e gap ≥ 4, `parcial` se
≥ 9, senão `baixa`.

### 7.2 Fatos medidos

| Fenômeno | Evidência |
| --- | --- |
| **Ranking muda com `--limit`** | S1 top-1 `scene/Scene.h` (limite 3/5/8) → `game/Runtime.h` (20/50); S4 `editor/EditHistory.h` (3/5) → `game/Runtime.cpp` (8) → `objects/Part.h` (20/50); S7 `game/Npc.cpp` (3–8) → `scripting/LuaRuntime.cpp` (20/50); S6 troca `.h`→`.cpp`; S3 troca `Rig.cpp`→`Rig.h`; S8 troca o doc de topo |
| **Inferido vence evidência textual** | S1 (limite 8): módulo `scene` 39,0 (inferido por "aparece") acima de `rendering/PlayerMenu.cpp` 31,61 |
| **Docs dominam consultas técnicas** | S5: `docs/EDITOR_PRECISAO.md` (15,68), `MEGA_UPDATE_3D_PROGRESSO.md`, `AUDITORIA_COMPLETA…` ocupam 1º–3º, com `ConstructionPlacement.cpp` em 10º; S8: 3 docs no topo |
| **Glossário cobre pouco do domínio** | 81 termos; ausentes: `slider`, `audio`, `gizmo`, `construcao`, `parede`, `piso`, `ancorada`, `timeout`, `explorador`; presentes e genéricos: `aparece`, `mundo`, `padrao`, `teste` |
| **`symbol` casa por substring** | `symbol Part` devolve `part` (função de script Python em `tests/direct_face_capture.py`) e `Part` (struct de `prototype/legacy_main.cpp`) antes do `loblox::Part` |
| **Custo por consulta** | 66 ms, 513 statements, 67 `_test_hits`, 15 `_module_symbols`, 156 leituras de `file_fts_docsize` (snippet) |

### 7.3 O que está bom

- Determinismo para o **mesmo** par (consulta, limite): `test_limits_are_deterministic` passa; ordenações têm desempate.
- `score_breakdown` por arquivo permite auditoria da decisão.
- `inferred=true` marca candidatos sem evidência textual (o problema é o peso, não a marcação).
- Normalização de acentos e plurais simples funciona (`camera` casa `câmera`).
- `--module` restringe corretamente (`locate audio --module rendering` → só `PlayerMenu.cpp`).

---

## 8. Parser C++ e grafo

### 8.1 O que o scanner acerta e o que falha

| Recurso | Situação | Evidência |
| --- | --- | --- |
| `class`/`struct`/`enum class` com herança | acerta | `CPP_CLASS_RE` (linha 56); `loblox::Part` como `class` |
| `interface` (virtual puro) | acerta (1 no projeto) | heurística `= 0;` no corpo |
| Métodos declarados no header | acerta e marca `method` | `loblox::EditorUI::drawExplorerNode` (`EditorUI.h:97`) |
| **Definições fora da classe** | **falha parcial** | `EditorUI::drawExplorerNode` (`EditorUI.cpp:1987`) fica sem `loblox::` e com `kind=function` |
| Namespaces aninhados/`a::b` | acerta | `CPP_NAMESPACE_RE` aceita `::` |
| Templates | acerta a classe, ignora a cláusula | linha `template<…>` é pulada |
| Sobrecargas | lista todas, sem distinguir assinatura | 3 `simulate` distintos (2 reais + `Simulate` de teste) |
| `operator==`/`operator[]` | **não indexa** (nome com símbolos não casa `name_re`) | não há ocorrência em `src/`, mas o padrão falha por construção |
| Lambdas e atribuições | corretamente ignoradas (`=` no prefixo) | `_extract_cpp_functions` linha 269 |
| Macros/`#define` | ignoradas | linhas iniciadas por `#` |
| Includes | 598 confirmados / 722 inferidos | `_resolve_include` (linha 531) |
| Chamadas entre funções | **não existem** | nenhuma tabela de call graph |
| Callbacks/eventos | **não existem** | idem |

Quantificação da falha de namespace: **645 de 1.330** definições fora de classe em
`src/*.cpp` (48,5%, em 42 arquivos) ficam sem o prefixo `loblox::`, enquanto o
header declara 2.130 símbolos com prefixo. Resultado: o mesmo símbolo aparece
duas vezes com qualificados diferentes e tipos diferentes — o que também infla
`symbols-homonymous` no `doctor`.

Causa exata, `indexer.py:336–339`:

```python
if "::" in name:
    qualified = name          # ignora namespace/owner já calculados
else:
    qualified = "::".join(parts + [name]) if parts else name
```

### 8.2 Grafo de build/testes

`parse_cmake` reconhece `add_library`, `add_executable`, `target_sources`,
`target_link_libraries`, `add_dependencies`, `add_test`, `set_tests_properties`,
`find_package`, `pkg_check_modules`, `FetchContent_Declare`. Medido: 25,2 ms,
44 targets, 64 testes, 101 fontes, 80 dependências.

Duas falhas comprovadas de direção:

1. **`tests_for` não sobe a cadeia** (`graph.py:68–92`). Ele consulta
   `tests WHERE target = <target do arquivo>`; para um `.cpp` de biblioteca o
   target é `linblux_engine`/`linblux_renderer`/`linblux_editing`, e nenhum teste
   tem esse `target`. Resultado: **0 testes** para `PlayerMenu.cpp`,
   `SceneRenderer.cpp`, `ConstructionPlacement.cpp`, `EditorUI.cpp`,
   `LuaRuntime.cpp`, `Part.cpp`. Os dados necessários existem
   (`linblux_engine` → dependentes → `linblux_*_tests`), mas a consulta não os usa.
2. **`impact` para no target intermediário** (`graph.py:105–195`). O BFS adiciona
   apenas *arquivos-fonte* à fronteira; um target sem fontes (ex.:
   `linblux_core`, `INTERFACE`) encerra o ramo. `impact
   src/editor/ConstructionPlacement.cpp` → `affected_count = 0`, apesar de
   `linblux_editing → linblux_core → linblux_studio`.

Efeito colateral: `tests_for tests/x11_focus.cpp` devolve **20 testes**, porque o
parser associa ao teste `sources[-1]` do target (`indexer.py`, resolução pós-parse),
e `linblux_rml_ui_tests` tem `tests/x11_focus.cpp` como última fonte. Já
`tests/engine_default_main.cpp` mapeia corretamente 1 teste
(`linblux_engine_default_lifecycle`).

### 8.3 Ferramentas disponíveis na máquina

| Ferramenta | Estado | Uso possível hoje |
| --- | --- | --- |
| `clangd` | ausente | — |
| `clang` / `clang++` | ausente | — |
| `clang-format` | ausente | — |
| `ctags` | ausente | — |
| `rg` | ausente | fallback é FTS5 |
| `cscope` | ausente | — |
| `sqlite3` (CLI) | ausente | módulo `sqlite3` do Python funciona |
| `compile_commands.json` | **existe** (`build-linblux/`, 210.316 B, **302 entradas**, todas com `-std=c++`) | insumo pronto para uma camada semântica opcional |

Observação de infraestrutura: `build-linblux/` e o `compile_commands.json` pertencem
a `root`; um agente comum não consegue regenerá-los. Qualquer camada semântica
precisa ser **opcional** e degradar para regex quando o arquivo não existir ou
estiver ilegível.

---

## 9. Diagnósticos existentes

| Capacidade | Existe? | Onde | Observação |
| --- | --- | --- | --- |
| Saúde do índice (existência, versão, integridade) | sim | `diagnostics.status`, `doctor`, `verify` | três comandos com sobreposição |
| Defasagem vs working tree | sim | `_quick_stale` | por `size`+`mtime_ns`; não detecta reescrita com mesmo tamanho |
| Hash real | sim | `verify` | custa 1.312 ms quente / ~8 s frio |
| Grafo CMake vazio, testes condicionais, includes não resolvidos, símbolos homônimos, contratos ausentes | sim | `doctor` (códigos `cmake-graph-empty`, `tests-conditional`, `includes-unresolved`, `symbols-homonymous`, `contracts-missing`, `agents-missing`, `glossary-missing`) | bons códigos para automação |
| Diagnóstico de **sintoma** (arquivo provável, hipótese, confiança, validação) | **não** | — | é o comando `diagnose` proposto; hoje o agente improvisa com `locate`+`trace` |
| Plano de correção | **não** | — | `fix-plan` proposto |
| Distinção "arquivo relacionado" vs "causa comprovada" | só textualmente | `limitations[]` | nenhum campo estruturado de hipótese |

Problemas de semântica de saída:

- `doctor --strict` reprova (`rc=1`) com 4 achados de severidade **baixa**
  (`external-target-sources`, `tests-conditional`, `includes-unresolved`,
  `symbols-homonymous`) — inutilizável como gate de CI em repositório saudável.
- `doctor` compara `schema_version` com `INDEX_VERSION` (`config.py:12`) e
  `verify` idem, mas `db.index_is_compatible` usa `INDEX_SCHEMA_VERSION`
  (`__init__.py:4`). Duas fontes de verdade para o mesmo número.
- `verify` retorna 1 quando apenas o conteúdo está defasado, o que é o estado
  normal durante o desenvolvimento; o agente precisa interpretar `checks[]`.

---

## 10. Benchmarks

### 10.1 Método

- Comandos: `atlas.py status --json`, `bench --iterations 2`, `eval --json`,
  além de um harness externo (subprocesso + `getrusage(RUSAGE_CHILDREN)`),
  porque `/usr/bin/time` não existe nesta máquina.
- Medições "frias": `posix_fadvise(POSIX_FADV_DONTNEED)` nos arquivos/banco do
  próprio processo (sem root, sem descartar cache do sistema). É uma aproximação
  conservadora de HDD frio: mantém cache de inode/diretório.
- Não foram executados builds, suítes gráficas nem o modo FULL do TURBO.

### 10.2 Índice e atualização

| Métrica | Valor medido | Observação |
| --- | ---: | --- |
| Índice completo em cópia isolada (`bench`) | 3.973,7 ms | 443 arquivos, 4.224.782 B |
| `update` sem alterações | 928–966 ms (443 arquivos) / 1.300–1.404 ms (444) | 100% `unchanged`, `bytes_read` 4.224.782 → 4.313.383 |
| `update` com 4 arquivos alterados | 1.338,5 ms | antes da reindexação dos WIP |
| `update` após adicionar este relatório (88 KB) | 3.785 ms | 1 arquivo novo + 1 reparse; mostra que a inserção no FTS de um único documento grande domina o custo quando há mudança |
| `bench` update de 1 arquivo | 501,4 ms | cópia isolada |
| `bench` update de 5 arquivos | 537,1 ms | confirma custo fixo, não proporcional |
| `collect_files` | 514–525 ms | 9.610 entradas percorridas |
| `collect_files` ignorando `.turbo` | **31,0 ms** | 43 ignorados em vez de 5.932 |
| `parse_cmake` | 25,2 ms | 44 targets / 64 testes |
| Parse de todos os arquivos | 1.351,5 ms | pior caso, não é o caminho do update |

### 10.3 Consultas

| Consulta | Tempo | Saída (chars) |
| --- | ---: | ---: |
| `--version` | 57–66 ms (1ª: 183 ms) | 20 |
| `status --json` | 569–925 ms | 2.420 |
| `locate` (12 perguntas do `eval`, média) | 42,45 ms (máx. 57,4) | — |
| `locate` sintoma S1 | 116 ms (1ª: 1.174 ms) | 23.934 |
| `symbol EditorCamera` | 76–97 ms | 4.513 |
| `impact src/scene/Scene.h` | 79–86 ms | 19.801 |
| `tests-for tests/x11_focus.cpp` | 67–70 ms | 5.541 |
| `text ExperienceManager` | 62 ms | 4.131 |
| `trace "mundo padrao"` | 106 ms | 7.709 |
| `doctor` | 1.214 ms | 2.471 |
| `verify` | 1.312 ms | 1.141 |
| `eval` | 558 ms | 7.015 |
| `locate` frio (fadvise no índice) | **2.071 ms** | — |

### 10.4 Memória e tamanho

| Métrica | Valor |
| --- | ---: |
| Pico de RSS (`bench`, `getrusage`) | 36.148 KiB |
| RSS por consulta | 19.148–24.104 KiB |
| Banco principal | 9.965.568 B |
| Banco de cópia do `bench` | 7.540.736 B |
| Conteúdo FTS | 4.213.670 B |

O ATLAS é leve em memória (≤ 40 MB) e esse é um ponto forte a preservar.

### 10.5 Comparação com a documentação

| Documento | Medição documentada (24/10) | Medição atual (30/10) |
| --- | ---: | ---: |
| `docs/atlas/benchmarks.md` — arquivos | 235 | **443** |
| símbolos | 2.505 | **4.443** |
| relações | 895 | **1.320** |
| targets / testes | 34 / 34 | **44 / 64** |
| índice completo | 1.750–2.438 ms | 3.973,7 ms |
| update 1 arquivo | 202–464 ms | 501,4 ms |
| consulta média | 30,0–37,5 ms | 42,45 ms |
| banco | ~4,1 MB | 9,97 MB |
| `eval` top-1 | 12/12 | **9/12** |

Os números antigos não são falsos; eles descrevem um repositório ~1,9× menor. O
`bench` atual continua medindo o que se propõe. O que envelheceu foram os valores,
`docs/atlas/generated/inventory.md` (ainda com 235 arquivos no HEAD) e a taxa de
12/12 do `eval`.

---

## 11. Avaliação de oito sintomas reais

Referência verificada no código (grep direto, com linha), separando **principal**
(onde o comportamento é implementado) de **relacionado**. A causa-raiz de cada bug
**não foi confirmada** — não houve reprodução —, apenas o arquivo que implementa o
comportamento citado.

| ID | Sintoma | Evidência da referência (principal) | Relacionados |
| --- | --- | --- | --- |
| S1 | Slider de áudio do menu ESC aparece vazio | `src/rendering/PlayerMenu.cpp:44,68,72` (`slider("Volume geral",…)`, "Áudio indisponível nesta sessão") | `src/ui/StudioFrontend.cpp:320` (`player-volume`), `tests/player_menu.py` |
| S2 | Mover uma Part abre o gizmo antigo | `src/editor/Gizmo.{h,cpp}`, `src/editor/TransformSession.h` | `src/editor/EditorUI.cpp`, `src/ui/EditorActions.cpp` |
| S3 | Decal com texto aparece espelhado | `src/rendering/SceneRenderer.cpp:715,735,834,861,901` (`flipU`, `decal_flip_u`, comentário sobre espelhamento) | `tests/decal_render_main.cpp`, `src/objects/Part.h` |
| S4 | Part dinâmica cai durante edição | `src/app/Application.cpp:199` (`if (!ui_ \|\| (!ui_->playing() && !ui_->experienceOverlay())) physics_.simulate(…)`) | `src/physics/PhysicsWorld.cpp:438` (`simulate`) |
| S5 | Construção por superfície não encaixa parede no piso | `src/editor/ConstructionPlacement.cpp:31–48` (`SnapUnderCursor`), `src/editor/GizmoMath.cpp:113` | `src/editor/ConstructionUI.cpp` |
| S6 | Teste de Mundo Padrão expira por timeout | `tests/engine_default_main.cpp` (fonte de `linblux_engine_default_lifecycle`, `CMakeLists.txt:436`), `CMakeLists.txt` (labels/timeout) | `src/experiences/ExperienceManager.cpp` (`saveEngineDefaults`) |
| S7 | Script Lua de NPC falha porque a Part está ancorada | `src/scripting/LuaRuntime.cpp`, `src/objects/Part.cpp:194` (`setAnchored`) | `src/game/Npc.cpp`, `src/objects/Part.h:119` |
| S8 | Explorador cria nomes automáticos confusos | `src/editor/EditorUI.cpp:1987,2715` (`drawExplorerNode`, `renameSelection`), `src/objects/Instance.cpp:89` (`setName`) | `src/scene/Scene.cpp`, `src/ui/StudioFrontend.cpp` |

### 11.1 Resultado por sintoma (`locate`, limite padrão 8)

| ID | Módulo previsto (rank) | 1º arquivo | Rank do principal | Rank contando relacionados | Veredito |
| --- | --- | --- | ---: | ---: | --- |
| S1 | `game` (1), `rendering` (3) | `src/game/Runtime.h` | 4 (`PlayerMenu.cpp`) | 2 (`StudioFrontend.cpp`) | parcial: módulo certo em 3º, arquivo principal em 4º |
| S2 | `editor` (1) | `src/editor/Gizmo.h` | **1** | 1 | correto |
| S3 | `game` (1), `rendering` (3) | `src/game/Rig.cpp` | 5 (`SceneRenderer.cpp`) | 5 | módulo certo, arquivo errado |
| S4 | **nenhum dos esperados** | `src/editor/EditHistory.h` | — | — | **falha total**: `app` e `physics` ausentes do top-8 |
| S5 | `docs` (1) | `docs/EDITOR_PRECISAO.md` | 10 (`ConstructionPlacement.cpp`) | 10 (`GizmoMath.cpp` ausente) | docs dominam; código real em 10º |
| S6 | `experiences` (1), `tests` (3) | `ExperienceManager.h` | 4 (`engine_default_main.cpp`) | 1 | aceitável |
| S7 | `game` (1), `scripting` (2) | `src/game/Npc.cpp` | 4 (`LuaRuntime.cpp`) | 1 (`Npc.cpp`) | parcial |
| S8 | `docs` (1), `editor` (8) | `docs/relatorios/36-contornos-selecao-nomes.md` | — | 14 (`StudioFrontend.cpp`) | **falha**: `EditorUI.cpp` e `Instance.cpp` ausentes |

### 11.2 Métricas agregadas

| Métrica | `--limit 8` (padrão) | `--limit 50` |
| --- | ---: | ---: |
| Módulo Recall@1 | 1/8 | 2/8 |
| Módulo Recall@3 | 5/8 | 6/8 |
| Módulo Recall@5 | 6/8 | 7/8 |
| Módulo MRR | 0,359 | 0,484 |
| Arquivo principal Recall@1 | 1/8 | 2/8 |
| Arquivo principal Recall@3 | 1/8 | 2/8 |
| Arquivo principal Recall@5 | 5/8 | 5/8 |
| Arquivo principal Recall@10 | 6/8 | 7/8 |
| Arquivo principal MRR | 0,256 | 0,362 |
| Arquivo (principal+relacionado) Recall@3 | 4/8 | 3/8 |
| Arquivo (principal+relacionado) MRR | 0,475 | 0,471 |

Comparação com o `eval` do próprio ATLAS (12 perguntas, só módulo): 9/12 top-1,
12/12 top-3, 12/12 símbolo. **As duas medições não se contradizem**: o `eval`
pergunta no vocabulário do glossário e aceita acertar o módulo; os oito sintomas
usam linguagem de usuário e exigem o arquivo. O `eval` está medindo a coisa errada
para o objetivo do ATLAS 2.0.

### 11.3 Falsos positivos recorrentes

| Fonte | Exemplo em resultado |
| --- | --- |
| `.turbo/logs/**` | `.turbo/logs/direct-face/gizmo.py` (S2), `.turbo/home-layout-baseline/tests/home_video_main.cpp` (S4), `.turbo/logs/visual-correction/shadow-filter.py` (S8) |
| `prototype/legacy_main.cpp` | aparece em `symbol Part` (struct `Part`) |
| docs de auditoria/histórico | S2, S3, S4, S5, S6, S8 — inclusive o relatório untracked desta própria auditoria |
| `tests/*.py` de captura | `tests/direct_face_capture.py`, `tests/precision_tools_capture.py` (S5) |
| scripts de apoio | `tools/examples/build_rifledemo.py` em `text decal_flip_u` |

### 11.4 Precisão de `tests-for` e `impact` nos arquivos de referência

| Arquivo | `tests-for` | `impact.affected_count` | `impact.tests` |
| --- | ---: | ---: | ---: |
| `src/rendering/PlayerMenu.cpp` | **0** | — | — |
| `src/rendering/SceneRenderer.cpp` | **0** | 14 | 0 |
| `src/editor/ConstructionPlacement.cpp` | **0** | **0** | 0 |
| `src/editor/EditorUI.cpp` | **0** | — | — |
| `src/scripting/LuaRuntime.cpp` | **0** | 37 | 0 |
| `src/objects/Part.cpp` | **0** | 37 | 0 |
| `src/scene/Scene.h` (header) | vários via include reverso | 80 | 12+ |
| `tests/engine_default_main.cpp` | 1 (correto) | — | — |
| `tests/x11_focus.cpp` | **20** (superconjunto) | — | — |

Conclusão: a sugestão de testes só funciona em headers e no próprio arquivo de
teste. Para 80% dos arquivos que um agente precisa editar, `tests-for` responde
vazio e o agente volta a varrer o CMake à mão.

---

## 12. Integração com agentes

### 12.1 O que existe de fato no repositório

| Ativo | Conteúdo | Tamanho |
| --- | --- | ---: |
| `AGENTS.md` | orientação universal: começar pelo ATLAS, comandos, build/testes, segurança, contexto progressivo | 6.407 B (≈1,6k tokens por sessão) |
| `.opencode/skills/` | `atlas-navigate`, `atlas-debug`, `atlas-impact`, `atlas-extend`, `turbo` | 11.577 B |
| `.opencode/command/` | `localizar`, `diagnosticar`, `impacto`, `validar`, `teste-rapido`, `teste-impacto`, `teste-completo`, `organizar`, `otimizar-testes` | 9.056 B |
| `docs/atlas/README.md` | comandos, camadas, limites, segurança | 5.637 B |
| `docs/atlas/architecture.md` | desenho interno, ranking, grafo | 7.831 B |
| `docs/atlas/json.md` | contrato JSON + códigos de saída | 6.866 B |
| `docs/atlas/workflows/` | navegar, depurar, impacto, validar | 7.477 B |
| `docs/atlas/module-contracts/` | 13 contratos manuais por módulo | — |
| `schemas/output.schema.json` | envelope mínimo | 350 B (não validado) |

Leitura completa da documentação do ATLAS custa ~20 KB (≈5k tokens); só o
`AGENTS.md` custa ~1,6k tokens. Um agente disciplinado gasta isso uma vez; um
explorador gasta por tarefa.

### 12.2 Verificação por agente

| Agente | Descoberta automática do ATLAS | Estado |
| --- | --- | --- |
| OpenCode | skills e comandos em `.opencode/` | **parcialmente verificado** (arquivos existem e são carregáveis; nenhuma execução de skill foi possível nesta sessão) |
| ferramenta de terminal | apenas `AGENTS.md` | **NÃO VERIFICADA** — não há `.desenvolvimento/` no repositório (`.gitignore` lista `/.desenvolvimento/`) |
| desenvolvimento | apenas `AGENTS.md` | **NÃO VERIFICADA** — `.desenvolvimento/` existe com 1 arquivo, sem integração de ATLAS |
| ferramenta de terminal | nada específico | **NÃO VERIFICADA** |
| Qualquer agente com shell | CLI + `--json` | **verificado** (todos os comandos rodam sem rede, sem servidor, sem modelo) |

Não existe MCP, LSP, daemon ou API HTTP. A interface universal real é: processo
local + stdout + código de saída.

### 12.3 Respostas objetivas

| Pergunta | Resposta |
| --- | --- |
| Todos os agentes descobrem o ATLAS automaticamente? | **Não.** Só quem lê `AGENTS.md` ou carrega `.opencode/`. |
| Precisam gastar muitos tokens lendo a documentação? | Sim: ~6,4 KB obrigatórios + ~20 KB se ler `docs/atlas/`. Não há um "cartão de referência" de 10 linhas. |
| As respostas são extensas demais? | Sim: `locate` 23.934 chars, `impact` 19.801, `trace` 7.709 (JSON, limite padrão). |
| A consulta mostra linhas relevantes? | **Parcialmente.** `locate` mostra `path` + `símbolo:linha` + trecho FTS de 18 tokens; não mostra a linha do código que casou. `symbol` mostra `path:line` das definições e o texto da referência. |
| O desenvolvedor ainda precisa dezenas de grep depois? | Sim, quando o arquivo correto não está no top-3 (5/8 dos sintomas) ou quando `tests-for` volta vazio (6/6 arquivos de `src/` testados). |
| O ATLAS sugere os testes corretos? | **Não** para fontes de biblioteca (0/6); sim para headers e arquivos de teste. |
| Existe forma curta de pedir "encontre este bug"? | Não. O mais próximo é `locate "$ARGUMENTS" --limit 8` + ler JSON grande. |
| Indica rapidamente quais arquivos editar? | Indica candidatos; a taxa de acerto no arquivo principal é 1/8 em @1. |
| Há integração que só funciona em um agente? | As skills/comandos são só OpenCode. `AGENTS.md`, CLI e JSON são universais. |
| A saída JSON é estável e curta? | Estável no envelope (`ok`,`command`,`data`) e determinística para o mesmo limite; **não é curta** e o envelope de erro é inconsistente em `impact`. |

---

## 13. Integração com TURBO

### 13.1 Como o TURBO usa o ATLAS

O TURBO **não chama a CLI de consulta**. Ele lê o banco diretamente em modo
somente-leitura (`file:…?mode=ro`) e monta o inventário:

```text
turbo test/build/inventory/doctor
   └─ inventory.load_inventory()
        ├─ ctest --show-only=json-v1        (testes registrados, labels, comando)
        ├─ CTestCostData.txt                (duração histórica)
        ├─ inventory.ensure_atlas()
        │    ├─ atlas.py status --json      (0,57 s)
        │    └─ atlas.py update             (0,93 s, só se stale/incompatível)
        └─ atlas_snapshot()  → lê tests, cmake_targets, target_sources,
                               test_files, files, relations
   └─ selection.build_plan(inventory, changes, mode)
```

Consumo confirmado (referências): `inventory.py:76–141` (`ensure_atlas`,
`atlas_snapshot`), `inventory.py:354–390` (`load_inventory`), `selection.py:1–420`
(seleção e razões), `cli.py:194–202` (`_inventory`), `cli.py:755,813`
(`atlas_info` no relatório/doctor).

### 13.2 Como o TURBO escolhe os testes

Ordem de evidência (`selection.py`):

1. arquivo do teste ou fonte direta (`test_files`/`source_path`);
2. target de teste direto que contém o arquivo alterado;
3. harness por nome (`linblux_<stem>`);
4. consumidor de target (fecho de dependências);
5. módulo afetado (dicas fixas em `config.MODULE_TEST_HINTS`);
6. regras de assets;
7. linha de base headless quando nada casou (marcada como cobertura desconhecida).

O ATLAS entra nos passos 1, 2 e 4 via tabelas. O ATLAS **não** entra nos passos
5–7, e o `tests_for`/`impact` da CLI não é usado: `selection.py` reimplementa o
grafo (`_reverse_includes`, `_targets_for_paths`, `_test_target_closure`).

### 13.3 Medições reais

| Operação | Tempo | Saída |
| --- | ---: | ---: |
| `turbo.py inventory --json` | 4.454 ms | **103.718 caracteres** |
| `turbo.py test --mode fast --changed --plan` | 745 ms | 3.865 caracteres |

Plano observado com o WIP atual (26 arquivos alterados, inclui `CMakeLists.txt`):
19 testes selecionados, todos `required`, **cobertura `unknown`**, 8 caminhos sem
mapeamento seguro entre eles `src/scene/Scene.h`, `src/rendering/SceneRenderer.h`,
`src/editor/EditorUI.h`, `src/rendering/PlayerMenu.h`, `tests/player_menu.py`,
`tests/fixtures/audio/null.conf`, `CMakeLists.txt`.

### 13.4 Diagnóstico da integração

| Pergunta | Resposta |
| --- | --- |
| O ATLAS ajuda a escolher testes? | Ajuda no inventário (nomes, targets, fontes, dependências), que é justamente onde o CTest não dá. |
| Testes irrelevantes sendo executados? | **Sim.** Mudança em `CMakeLists.txt` + headers amplamente incluídos gera 19 "obrigatórios" (~todo o conjunto headless relevante); headers aparecem como `Sem mapeamento seguro` apesar de estarem no índice. |
| Testes importantes ignorados? | Risco real: `unknown_paths` e a regra de linha de base significam cobertura `unknown`; o TURBO avisa, mas não bloqueia. |
| Recompilações desnecessárias? | `build_targets` inclui todos os alvos afetados + de teste; para headers amplos isso converge para quase tudo. Não medido nesta auditoria (sem build). |
| O desenvolvedor lê logs grandes demais? | `format_result` trunca em 1.200 caracteres; `runner.excerpt` corta em `MAX_EXCERPT_CHARS`, mantendo **as 8 últimas** linhas que casam com `error|failed|exception|assert|timeout|…`. |
| Resume o primeiro erro relevante? | **Não exatamente**: pega as últimas 8 linhas de erro, o que em cascata de compilação costuma ser a cauda, não a causa. |
| Distingue timeout, compilação, falha de teste e ausência de cobertura? | Sim, nos estados: `TIMEOUT`, `BUILD_FAILED`/`CONFIG_FAILED`, `FAILED`, `UNKNOWN`/`NO_TESTS`/`BUDGET_EXHAUSTED`/`PASSED_PARTIAL`/`REUSED`. |
| Aponta arquivo e linha a partir de logs de compilação? | **Não.** Nenhuma extração de `arquivo:linha:coluna: erro:`; o `excerpt` devolve texto bruto. |

### 13.5 Problemas de contrato entre ATLAS e TURBO

1. O TURBO depende de **tabelas internas** (`test_files`, `target_sources`,
   `cmake_targets`, `relations`), não do contrato JSON documentado em
   `docs/atlas/json.md`. Qualquer mudança de esquema quebra o TURBO sem aviso;
   `INDEX_SCHEMA_VERSION` não é checado pelo TURBO (ele checa `compatible` do
   `status`, o que cobre o caso, mas não versiona o contrato de leitura).
2. `ensure_atlas` decide atualizar por `stale_estimate` (tamanho/mtime). Arquivo
   reescrito com mesmo tamanho e mtime preservado (comum em `touch -r`/checkout)
   não dispara update.
3. Duas implementações do mesmo grafo (CLI e TURBO) divergem: `tests_for` diz 0
   teste para `Part.cpp`; a seleção do TURBO encontra `linblux_tests` etc. via
   `source_by_target`/fecho. O agente recebe respostas diferentes para a mesma
   pergunta dependendo de qual ferramenta usa.
4. `tests-conditional` (44 de 62 no índice antigo) faz o TURBO listar testes como
   "não registrados neste build"; a heurística de `if/endif` do parser de CMake
   não avalia condições.

---

## 14. Economia de tokens

Medição por **caracteres** (não são tokens de um modelo específico; aproximação
declara). `--json` inclui indentação de 2 espaços.

| Comando | JSON (chars) | Texto (chars) | Razão |
| --- | ---: | ---: | ---: |
| `locate` (sintoma) | 23.934 | 3.322 | 7,2× |
| `impact Scene.h` | 19.801 | — | — |
| `trace` | 7.709 | — | — |
| `eval` | 7.015 | — | — |
| `tests-for x11_focus.cpp` | 5.541 | — | — |
| `symbol` | 4.513 | — | — |
| `text` | 4.131 | — | — |
| `status` | 2.420 | — | — |
| `doctor` | 2.471 | — | — |
| `verify` | 1.141 | — | — |
| `turbo inventory` | 103.718 | — | — |

O `token_economy` do próprio `bench` reporta razão 0,0526 (8.243 chars de saída
contra 156.843 chars dos arquivos candidatos), o que é verdadeiro e enganoso: a
pergunta não é "é menor que ler os arquivos?", é "cabe confortavelmente no
contexto e resolve o problema?". 24 KB por `locate` + 20 KB por `impact` + 5 KB de
`tests-for` já são ~15k tokens para uma única investigação, e o resultado ainda
erra o arquivo em 7/8 dos casos.

### 14.1 Onde o contexto é desperdiçado

| Desperdício | Evidência |
| --- | --- |
| Ler arquivo inteiro em vez de trecho | o agente recebe `path` e precisa abrir o arquivo; `locate` não devolve a linha que casou (só um snippet FTS de 18 tokens) |
| Busca repetitiva | `AGENTS.md` manda `locate` → `trace` → `symbol` → `impact` → `tests-for`; `trace` repete `locate` (440 statements, 7,7 KB) |
| Logs grandes | `turbo inventory` = 103 KB; `runner.excerpt` só resume quando excede o limite |
| Saída excessiva do ATLAS | 8 módulos × 3 arquivos × evidências em toda consulta, sem opção `--brief` |
| Repetição de consultas | `eval` roda 12 consultas; cada agente costuma rodar 3–5 consultas por tarefa |
| Diagnósticos sem evidência | `next[]` e `limitations[]` repetem strings fixas em toda resposta |
| Comandos sem limite | `impact` e `trace` não aceitam `--brief`/`--max-chars` |
| Busca duplicada entre agentes | cada agente reindexa/consulta; não há cache de consulta |

### 14.2 Proposta de saída em camadas

| Modo | Conteúdo | Alvo de tamanho |
| --- | --- | --- |
| `--brief` (padrão para automação) | até 5 arquivos `path:linha`, 1 símbolo, 1–3 testes, confiança | ≤ 1.200 chars |
| `--detail` | relações, trechos numerados (3–8 linhas por arquivo), `score_breakdown` | ≤ 6.000 chars |
| `--diagnose` | hipóteses ranqueadas + evidência + validação sugerida | ≤ 3.000 chars |
| `--json` | envelope completo (atual), com `--fields` para podar | sem limite (opt-in) |

Limites configuráveis: `--max-files`, `--max-chars`, `--max-tests`, e um
`atlas.toml`/`.atlas/config.json` com padrões por consumidor.

---

## 15. Desempenho em hardware fraco

O ambiente da auditoria **é** o cenário alvo: 8 threads, 5,8 GiB RAM (com 4,1 GiB
em cache), HDD 5400 rpm. Nada aqui exige GPU, rede ou modelo local.

| Recomendação | Custo de hardware | Ganho medido/estimado |
| --- | --- | --- |
| Ignorar `.turbo/` (e logs) no walk | **zero** | `collect_files` 525 → 31 ms (17×) |
| Filtro `mtime`+`size` antes do hash | zero | `update` frio 7.132 → ~0 ms quando nada mudou; quente 928 → ~400 ms |
| Guardar só `content` no FTS quando necessário; hoje 4,21 MB de 9,96 MB | zero | consulta fria 2.071 ms proporcional ao banco lido |
| Eliminar N+1 de `_test_hits`/`_symbol_hits` | zero | `locate` 513 → <30 statements |
| Cache de `glossary.json`/contratos por processo | zero | ~0,3 ms por consulta (20 KB relidos) |
| `PRAGMA mmap_size` + evitar `wal_checkpoint(TRUNCATE)` em update no-op | zero | menos escrita em HDD |
| `atlas serve` (daemon local) | baixo (RAM ~25 MB) | remove 60 ms de startup e reabertura por consulta — **opcional**, não obrigatório |
| Camada semântica com `clangd`/`libclang` | **alto** (LLVM ~1 GB, tempo de parse) | precisa ser opt-in e nunca requisito |
| Índice vetorial / embeddings locais | alto (modelo + RAM + CPU) | não recomendado para o objetivo "leve e offline" |

Conclusão: as melhorias de maior impacto (P1–P6) são **todas leves** — nenhuma
exige mais hardware. O que exige hardware é justamente o que não deve ser
obrigatório (AST completa, embeddings, daemon).

---

## 16. Bugs e limitações comprovados

Cada item tem evidência reproduzível nesta sessão.

| # | Severidade | Bug | Evidência | Arquivo:linha |
| --- | --- | --- | --- | --- |
| B1 | alta | `.turbo/` não está em `IGNORED_DIRS`: 9.077 arquivos percorridos, 21 indexados com módulo `root` e presença em resultados | `collect_files` 525 ms → 31 ms ao excluir; `skipped` 5.932 → 43; S2/S4/S8 | `config.py:61` |
| B2 | alta | `update` lê e hasheia os 443 arquivos sempre, mesmo sem mudanças | `bytes_read=4224782` com `unchanged=443`; 10,9 ms quente vs 7.132,7 ms frio | `indexer.py:758` |
| B3 | alta | Ranking depende de `--limit` (resultado muda de top-1) | 6/8 sintomas trocam de top-1 entre 8 e 50 | `search.py:276` (`max(limit*8,40)`) + agrupamento `search.py:441–470` |
| B4 | alta | Candidato inferido por glossário supera evidência textual | S1: `scene` 39,0 (inferido por "aparece") > `PlayerMenu.cpp` 31,61 | `search.py:79–96,234–262,430` |
| B5 | alta | `tests_for` devolve 0 para fontes de biblioteca (direção do grafo) | 6/6 arquivos de `src/` testados → 0 | `graph.py:68–92` |
| B6 | alta | `impact` para em target intermediário | `ConstructionPlacement.cpp` → `affected_count=0` | `graph.py:105–195` |
| B7 | média | 645/1.330 definições fora de classe perdem namespace e viram `function` | `Application::initialize` vs `loblox::Application::initialize` | `indexer.py:336–339` |
| B8 | média | Glossário cobre 81 termos e falta vocabulário de 6 dos 8 sintomas | `slider`, `audio`, `gizmo`, `construcao`, `parede`, `piso`, `ancorada`, `timeout`, `explorador` ausentes | `data/glossary.json` |
| B9 | média | Docs dominam consultas técnicas (peso de área não compensa) | S5: 3 docs à frente do código; S8: 3 docs no topo | `search.py:46–56`, `_candidate_rows` |
| B10 | média | Nenhuma consulta avisa índice desatualizado | `locate` respondendo com 4 arquivos defasados, sem aviso | `cli.py` (todos os subcomandos de leitura) |
| B11 | média | `doctor --strict` reprova repositório saudável | `rc=1` com `alta=0` e 4 achados `baixa` | `diagnostics.py:262–265` |
| B12 | média | Duas constantes de versão de esquema | `INDEX_VERSION` (config) vs `INDEX_SCHEMA_VERSION` (`__init__`) usadas em pontos diferentes | `config.py:12`, `__init__.py:4`, `db.py:169`, `diagnostics.py:144` |
| B13 | média | `docs/atlas/generated/inventory.md` versionado estava defasado (235 arquivos vs 443) | `git diff` após regenerar; valores do HEAD | `docs/atlas/generated/inventory.md` |
| B14 | média | Envelope JSON inconsistente: `impact` põe `error` dentro de `data` | `{"ok":false,"data":{"error":…}}` sem `payload["error"]` | `graph.py:118–125` |
| B15 | baixa | `symbol` casa por substring case-insensitive e lê arquivos do disco para referências | `symbol Part` → `part` de `tests/direct_face_capture.py`, struct de `prototype/` | `search.py:513–571` |
| B16 | baixa | 20 testes para `tests/x11_focus.cpp` (superconjunto) | parser escolhe `sources[-1]` do target | `indexer.py:695–706` |
| B17 | baixa | `meta.glossary_terms` gravado como 1 | `len(glossary(root))` conta a chave `terms` | `indexer.py:931` |
| B18 | baixa | `DELETE` de FTS para arquivo removido é no-op | subconsulta após apagar a linha | `indexer.py:789–793` |
| B19 | baixa | Reescrita integral de `cmake_targets`/`tests`/`target_sources`/`external_deps` a cada update | `parse_cmake` 25 ms + 181+ DELETE/INSERT | `indexer.py:837–905` |
| B20 | baixa | `eval` mede só módulo, com perguntas no vocabulário do glossário | 9/12 hoje vs 12/12 documentado; 12/12 top-3 | `report.py:194–260` |
| B21 | baixa | `schemas/output.schema.json` não é validado por código nem teste | nenhuma referência no repositório | `schemas/output.schema.json` |
| B22 | baixa | `trace` duplica `locate` e custa 440 statements | medição | `graph.py:197–236` |
| B23 | baixa | `status` paga o walk completo (514 ms) só para estimar defasagem | medição | `diagnostics.py:25–44` |
| B24 | baixa | `benchmarks.md` e `eval` desatualizados frente ao repositório | 235 vs 443 arquivos; 12/12 vs 9/12 | `docs/atlas/benchmarks.md` |

### 16.1 Limitações estruturais (não são bugs)

- Sem AST: não há call graph, resolução de sobrecarga, macros ou templates.
- Sem stemming linguístico: só plural PT simples.
- Sem sinônimos além do glossário manual.
- Sem avaliação semântica de condições CMake.
- Sem paralelismo na indexação (single-thread); o custo é dominado por I/O e walk.
- `INDEX_VERSION` incompatível ⇒ `remove_index` + reindexação total (sem migração).
- Resíduo de teste pré-existente: `.atlas/atlas-test-2qtu9bcx/` (dono `root`, 28/10) ficou
  no cache — `tearDown` não conseguiu remover o que o teste criou como root. Não foi
  removido nesta auditoria (não pertence a ela e `rm -rf` está fora de escopo);
  é um sintoma de que `tests/atlas` não tolera execução com UID diferente do dono
  do cache.

---

## 17. Melhorias recomendadas

### 17.1 Comparação ATLAS atual × ATLAS 2.0 desejado

| Capacidade | Atual | Problema comprovado | Melhoria proposta | Risco |
| --- | --- | --- | --- | --- |
| Localização | `locate` FTS+heurística, agrupado por módulo | R@1 de arquivo 1/8; top-1 muda com `--limit` | ranking por arquivo com pool fixo; `--limit` só corta; devolver `path:linha` do match | mudar ordem quebra testes/contratos de saída |
| Precisão | glossário manual (81 termos) com peso alto | inferido vence evidência textual; vocabulário ausente | peso de inferido nunca acima de match textual; sinônimos derivados do código (símbolos + comentários) | falso positivo se sinônimo for amplo |
| Indexação | lê/hasheia tudo; walk inclui `.turbo` | 7,1 s frio; 525 ms de walk | filtro `mtime`+`size`; ignorar `.turbo`/logs/protótipo; índice de coluna | mtime mentiroso (mitigar com hash amostral) |
| Análise C++ | regex, `medium` | 645/1.330 sem namespace; sem call graph | corrigir namespace/kind; camada opcional `compile_commands.json` + `clangd` | LLVM pesado — deve ser opt-in |
| Diagnóstico | `locate`+`trace` manuais | sem hipótese/confiança/validação | `diagnose` e `fix-plan` com evidência estruturada | hipótese errada parecer conclusão |
| Impacto | BFS por include/target | 0 afetados para `linblux_editing`; 80 para `Scene.h` | BFS por grafo de targets normalizado + poda por profundidade | explosão de consumidores em headers |
| Seleção de testes | `tests_for` + heurística do TURBO | 0 teste para fontes de lib; 20 para `x11_focus` | fonte única (ATLAS) consumida pelo TURBO; `tests-for` sobe a cadeia de targets | divergência CLI×TURBO durante transição |
| Integração com automação | CLI+JSON+AGENTS.md | saída grande, erro inconsistente, sem `--brief` | contrato JSON versionado + camadas + schema validado em teste | quebra de consumidores antigos |
| Economia de tokens | 24 KB por `locate` | 7× maior que o modo texto | `--brief` padrão para automação, `--detail` opt-in, `--max-chars` | perda de evidência se `brief` for agressivo |
| Memória | 36 MB de pico | ok | preservar (não introduzir daemon obrigatório) | daemon aumentaria RAM ociosa |
| HDD | leitura integral por update | 7,1 s frio | incremental por metadados; menos escrita (sem reescrever grafo CMake) | cache inconsistente se mtime for adulterado |
| Segurança | caminho validado, SQL parametrizado, sem rede | ok | manter; adicionar validação de schema e limites de saída | nenhum |
| Compatibilidade | Python 3.13 stdlib + SQLite | ok | manter; nada além da stdlib no núcleo | dependências externas opcionais |

### 17.2 Lista priorizada

| ID | Melhoria | Impacto | Custo | Depende de |
| --- | --- | --- | --- | --- |
| M1 | `".turbo"`, `"logs"` e `"prototype"` em `IGNORED_DIRS`; excluir `*.log`/`*.xml` de relatório | alto | trivial | — |
| M2 | `update` incremental: pular leitura quando `size` e `mtime_ns` iguais; hash só em dúvida | alto | baixo | — |
| M3 | `tests_for` corrigido: subir `target → dependentes → testes` com fecho transitivo | alto | baixo | — |
| M4 | `impact` com BFS no grafo de targets (nós = targets, arestas = link/dependência) | alto | baixo | M3 |
| M5 | Ranking estável: pool fixo independente de `--limit`; inferido com teto abaixo de match textual | alto | baixo | — |
| M6 | `--brief`/`--detail`/`--max-chars`/`--max-files`; `locate` breve com `path:linha` | alto | baixo | — |
| M7 | `atlas diagnose "sintoma"` (arquivos, linhas, hipóteses, confiança, testes, validação) | alto | médio | M3–M6 |
| M8 | Namespace/kind em definições fora de classe; deduplicar símbolo header/impl | médio | baixo | — |
| M9 | Glossário derivado: indexar identificadores e comentários para sugerir sinônimos; manter curadoria manual | médio | médio | M5 |
| M10 | Contrato JSON versionado + validação do schema no `linblux_atlas`; envelope de erro uniforme | médio | baixo | — |
| M11 | TURBO consome `atlas tests-for`/`impact` (fonte única) e log com `arquivo:linha` + primeiro erro | alto | médio | M3, M4 |
| M12 | Camada semântica opcional via `compile_commands.json` + `clangd`, com fallback regex | médio | alto | — |
| M13 | `eval` com ≥40 perguntas, referência de **arquivo**, Recall@1/3/5 e MRR | alto | baixo | M5 |
| M14 | `status`/`doctor` sem walk completo (usar `mtime` do diretório ou cache) | médio | baixo | M2 |

---

## 18. Arquitetura proposta para ATLAS 2.0

Princípios: offline, stdlib + SQLite, sem serviço obrigatório, índice
reconstruível, resposta curta por padrão, evidência sempre explícita.

### 18.1 Camadas

```text
[1] Fonte          arquivos do projeto (working tree, inclusive untracked)
[2] Coleta         walk com ignore por diretório (build*, .turbo, logs, .atlas, .git)
                   filtro rápido mtime+size; hash sob demanda
[3] Extração       parsers por linguagem (C++, Lua, Python, RML, Markdown, CMake)
                   + resolvedor de includes e de targets (grafo normalizado)
[4] Índice         SQLite: files, symbols, relations, targets, tests,
                   file_fts, match_lines (nova), metrics
[5] Consulta       locate (arquivo), symbol, text, impact, tests-for, trace
[6] Diagnóstico    diagnose, fix-plan (com hipóteses e validação)
[7] Entrega        brief (padrão) / detail / diagnose / json versionado
```

### 18.2 Mudanças de esquema propostas

| Nova estrutura | Para quê |
| --- | --- |
| `match_lines(file_id, line, token, weight)` | devolver `path:linha` sem FTS `snippet()` e permitir `brief` barato |
| `test_targets(test_id, target, confidence)` | fecho teste→target→biblioteca, corrigindo `tests_for` |
| `target_edges(source_target, target, kind)` | BFS de impacto em grafo de targets, não em arquivos |
| `symbol_refs(symbol_id, file_id, line)` | referências textuais materializadas, sem reler arquivos no `symbol` |
| `diagnoses(id, query, hypothesis, confidence, evidence_json)` | cache/auditoria de diagnósticos |
| `meta.contract_version` | contrato de leitura para o TURBO |

### 18.3 `atlas diagnose` (proposto)

```json
{
  "query": "uma part dinamica cai durante edicao",
  "files": [
    {"path": "src/app/Application.cpp", "line": 199, "score": 82.0,
     "why": ["símbolo PhysicsWorld::simulate", "condição playing()", "include de PhysicsWorld.h"]}
  ],
  "hypotheses": [
    {"text": "simulação roda quando não está em Play", "confidence": "média",
     "evidence": ["Application.cpp:199"], "verify": "abrir Application::run e checar ui_->playing()"}
  ],
  "tests": [{"name": "linblux_game", "reason": "target linblux_engine → linblux_game_tests"}],
  "validate": ["python3 tools/turbo/turbo.py test --mode fast --changed"],
  "next": "confirmar em Application.cpp:199 antes de editar",
  "chars": 1180
}
```

Reaproveitamento: `locate` (candidatos) + `trace` (fluxo) + `impact` (consumidores)
+ `tests-for` (validação) + glossário ampliado. Nada de novo algoritmo pesado — é
composição com ranking estável e saída curta.

### 18.4 `atlas fix-plan` (proposto)

Não edita nada. Devolve um plano de 3–6 passos com `path:linha`, o comando de
validação por passo e o critério de parada. Sem autorização explícita do usuário,
nenhuma escrita.

---

## 19. Plano de implementação

### Fase A — corrigir bugs e ranking atuais

| Item | Detalhe |
| --- | --- |
| Arquivos | `atlas_lib/config.py`, `atlas_lib/indexer.py`, `atlas_lib/search.py`, `atlas_lib/graph.py`, `atlas_lib/diagnostics.py` |
| APIs afetadas | nenhuma assinatura muda; semântica de `locate`, `tests-for`, `impact` |
| Testes | novo `test_limits_do_not_change_ranking`, `test_turbo_dir_is_ignored`, `test_tests_for_library_source`, `test_impact_across_interface_target`, `test_inferred_never_beats_textual` |
| Riscos | mudança de ordem quebra `eval` e expectativas de agentes; mitigar com `eval` novo antes de mudar pesos |
| Compatibilidade | `INDEX_SCHEMA_VERSION` 1→2 se `match_lines` entrar; `init --force` documentado |
| Ganho esperado | R@1 de arquivo ≥ 4/8; `tests-for` ≥ 1 em 6/6 fontes; `update` no-op ≤ 400 ms quente |
| Critério de aprovação | todos os testes acima verdes + `bench` sem regressão > 20% + `eval` novo ≥ baseline |

### Fase B — diagnóstico compacto

| Item | Detalhe |
| --- | --- |
| Arquivos | novo `atlas_lib/diagnose.py`; `cli.py`; `search.py` (match_lines) |
| APIs afetadas | novos subcomandos `diagnose`, `fix-plan`; `locate --brief` |
| Testes | `test_diagnose_returns_files_with_lines`, `test_brief_output_under_limit`, `test_diagnose_is_read_only` |
| Riscos | hipótese apresentada como fato; mitigar com campo `confidence` e `verify` obrigatórios |
| Compatibilidade | aditivo; nada removido |
| Ganho esperado | 1 comando em vez de 3–5; ≤ 1.200 chars por diagnóstico |
| Critério de aprovação | 8 sintomas com arquivo correto no top-3 em ≥ 5; saída ≤ 1.200 chars |

### Fase C — integração ATLAS + TURBO

| Item | Detalhe |
| --- | --- |
| Arquivos | `tools/turbo/turbo_lib/selection.py`, `inventory.py`, `runner.py`, `report.py` |
| APIs afetadas | seleção passa a consumir `tests-for`/`impact`; `excerpt` extrai `arquivo:linha` e o **primeiro** erro |
| Testes | `test_turbo_uses_atlas_tests_for`, `test_excerpt_first_error`, `test_plan_coverage_known_for_header` |
| Riscos | regressão na seleção pode pular testes; manter modo de comparação (`--explain-selection`) |
| Compatibilidade | TURBO continua lendo o banco se a CLI falhar (fallback) |
| Ganho esperado | plano com cobertura `known` em headers; menos testes irrelevantes; log apontando `path:line` |
| Critério de aprovação | mesmo conjunto de testes para a mesma mudança nas duas ferramentas; nenhum teste obrigatório perdido |

### Fase D — integração universal com qualquer agente

| Item | Detalhe |
| --- | --- |
| Arquivos | `schemas/output.schema.json` + `docs/atlas/json.md`, `AGENTS.md` (cartão de 10 linhas), `cli.py` |
| APIs afetadas | envelope de erro uniforme; `--brief` padrão; `contract_version` |
| Testes | `test_json_envelope_matches_schema` (todos os comandos e caminhos de erro) |
| Riscos | quebrar consumidores que esperam saída completa; versionar e manter `--detail` |
| Compatibilidade | `ATLAS_FORMAT=1` para o comportamento antigo durante uma versão |
| Ganho esperado | qualquer agente descobre e consome o ATLAS com 1 comando e ≤ 1,2 KB |
| Critério de aprovação | schema validado em CI; erro uniforme em 100% dos comandos |

### Fase E — análise semântica opcional

| Item | Detalhe |
| --- | --- |
| Arquivos | novo `atlas_lib/semantic.py`; uso de `compile_commands.json` |
| APIs afetadas | `symbol`/`impact` com `confidence=high` quando houver AST |
| Testes | `test_semantic_absent_falls_back`, `test_semantic_reads_compile_commands` |
| Riscos | dependência de LLVM; nunca obrigatória; build de root ilegível |
| Compatibilidade | detecção em runtime; sem clangd, comportamento atual |
| Ganho esperado | call graph e sobrecargas corretos onde a ferramenta existir |
| Critério de aprovação | degradação idêntica sem ferramentas; ganho mensurável com elas |

### Fase F — benchmarks, testes de regressão e documentação

| Item | Detalhe |
| --- | --- |
| Arquivos | `docs/atlas/benchmarks.md`, `atlas_lib/report.py`, `tests/atlas/` |
| APIs afetadas | `eval` com referência de arquivo; `bench` registra frio/quente |
| Testes | suíte de 8 sintomas como teste de regressão de ranking |
| Riscos | benchmark virar número decorativo; exigir data/hardware no relatório |
| Compatibilidade | nenhuma |
| Ganho esperado | regressões de ranking detectadas automaticamente |
| Critério de aprovação | `eval` ≥ 40 perguntas, thresholds versionados, `bench` reprodutível |

Sequência recomendada: **A → B → C → D → F → E**. E é opcional e independente.

---

## 20. Critérios objetivos de aprovação

| Área | Critério mensurável | Baseline atual |
| --- | --- | --- |
| Velocidade | `update` sem mudanças ≤ 400 ms (quente) e ≤ 500 ms (frio, subconjunto inalterado) | 928 ms / 7.132 ms |
| Velocidade | `locate` ≤ 100 ms quente e ≤ 1.000 ms frio | 116 ms / 2.071 ms |
| Velocidade | `status`/`doctor` ≤ 200 ms | 569–1.214 ms |
| Precisão | 8 sintomas: arquivo principal no top-3 em ≥ 5/8; top-5 em ≥ 7/8 | 1/8 e 5/8 |
| Precisão | top-1 idêntico para `--limit` ∈ {3,5,8,20,50} em 8/8 | 2/8 |
| Precisão | inferido nunca acima de arquivo com match textual | violado em S1 |
| Testes | `tests-for` ≥ 1 teste para 6/6 fontes de `src/` testadas | 0/6 |
| Testes | nenhum resultado com > 5 testes para um único arquivo sem explicação | 20 para `x11_focus.cpp` |
| Impacto | `ConstructionPlacement.cpp` com `affected_count ≥ 5` e ≥ 1 teste | 0 e 0 |
| Tokens | `locate --brief` ≤ 1.200 chars; `diagnose` ≤ 1.200 chars | 23.934 / não existe |
| Contrato | envelope de erro uniforme em 100% dos comandos; schema validado em teste | inconsistente em `impact` |
| Segurança | nenhuma consulta altera arquivos; `update` grava só `.atlas` | já cumprido |
| Memória | pico ≤ 64 MB | 36 MB |
| Compatibilidade | funcionar com Python 3.10+ e SQLite ≥ 3.35, sem dependências externas no núcleo | hoje 3.13/stdlib |

---

## 21. Referências exatas ao código

| Arquivo:linha | O que está ali |
| --- | --- |
| `tools/atlas/atlas.py:1–14` | entrada e injeção de `sys.path` |
| `atlas_lib/cli.py:21–33` | `_normalize_argv` (flags globais antes/depois do subcomando) |
| `atlas_lib/cli.py:36–96` | definição de todos os subcomandos e flags |
| `atlas_lib/cli.py:99–330` | `_text_render` por comando (formatos de saída) |
| `atlas_lib/cli.py:333–371` | `main`: códigos 0/1/2 e envelope JSON |
| `atlas_lib/config.py:12–14` | `INDEX_VERSION`, `MAX_FILE_BYTES`, `MAX_FTS_CHARS` |
| `atlas_lib/config.py:61–82` | `IGNORED_DIRS` (sem `.turbo`) |
| `atlas_lib/config.py:177–213` | `should_index` |
| `atlas_lib/config.py:215–238` | `module_for` (`root` para `.turbo`/`prototype`) |
| `atlas_lib/db.py:14–121` | esquema SQLite/FTS5 |
| `atlas_lib/db.py:124–133` | conexão WAL e pragmas |
| `atlas_lib/db.py:163–170` | `index_is_compatible` (`INDEX_SCHEMA_VERSION`) |
| `atlas_lib/indexer.py:200–315` | `_extract_cpp_functions` (scanner linha a linha) |
| `atlas_lib/indexer.py:317–377` | `parse_cpp`; **336–339** bug de namespace |
| `atlas_lib/indexer.py:483–512` | `collect_files` (walk, symlink, `should_index`) |
| `atlas_lib/indexer.py:531–549` | `_resolve_include` (confirmed/inferred) |
| `atlas_lib/indexer.py:579–720` | `parse_cmake` (targets, tests, external) |
| `atlas_lib/indexer.py:722–960` | `index_repository`; **751–771** leitura+hash de todos; **832–835** FTS; **837–905** reconstrução do grafo |
| `atlas_lib/search.py:46–56` | `AREA_WEIGHTS` |
| `atlas_lib/search.py:68–96` | `_hint_map` / `_hint_score` (glossário) |
| `atlas_lib/search.py:98–131` | `_symbol_hits` (1 query por arquivo) |
| `atlas_lib/search.py:234–262` | `_synthesize_hint_rows` (candidatos inferidos) |
| `atlas_lib/search.py:264–271` | `_test_hits` (N+1) |
| `atlas_lib/search.py:273–309` | `_candidate_rows` (`max(limit*8,40)` + fallback LIKE) |
| `atlas_lib/search.py:316–321` | `_confidence` |
| `atlas_lib/search.py:343–511` | `locate` (ranking e agrupamento) |
| `atlas_lib/search.py:513–571` | `symbol` (definições + referências lidas do disco) |
| `atlas_lib/search.py:573–593` | `text_search` |
| `atlas_lib/graph.py:32–66` | consultas de include/target |
| `atlas_lib/graph.py:68–92` | `tests_for` (direção incorreta) |
| `atlas_lib/graph.py:105–195` | `impact` (BFS que não atravessa targets) |
| `atlas_lib/graph.py:197–236` | `trace` |
| `atlas_lib/diagnostics.py:25–44` | `_quick_stale` (chama `collect_files`) |
| `atlas_lib/diagnostics.py:46–82` | `status` |
| `atlas_lib/diagnostics.py:106–266` | `doctor` (12 códigos de issue) |
| `atlas_lib/diagnostics.py:262–265` | semântica de `ok`/`strict` |
| `atlas_lib/diagnostics.py:268–346` | `verify` (7 checks) |
| `atlas_lib/report.py:18–78` | `write_generated_docs` (grava arquivo versionado) |
| `atlas_lib/report.py:98–181` | `benchmark` (cópia isolada) |
| `atlas_lib/report.py:194–260` | `evaluate` (só módulo) |
| `tools/atlas/data/glossary.json` | 81 termos |
| `tools/atlas/data/eval_questions.json` | 12 perguntas |
| `tools/atlas/schemas/output.schema.json` | envelope (não validado) |
| `tests/atlas/test_atlas.py:28–247` | 11 testes de integração em fixture |
| `CMakeLists.txt:502–508` | `add_test(NAME linblux_atlas … unittest discover)` |
| `CMakeLists.txt:514–556` | labels/timeouts do TURBO |
| `tools/turbo/turbo_lib/inventory.py:76–141` | `ensure_atlas`, `atlas_snapshot` |
| `tools/turbo/turbo_lib/inventory.py:340–400` | `load_inventory` (consumo das tabelas) |
| `tools/turbo/turbo_lib/selection.py:1–420` | seleção de testes (grafo duplicado) |
| `tools/turbo/turbo_lib/runner.py:74–106` | `excerpt` (últimas 8 linhas de erro) |
| `tools/turbo/turbo_lib/report.py:57–174` | `format_result` (primeira falha + truncamento 1.200) |

---

## 22. Lacunas que ainda precisam de investigação

1. **Causa-raiz dos 8 sintomas não confirmada.** A auditoria fixou o arquivo que
   implementa o comportamento (grep com linha), não o defeito. Nenhum bug foi
   reproduzido — os números de recall medem localização, não diagnóstico.
2. **Medição fria é aproximada.** `posix_fadvise(DONTNEED)` descarta as páginas do
   processo, mas mantém cache de inode/diretório e o cache do sistema para outros
   arquivos. Um boot limpo com `drop_caches` (root) daria números piores; não foi
   feito por restrição de escopo.
3. **Build e testes da engine não foram executados** (nem `FULL`, nem gráficos).
   Portanto: não se sabe se os testes sugeridos passam, nem se o conjunto
   selecionado pelo TURBO realmente cobre as mudanças do WIP atual.
4. **`compile_commands.json` não foi validado como base semântica** (não há
   `clang`/`clangd`); 302 entradas, mas inclui `_deps/` e caminhos de `root`.
5. **Desempenho das skills/comandos do OpenCode** não foi observado em execução
   real (sem sessão de agente instrumentada).
6. **Suporte a ferramenta de terminal e ferramenta de terminal**: nenhuma configuração no repositório;
   marcado como NÃO VERIFICADO, não como suportado.
7. **Comportamento concorrente** (dois agentes rodando `update` ao mesmo tempo)
   não foi testado; há WAL + `busy_timeout=30 s`, mas não há teste.
8. **Qualidade do parser em arquivos gerados/macros** (`StudioPalette.generated.h`,
   `ThemeColors.generated.h`) não foi medida individualmente.
9. **Custo do `tests-conditional`** (44 de 62 no índice pré-auditoria) não foi
   confrontado com `ctest -N` por teste; apenas o total (64) coincide.
10. **Impacto real da remoção de `.turbo` do índice** sobre a qualidade do ranking
    não foi medido com um índice limpo — a estimativa é baseada em quais
    resultados deixariam de aparecer (S2, S4, S8).

---

### Registro de execução desta auditoria

Comandos de medição usados (todos somente leitura, exceto `update`/`bench` que
 gravam apenas em `.atlas/`):

```bash
git status -sb && git rev-parse HEAD && git rev-parse origin/main
python3 tools/atlas/atlas.py status --json
python3 tools/atlas/atlas.py locate "<sintoma>" --json [--limit 3|5|8|20|50]
python3 tools/atlas/atlas.py symbol EditorCamera --json
python3 tools/atlas/atlas.py text decal_flip_u --json
python3 tools/atlas/atlas.py impact src/scene/Scene.h --json
python3 tools/atlas/atlas.py trace "mundo padrao" --json
python3 tools/atlas/atlas.py tests-for <arquivo> --json
python3 tools/atlas/atlas.py doctor --json   # e --strict
python3 tools/atlas/atlas.py verify --json
python3 tools/atlas/atlas.py update --json
python3 tools/atlas/atlas.py bench --iterations 2 --json
python3 tools/atlas/atlas.py eval --json
python3 tools/turbo/turbo.py inventory --json
python3 tools/turbo/turbo.py test --mode fast --changed --plan
```

Consultas que falharam ou não produziram o esperado:

- `impact src/editor/ConstructionPlacement.cpp` → `affected_count = 0` (bug B6).
- `tests-for` em 6 arquivos de `src/` → lista vazia (bug B5).
- `locate` nos sintomas S4 e S8 → nenhum arquivo de referência no top-8.
- `verify` retornou `rc=1` antes da reindexação dos 4 arquivos do WIP (estado
  correto, mas confunde automação).
- `/usr/bin/time` não existe na máquina; a medição de RSS foi feita com
  `resource.getrusage`.

Estado final do repositório: `git status -s` com as mesmas 25 entradas do início;
nenhum commit, push, reset ou limpeza foi executado; nenhuma dependência instalada.

