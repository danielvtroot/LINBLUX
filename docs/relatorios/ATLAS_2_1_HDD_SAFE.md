# LINBLUX ATLAS 2.1 — MODO HDD-SAFE E PRESERVAÇÃO DO NOTEBOOK

**Data:** 2026-10-30
**HEAD de referência da auditoria:** `065f044` (`test(editor): validate face-based Move in the Studio harness`)
**HEAD remoto no início:** `065f0440040178d8e2ddfd38d935a723c516608a` (idêntico ao local)
**Escopo executado:** proteção dos dados, Fase A (política de I/O), Fase C (guarda no TURBO) e Fase D (documentação).
**Escopo não executado:** Fase B (cache RAM) — ver §6 — por falta de ganho medido e risco de swap.

## 1. Estado do projeto e proteção dos dados

| Item | Antes | Depois |
| --- | --- | --- |
| HEAD local | `065f044` | `065f044` + commits desta entrega |
| HEAD remoto | `065f044` | confirmado ao final (`git ls-remote`) |
| Arquivos rastreados modificados (outros agentes) | 22 | 22, intocados |
| Arquivos não rastreados | 4 (`docs/relatorios/AUDITORIA_*`, `LINBLUX_PLANO_*`, `tests/fixtures/audio/null.conf`) | 4, intocados |
| Backup do WIP | não existia | branch local `backup/wip-atlas21` = `c2b8e2e` (489 arquivos) |
| Espaço livre em `/` | 777 GB | 777 GB |

O backup foi feito com índice Git temporário (`GIT_INDEX_FILE` em `/tmp`), `git write-tree` e
`git commit-tree`, criando um commit **sem alterar o worktree nem o índice real**. Nada foi
apagado, resetado, "limpado" ou enviado à força. Nenhum caminho de outro agente entrou nos
commits desta tarefa.

Código existente **somente no HDD** além do HEAD: os 22 caminhos modificados e os 4 não
rastreados listados acima (todos agora também dentro de `backup/wip-atlas21`).

## 2. O que foi implementado (Fase A)

`tools/atlas/atlas_lib/io_mode.py` (novo, ~120 linhas):

- `IoPolicy` imutável com `read_only`, `allow_index_write`, `allow_integrity`, `allow_bench`,
  `allow_reports`, `check_staleness` e `blocked_reason(command)`.
- `normal`, `hdd-safe` e `hdd-critical`; aliases `safe`/`critical`; origem `--io-mode` ou
  `ATLAS_IO_MODE`; valor inválido é recusado com a lista de modos válidos.
- Resolver um modo **não lê o disco** (apenas ambiente).

`db.py`:

- `_readonly_connection` abre com `mode=ro`, `query_only=1`, `temp_store=MEMORY` e
  `busy_timeout`; **nunca** troca `journal_mode`, portanto não cria WAL/SHM.
- `open_db_readonly`: usa `immutable=1` quando não há WAL pendente (nada a recuperar), evitando
  escrever `-wal`/`-shm` a cada consulta; se houver WAL não vazio (desligamento sujo), usa
  `mode=ro` para não ignorar páginas confirmadas; último recurso é `immutable=1`.
- `open_db(..., policy=...)` roteia consultas para a conexão somente leitura e recusa
  `create=True` em modo somente leitura.

`indexer.py`: `index_repository(..., policy=...)` recusa indexação em `hdd-critical` **antes** de
criar cache, remover índice ou varrer arquivos; o handle de escrita só é aberto depois da
autorização.

`diagnostics.py`: `status`/`doctor`/`verify` aceitam `policy`; `status` deixa de fazer a varredura
de metadados quando o modo não permite verificar defasagem e reporta `staleness_checked=false`.
Novo `health(root, policy)` lê `/proc/meminfo` e `/proc/swaps`, tamanho/idade do índice e
`MemAvailable`, e sugere orçamento de cache — sem SMART, sem teste de setores, sem escrita.

`cli.py`: `--io-mode` global (antes ou depois do subcomando), comando `health`, bloqueio com
mensagem acionável e `io_mode`/`io_note` preservados também em `--brief --json`.

Tabela de decisão efetiva:

