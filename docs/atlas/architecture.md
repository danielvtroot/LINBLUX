# Arquitetura do ATLAS

## Objetivo

O ATLAS responde perguntas estruturais sobre o LINBLUX sem exigir que um agente
leia o repositório inteiro: onde um sintoma provavelmente mora, quais símbolos
existem, quem inclui um cabeçalho, quais testes cobrem uma área e o que pode
quebrar antes de uma alteração.

Ele é deliberadamente **local, determinístico e conservador**. O índice é um
cache; a verdade continua no código, no CMake, nos testes e nos contratos
manuais.

## Fluxo de dados

```text
arquivos do projeto
      │
      ▼
collect_files + should_index       (ignora builds, third_party, mídia, cache)
      │
      ▼
read_text_file + SHA-256           (limite de 2 MiB; binários/texto inválido fora)
      │
      ▼
parse_text                         (C++, Lua, Python, RML, Markdown, CMake...)
      │            │
      │            ├── symbols
      │            └── relations (includes, requires)
      ▼
transação SQLite                   (arquivos, símbolos, relações, targets, testes)
      │
      ├── file_fts (FTS5)           busca textual
      ├── cmake_targets/test_files  grafo de build e testes
      └── meta                       versão, contagens, hashes e data
      │
      ▼
consultas: locate, symbol, text, impact, trace, tests-for, doctor, verify
```

`index_repository` compara o SHA-256 do conteúdo. Arquivos inalterados não são
reparseados. Arquivos novos/alterados são reprocessados; removidos são apagados
com suas relações por `ON DELETE CASCADE`. O grafo CMake/testes é reconstruído
por inteiro em cada atualização porque é pequeno e precisa refletir remoções de
targets/testes.

O fixture controlado em `tests/atlas/fixture/` é copiado para uma raiz isolada
pelos testes e excluído do índice do projeto real. Sem essa exclusão, targets e
símbolos falsos apareceriam no inventário e no grafo de impacto.

## Módulos Python

| Arquivo | Papel |
| --- | --- |
| `atlas.py` | entrada; injeta `tools/atlas` no `sys.path` |
| `atlas_lib/cli.py` | argparse, normalização de flags, JSON/texto, códigos de saída |
| `atlas_lib/config.py` | ignore rules, linguagens, módulos, glossário, normalização |
| `atlas_lib/db.py` | SQLite/WAL, esquema, transação, metadados, integridade |
| `atlas_lib/indexer.py` | descoberta, hash, parsers, CMake, grafo, indexação incremental |
| `atlas_lib/search.py` | FTS5, ranking explicável, símbolos, texto |
| `atlas_lib/graph.py` | impacto reverso, testes e fluxo |
| `atlas_lib/diagnostics.py` | status, doctor, verify |
| `atlas_lib/report.py` | inventário, benchmark e avaliação |
| `data/glossary.json` | termos de domínio, módulos, caminhos e símbolos |
| `data/eval_questions.json` | amostra real de perguntas e respostas esperadas |
| `schemas/output.schema.json` | contrato mínimo do envelope JSON |

## Registro estrutural

Cada linha de `files` guarda caminho relativo, módulo, área, linguagem, tamanho,
mtime, SHA-256, número de linhas e se é externo. Símbolos guardam nome,
qualificado, tipo, linha, assinatura, origem e confiança. Relações guardam
origem, destino, tipo e confiança.

### Parsers e confiança

| Linguagem | Extração | Limite conhecido |
| --- | --- | --- |
| C/C++ | classes/structs/enums, funções/declarações/definições, `include`, `using` e `typedef` | regex por linha; não resolve templates, sobrecargas, macros ou chamadas |
| Lua | funções, `require` e tabelas | não é um parser Lua completo |
| Python | `def`/`class`/`import` | AST textual, sem resolução de imports |
| RML | elementos com `id` | IDs são elementos de UI, não símbolos C++ |
| Markdown | títulos como símbolos do documento | headings não são APIs |
| CMake | targets, fontes, dependências, `add_test`, `find_package` | condições são apenas marcadas, não avaliadas |

