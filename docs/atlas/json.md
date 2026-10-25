# Contrato JSON

Todas as consultas aceitam `--json` (`-j`). O envelope é estável:

```json
{
  "ok": true,
  "command": "locate",
  "data": {}
}
```

Em erro de entrada, índice ou SQLite:

```json
{
  "ok": false,
  "command": "impact",
  "error": "arquivo ou símbolo não encontrado"
}
```

O schema mínimo versionado está em
`tools/atlas/schemas/output.schema.json`. Ele valida o envelope, não cada campo
específico de comando. Novos campos podem ser adicionados sem quebrar
consumidores; campos existentes não devem mudar de significado sem incremento
de `INDEX_VERSION`/documentação.

## Códigos de saída

| Código | Significado |
| ---: | --- |
| 0 | sucesso |
| 1 | consulta sem resultado fatal, `verify` falhou ou `doctor --strict` encontrou problema |
| 2 | erro de argumento, caminho, índice ausente/corrompido ou SQLite |

`--quiet` silencia mensagens auxiliares; `--json` também evita texto no stdout.
Use `--root <diretório>` para apontar outro projeto.

## `init` / `update`

```json
{
  "ok": true,
  "command": "update",
  "data": {
    "indexed_files": 202,
    "added": 0,
    "updated": 1,
    "deleted": 0,
    "unchanged": 201,
    "skipped": 15,
    "symbols": 2409,
    "relations": 895,
    "symbols_changed": 3,
    "relations_changed": 0,
    "elapsed_seconds": 0.57,
    "database_bytes": 3932160,
    "counts": {
      "files": 202,
      "symbols": 2409,
      "relations": 895,
      "cmake_targets": 34,
      "tests": 33,
      "external_deps": 14
    }
  }
}
```

`symbols` e `relations` são totais após a atualização; `*_changed` mostram
quanto foi inserido no último processamento.

## `locate`

Campos principais:

- `query`, `tokens`, `confidence`: consulta normalizada e confiança global.
- `candidates`: módulos ordenados.
- `candidates[].files[]`: arquivos com `score` e `score_breakdown`.
- `candidates[].symbols[]`: até quatro símbolos representativos do módulo.
- `candidates[].tests[]`: testes associados.
- `candidates[].inferred`: `true` quando o módulo veio do glossário, sem
  correspondência textual forte.
- `total_files_considered`: linhas avaliadas antes do agrupamento.

Exemplo reduzido:

```json
{
  "ok": true,
  "command": "locate",
  "data": {
    "query": "a camera nao acompanha o personagem",
    "tokens": ["camera", "acompanha", "personagem"],
    "confidence": "alta",
    "candidates": [
      {
        "module": "game",
        "score": 72.5,
        "files": [
          {
            "path": "src/game/Runtime.h",
            "score": 49.0,
            "score_breakdown": {
              "fts": 12.4,
              "path": 4.0,
              "glossary": 7.0,
              "symbols": 18.0,
              "tests": 0.0,
              "area_weight": 1.0
            },
            "evidence": ["glossário: camera → módulo game", "símbolo ..."]
          }
        ],
        "symbols": [
          {"qualified": "loblox::game::PlayerCamera", "kind": "class", "path": "src/game/Runtime.h", "line": 26}
        ],
        "tests": [],
        "inferred": false
      }
    ],
    "next": ["aprofundar com: atlas.py impact <arquivo> e atlas.py symbol <símbolo>"],
    "limitations": ["ranking textual/regex; não é análise semântica completa de C++"]
  }
}
```

`score_breakdown` é a fonte para explicar por que um arquivo apareceu. Não use
apenas `score` para afirmar causalidade.

## `symbol`

```json
{
  "ok": true,
  "command": "symbol",
  "data": {
    "name": "EditorCamera",
    "definitions": [
      {
        "name": "EditorCamera",
        "qualified": "loblox::EditorCamera",
        "kind": "class",
        "path": "src/rendering/EditorCamera.h",
        "module": "rendering",
        "line": 9,
        "end_line": 76,
        "signature": "class EditorCamera {",
        "origin": "regex-cpp",
        "confidence": "medium"
      }
    ],
    "references": [
      {"path": "src/rendering/EditorCamera.cpp", "line": 1, "text": "#include \"rendering/EditorCamera.h\""}
    ],
    "limitations": ["não há distinção semântica entre sobrecargas e namespaces homônimos"]
  }
}
```

`definitions` inclui declarações e definições; `end_line` só existe quando o
scanner encontrou um corpo. `references` é busca textual, não call graph.

## `text`

```json
{
  "ok": true,
  "command": "text",
  "data": {
    "query": "ExperienceManager",
    "results": [
      {"path": "src/experiences/ExperienceManager.h", "module": "experiences", "snippet": "class [[ExperienceManager]]"}
    ]
  }
}
```

## `impact`

```json
{
  "ok": true,
  "command": "impact",
  "data": {
    "input": "src/core/Widget.h",
    "resolved_file": "src/core/Widget.h",
    "module": "core",
    "public_interface": true,
    "levels": [
      {"depth": 1, "nodes": [{"path": "src/core/Widget.cpp", "module": "core", "relation": "include", "confidence": "confirmed", "via": "src/core/Widget.h"}]}
    ],
    "affected_files": ["src/core/Widget.cpp"],
    "affected_count": 1,
    "tests": [{"name": "widget_test", "command": "widget_test", "target": "fixture_tests", "source_path": "tests/widget_test.cpp", "timeout": null, "origin": "cmake"}],
    "risks": ["cabeçalho: interface de compilação pode afetar todos os consumidores"],
    "documentation": ["docs/atlas/module-contracts/core.md"],
    "limitations": ["dependências de include e CMake são estruturais; chamadas dinâmicas/Lua/eventos podem não aparecer"]
  }
}
```

## `tests-for`

```json
{
  "ok": true,
  "command": "tests-for",
  "data": {
    "path": "src/core/Widget.h",
    "tests": [
      {"name": "widget_test", "command": "widget_test", "target": "fixture_tests", "source_path": "tests/widget_test.cpp", "timeout": null, "origin": "cmake"}
    ]
  }
}
```

## `trace`

Retorna `query`, `confidence`, `flow[]` (módulo, caminho, símbolos, includes,
targets, testes), `next` e `limitations`. O fluxo é um roteiro de leitura, não
uma prova de execução.

## `status`

Inclui `exists`, `compatible`, `counts`, `database_bytes`, `stale_estimate`,
`missing_files`, `removed_files`, `external_deps`, `tools`, `next` e
`limitations`. `tools.semantic_cpp` documenta a ausência de ferramentas
semânticas.

## `doctor` / `verify`

`doctor` retorna `ok`, `summary` (`alta`, `media`, `baixa`) e `issues[]` com
`severity`, `code`, `message`, `examples` e `fix`. `verify` retorna `ok`,
`checks[]` e `limitations`. Use os códigos (`index-stale`, `query-smoke`,
`content-hashes` etc.) para automação; não dependa apenas do texto em português.

## `bench` / `eval`

`bench` retorna tempos em milissegundos, contagens, `index_bytes`,
`peak_rss_kib`, `first_index` e `token_economy`. `eval` retorna totais, taxas,
metodologia, limitações e a lista por pergunta com `module_rank`/`symbol_rank`.

Esses números são medições locais; não são garantias de desempenho em outra
máquina ou GPU.