| Comando | `normal` | `hdd-safe` | `hdd-critical` |
| --- | --- | --- | --- |
| `locate`, `symbol`, `text`, `impact`, `trace`, `tests-for`, `diagnose`, `fix-plan`, `eval` | leitura/escrita | **somente leitura** | **somente leitura** |
| `status`, `health` | defasagem verificada | verificada (`stat`) | não verificada, avisada |
| `init`, `update` | permitido | permitido quando pedido | **rc=2, bloqueado** |
| `verify`, `doctor` | permitido | permitido (precisa de handle gravável para `fts integrity-check`) | **bloqueado** |
| `docs`, `bench` | permitido | permitido | **bloqueado** |

## 3. Fase C — guarda no TURBO

`tools/turbo/turbo_lib/cli.py`: `_hdd_critical_refusal` recusa `build`, `test` (execução real) e
`benchmark` quando `ATLAS_IO_MODE=hdd-critical`, com mensagem que indica o modo autorizado.
`test --plan` continua permitido porque só lê inventário/CMake.

Motivo: sem isso, o modo crítico do ATLAS seria anulado por um `turbo.py test` iniciado por
hábito — compilar e rodar 60+ testes é a maior fonte de I/O do projeto. A recusa é explícita;
nunca transforma teste não executado em aprovação.

## 4. Testes executados

| Suíte | Resultado | Duração |
| --- | --- | --- |
| `python3 -m unittest discover -s tests/atlas -p 'test_*.py'` | **40 testes, OK** (8 novos em `tests/atlas/test_io_mode.py`) | 38,4 s |
| `python3 -m unittest discover -s tests/turbo -p 'test_*.py'` | **38 testes, OK** (sem alteração de lógica de seleção) | 11,7 s |
| `python3 -m unittest tests.turbo.test_io_mode_guard -v` | **3 testes, OK** (novo) | 3,7 s |

Cobertura dos 13 itens obrigatórios do pedido:

| # | Requisito | Teste |
| --- | --- | --- |
| 1 | HDD-Safe não reindexa sem necessidade | `test_safe_queries_are_read_only`, `test_safe_update_is_explicit_and_idempotent` |
| 2 | HDD-Critical não grava | `test_critical_reads_never_touch_the_index`, `test_critical_blocks_integrity_reports_and_bench` |
| 3 | Índice ausente não dispara varredura | `test_critical_query_without_index_does_not_scan` |
| 4 | Cache RAM respeita limites | **não implementado** (Fase B adiada; `health` apenas reporta orçamento) |
| 5 | Cache inválido não retorna como atual | `test_stale_file_is_reported_without_writing` |
| 6 | Índice desatualizado gera aviso | idem (`stale_estimate`, `warnings`) |
| 7 | SQLite recupera índice perdido | `test_corrupt_index_recovers` (existente, modo normal) |
| 8 | JSON permanece válido | `test_health_json_is_small_and_parsable_in_every_mode` + todos os testes `--json` |
| 9 | `diagnose` continua funcionando | `test_critical_reads_never_touch_the_index` |
| 10 | `tests-for`/`impact` continuam corretos | testes existentes de `tests/atlas/test_atlas.py` |
| 11 | Dois agentes sem corromper | `test_concurrent_readers_do_not_corrupt_the_index` |
| 12 | Falta de RAM não derruba a máquina | **não implementado** (nenhuma alocação nova é feita; apenas leitura de `/proc/meminfo`) |
| 13 | Modo normal compatível | `test_normal_mode_remains_writable_and_compatible` + suíte antiga |

## 5. Medições reais (sem estressar o HDD)

Máquina: Ryzen 5 3500U, 5,8 GiB RAM, HDD. Índice com 425 arquivos, 7.823.360 bytes.

| Operação | Modo | Tempo | Saída |
| --- | --- | ---: | ---: |
| `diagnose "Decal ... espelhado ... esticada" --brief` | `hdd-critical` | 188,3 ms | 661 B |
| `diagnose "decal espelhado distorcido" --brief` | `hdd-safe` | 119,1 ms | 629 B |
| `health --brief` | `hdd-critical` | 77,1 ms | 493 B |
| `status` | `hdd-safe` | 97,9 ms | 520 B |
| `locate "decal espelhado" --brief` | `hdd-critical` | 120,2 ms | 412 B |
| `tests-for src/rendering/SceneRenderer.cpp --brief` | `hdd-critical` | 88,1 ms | 535 B |
| `update` (bloqueado) | `hdd-critical` | 73,9 ms | rc=2 |

