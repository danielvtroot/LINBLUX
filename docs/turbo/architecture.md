# Arquitetura do ATLAS TURBO

## Princípio

O TURBO é um executor local, determinístico e explicável. Ele não substitui o
CMake, o CTest ou o ATLAS: lê os dados reais dessas ferramentas, seleciona um
escopo e registra evidências. Nenhuma etapa usa rede ou inventa resultados.

```text
Git (worktree)
    │
    ▼
changes.py ──► inventory.py ──► selection.py
                    ▲                 │
                    │                 ▼
             CMake/CTest/ATLAS     runner.py
                                      │
                                      ▼
                                  cache.py
                                      │
                                      ▼
                                  report.py
```

## Módulos

| Arquivo | Responsabilidade |
| --- | --- |
| `tools/turbo/turbo.py` | entrada executável |
| `turbo_lib/cli.py` | comandos, estados, orquestração e códigos de saída |
| `turbo_lib/config.py` | caminhos, versões, jobs seguros e regras de módulo/asset |
| `turbo_lib/changes.py` | alterações reais, exclusão de builds/caches e fingerprint |
| `turbo_lib/inventory.py` | CTest JSON, custos, labels, ambiente e snapshot ATLAS |
| `turbo_lib/selection.py` | grafo de targets, consumidores, módulos, caps e cobertura |
| `turbo_lib/runner.py` | Popen, logs, timeout, grupo de processos, JUnit e excerpts |
| `turbo_lib/cache.py` | histórico, estado, trava, assinatura de build e reuso |
| `turbo_lib/docscheck.py` | links Markdown e JSON inválido antes de qualquer build |
| `turbo_lib/organize.py` | auditoria somente leitura de estrutura/duplicatas |
| `turbo_lib/report.py` | saída compacta em português e estado consultável |
| `turbo_lib/benchmark.py` | registros de medições reais, sem percentuais inventados |

## Fontes de seleção

1. O worktree é a fonte da verdade para alterações não commitadas. Arquivos em
   `build*`, `cache/`, `.turbo/`, `.atlas/` e `__pycache__/` são ignorados.
2. O ATLAS fornece módulos, fontes de targets, dependências e relações de
   include. Como `clangd`/`ctags` não estão disponíveis, essas relações têm
   confiança textual `medium`; não são tratadas como chamadas comprovadas.
3. O CTest fornece o conjunto realmente registrado, comando, ambiente, labels,
   timeout, `PROCESSORS`, `RESOURCE_LOCK` e JUnit. Testes condicionais ausentes
   do build atual nunca são considerados executados.
4. `CTestCostData.txt` fornece duração medida e número de execuções. É usado
   para estimativa, classificação de lentidão e ordenação de candidatos.

Um teste entra no plano por uma destas evidências, registradas em `reasons`:

- fonte de teste ou `test_files` alterado;
- target do teste no fecho de um target afetado;
- módulo afetado compatível com o módulo do teste;
- regra explícita de asset/recurso;
- harness Python/shell com nome exato;
- mudança de CMake, que amplia conservadoramente o escopo.

Se nenhuma associação segura existir, o TURBO seleciona uma linha de base
headless quando disponível e marca a cobertura como desconhecida. Se todos os
candidatos forem gráficos/lentos no FAST, não executa um substituto arbitrário:
retorna cobertura desconhecida e exige IMPACT/FULL.

## FAST, IMPACT e FULL

- **FAST** exclui testes gráficos e lentos. Compila targets afetados e os
  executáveis dos testes selecionados. Não declara cobertura plena quando há
  alteração de CMake, asset sem regra ou base sem associação.
- **IMPACT** mantém FAST, inclui consumidores e integrações associadas e limita
  por padrão a quatro testes gráficos. O limite é um controle de custo, não uma
  afirmação de cobertura total; os omitidos aparecem em `filtered` e o estado é
  `PASSED_PARTIAL` quando a cobertura fica limitada.
- **FULL** usa todos os testes registrados e não é executado automaticamente por
  uma mudança pequena.

## Cache e invalidação

O fingerprint inclui HEAD, caminho/conteúdo dos arquivos alterados, modo,
seleção, cobertura, assinatura de `CMakeCache.txt` e versões de ferramentas. Um
resultado `REUSED` só é permitido quando:

- o último estado era aprovado, não parcial e sem timeout;
- o fingerprint e a seleção são idênticos;
- a cobertura era `known`;
- todos os testes selecionados são headless determinísticos e reutilizáveis.

Testes gráficos, vídeo, falhas e resultados parciais nunca são reutilizados. Um
`REUSED` nunca é apresentado como execução atual.

## Supervisão de processos

Cada execução recebe um `run_id` e um diretório próprio em `.turbo/logs/`. O
runner usa `start_new_session=True`, grava stdout/stderr no log e mantém o
código de saída. Em timeout:

1. envia `SIGTERM` ao grupo de processos;
2. aguarda uma janela curta;
3. envia `SIGKILL` se ainda houver processos;
4. registra `TIMEOUT` e não emite aprovação.

A trava `.turbo/run.lock` é um `flock` de processo, separado do cache. Dois TURBO
no mesmo checkout recebem uma recusa explícita em vez de disputar o build. O
estado é consultável em `.turbo/state.json`; `status` verifica se o PID ainda
existe e não exige polling do agente.

## Segurança e limites

- Não há `shell=True`; comandos são listas de argumentos.
- Caminhos são validados dentro da raiz; logs e caches ficam em `.turbo/`.
- `organize` é somente leitura e `--apply` é recusado.
- Nenhum `git reset --hard`, `git clean -fd` ou limpeza em massa é executado.
- A ausência de `ninja`, `ccache`, `clangd`, `rg` e `ctags` é registrada como
  limitação; o build permanece Unix Makefiles e a evidência C++ é textual.
- A suíte completa não é executada implicitamente. A estimativa histórica não é
  tratada como resultado real.