Símbolos C++ usam `origin=regex-cpp` e `confidence=medium`. Referências são
`textual`; includes resolvidos localmente podem ser `confirmed`, includes de
sistema/terceiros permanecem `inferred`. **Nenhuma dessas relações prova uma
chamada em tempo de execução.**

## Grafo de build e testes

`parse_cmake` reconhece `add_library`, `add_executable`, `target_sources`,
`target_link_libraries`, `add_test`, `set_tests_properties`, `find_package`,
`pkg_check_modules` e `FetchContent_Declare`. Caminhos relativos são
normalizados contra o diretório do `CMakeLists.txt`.

`tests_for` combina três evidências, em ordem:

1. arquivo-fonte declarado no alvo do teste;
2. include reverso: um teste que inclui um cabeçalho coberto pelo alvo;
3. alvo CMake do arquivo consultado.

Isso evita afirmar que um teste executa uma função; ele apenas indica cobertura
estrutural provável.

## Ranking do `locate`

O ranking é determinístico e cada parcela aparece em `score_breakdown`:

```text
score = fts
      + 4.0 por termo presente no caminho
      + glossário (módulo: 7; caminho: 3; total limitado a 18)
      + símbolos (exato: 10; prefixo: 6; contém: 3.5; símbolo de glossário: 8/4)
      + teste associado (1.5 por teste, máximo 4.5)
      + 5.0 quando o arquivo é apenas uma sugestão do glossário
      × peso da área
```

Pesos de área: `source=1.0`, `test=0.72`, `docs=0.55`, `asset=0.45`,
`tool=0.35`, `script=0.55`, `config=0.5`, `agent-config=0.35`,
`package=0.4`.

O glossário pode criar candidatos mesmo quando a consulta usa uma palavra
diferente do código (por exemplo, “animação” para `AnimationClip`). Esses
candidatos são marcados `inferred=true` e não devem ser lidos como evidência
textual. A saída agrupa no máximo três arquivos por módulo; o primeiro arquivo
pesa integralmente e os seguintes pela metade.

Consultas com acentos são normalizadas; plurais simples em português
(`-oes → -ao`, `-ais → -al`, `-s`) geram termos adicionais. Não é stemming
linguístico completo.

## Impacto e trace

`impact` faz BFS reverso por includes e por dependências de target, com
profundidade máxima 5 e limite de visitas. Ele também lista testes mapeados,
riscos por tipo de módulo e contratos manuais existentes. O resultado nunca
executa testes automaticamente.

`trace` usa os três primeiros módulos do `locate` e mostra símbolos, includes,
targets e testes dos arquivos candidatos. É um roteiro de investigação, não uma
traço de execução.

## Banco e integridade

- SQLite com WAL, `synchronous=NORMAL`, `foreign_keys=ON` e busy timeout.
- FTS5 com `unicode61 remove_diacritics 2`.
- `verify` roda `PRAGMA integrity_check`, verifica FTS, schema, hashes, contagem
  de arquivos, presença de símbolos, grafo CMake e uma consulta de fumaça.
- `doctor` usa tamanho/mtime para uma estimativa rápida de defasagem; não
  substitui `verify`.
- Índice incompatível, corrompido ou ausente é removido e recriado em `update`.

## Segurança e determinismo

- Caminhos de entrada são resolvidos e precisam permanecer dentro da raiz.
- Consultas SQL usam parâmetros, não concatenação de entrada.
- Nenhum script indexado é executado; nenhuma rede é usada.
- Ordenações têm desempate por módulo/caminho/linha para evitar resultados
  instáveis.
- O cache `.atlas/` é reconstruível e ignorado pelo Git.

## Limitações de engenharia

- Sem `clangd`, `rg` ou `ctags`, não há AST, call graph ou resolução semântica.
- Macros C++ e funções geradas podem aparecer de forma incompleta.
- Includes condicionais e testes dentro de `if(CMake)` são marcados, não
  avaliados.
- `locate` depende de vocabulário e do glossário; sinônimos ausentes podem
  produzir resultados vazios.
- Benchmark mede CPU/software rendering local; não representa GPU.
- MCP e integração com editores ficam como extensões futuras; a CLI/JSON já é a
  interface universal atual.