- **Índice inalterado:** SHA-256 + tamanho + `mtime_ns` de `index.sqlite*` idênticos antes/depois
  de todas as consultas acima (`index unchanged: True`).
- **Pico de RSS dos processos filhos:** 24,8 MiB.
- **Memória no momento:** 4.029 MiB disponíveis de 5.866 MiB; swap em uso 0 MiB.
- **I/O:** não foram usados `drop_caches`, `dd`, `fsck`, `badblocks` nem leitura integral do
  HDD. Contadores por processo (`/proc/pid/io`) não são observáveis depois que o processo
  termina sem ferramentas externas; a prova usada foi o hash/metadata do índice e do cache.
  **Não se afirma "zero I/O de disco"**: páginas não residentes podem ser lidas em qualquer modo.

## 6. Fase B adiada: cache em RAM (decisão medida, não omissão)

As consultas já ficam entre 77 ms e 190 ms com 24,8 MiB de pico. Um cache de resultados
acrescentaria 16–32 MiB, risco de swap em HDD e invalidação a cada `update` — sem ganho
perceptível para um agente que faz 1–3 consultas por tarefa. `health` já informa
`MemAvailable`, swap e um orçamento sugerido (8–32 MiB) para uma fase futura que possa ser
medida contra uma carga real. Nenhuma alocação nova é feita hoje.

## 7. Bugs/limitações encontrados nesta fase

1. **`mode=ro` não basta em banco WAL**: a primeira versão criava `-wal` (0 B) e `-shm`
   (32 KiB) em cada consulta. Corrigido com `immutable=1` quando não há WAL pendente; a
   regressão está coberta por asserção de snapshot do `.atlas`.
2. **`verify`/`doctor` exigem handle gravável**: `INSERT INTO file_fts(file_fts)
   VALUES('integrity-check')` é recusado por `query_only`. Esses comandos continuam usando
   handle gravável e são **bloqueados** em `hdd-critical`.
3. **Defasagem não é verificável sem ler metadados**: em `hdd-critical` o ATLAS não promete
   frescor; `status`/`health` dizem explicitamente "não verificada".
4. **`immutable=1` ignora WAL**: por isso só é usado quando o WAL está ausente ou vazio.
5. **TURBO `test --plan` ainda lê CMake/CTest** (barato, mas é I/O); não foi alterado.

## 8. Compatibilidade

- Modo `normal` é o padrão quando `ATLAS_IO_MODE` e `--io-mode` não são informados; nenhum
  contrato JSON existente mudou (apenas campos adicionais `io_mode`, `io_note`,
  `staleness_checked`).
- Códigos de saída preservados: `0` sucesso, `1` verify/doctor falho, `2` erro/bloqueio.
- `tools/atlas/atlas.py` continua funcionando só com a biblioteca padrão (nenhuma dependência
  nova, nada instalado).

## 9. Fluxo recomendado para qualquer agente

```bash
ATLAS_IO_MODE=hdd-safe python3 tools/atlas/atlas.py diagnose "sintoma" --brief
# ler somente o arquivo/linha indicados, editar
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.cpp --brief
python3 tools/turbo/turbo.py test --mode fast --changed    # modo hdd-safe/normal
# publicar checkpoint
```

Em `hdd-critical`: consultar `diagnose`/`locate`/`status`/`health`; **não** executar `update`,
`verify`, `doctor`, `bench`, `docs`, `turbo build` nem `turbo test` sem autorização.

## 10. Riscos restantes

- Índice pode ficar defasado por decisão explícita do modo crítico; a resposta sempre avisa.
- `hdd-safe` ainda permite `verify` integral (hash de todos os arquivos) — é explícito, nunca
  automático, mas custa leitura sequencial do HDD.
- Nenhum teste cobre falha física de disco; `health` é operacional, não diagnóstico de hardware.
- Fases B (cache) e a compactação de logs do TURBO além do resumo de primeira falha já
  existente continuam como trabalho futuro.
