# Benchmarks reais do ATLAS TURBO

Data da auditoria: **2026-10-25**. Máquina: Linux, 8 CPUs, cerca de 5,7 GiB de
RAM; build `build-linblux/` em Release com **Unix Makefiles**, sem Ninja, ccache,
clangd, rg ou ctags. Os testes gráficos usam Xvfb e renderização por software.

Nenhum número desta página é uma projeção apresentada como resultado. Onde não
houve execução, está escrito **estimativa**; a origem é indicada.

## Suíte completa: estimativa, não execução

```bash
python3 tools/turbo/turbo.py inventory --json
```

- 35 testes CTest registrados após o TURBO;
- soma dos custos medidos em `build-linblux/Testing/Temporary/CTestCostData.txt`:
  **≈865–868 s** (≈14,4–14,5 min);
- 20 testes classificados como gráficos, 17 como lentos;
- `test --mode full --changed --plan` seleciona os 35 testes registrados e o
  target `all`, com estimativa de 865,2 s;
- a suíte FULL **não foi executada** nesta validação. O valor acima é uma
  estimativa histórica do CTest, não um tempo de parede comprovado.

## FAST real

Execução com 20 arquivos alterados no worktree, incluindo código do editor e os
próprios arquivos do TURBO:

```bash
python3 tools/turbo/turbo.py test --mode fast --changed --no-cache
```

| Medida | Valor |
| --- | --- |
| build (incluindo configure quando necessário) | 4,634 s |
| testes executados | 3 (`linblux_manipulation`, `linblux_studio`, `linblux_turbo`) |
| testes aprovados | 3 |
| duração total | 10,737 s |
| saída compacta do relatório | ≈355 caracteres no benchmark; logs completos locais |

O status dessa execução foi de cobertura desconhecida porque o `CMakeLists.txt`
ainda não estava commitado e o TURBO corretamente não aprovou cobertura plena.

Benchmark posterior, com o build já aquecido:

| Etapa | Valor |
| --- | --- |
| inventário CTest/ATLAS | 0,107 s |
| build incremental | 1,780 s |
| testes FAST | 5,871 s |
| total | 7,788 s |
| estimativa CTest do plano FAST | 6,184 s |
| saída compacta | ≈355 caracteres |

O binário `linblux_turbo` responde por cerca de 5,9 s desse total. Sem ele, os
outros dois testes headless levam menos de 0,2 s.

### Comando manual equivalente

Para separar o custo da seleção/supervisão do custo dos próprios testes, o mesmo
conjunto de três testes foi executado diretamente:

```bash
ctest --test-dir build-linblux -R '^(linblux_manipulation|linblux_studio|linblux_turbo)$' -j2
```

Tempo de parede medido: **5,928 s**. O TURBO adiciona inventário, seleção,
fingerprint, logs e proteção de concorrência; a diferença não é apresentada como
melhoria percentual.

### Estado final após o commit

Depois de commitado o próprio TURBO, restaram apenas quatro alterações locais do
editor e do harness. Nesse estado:

```bash
python3 tools/turbo/turbo.py test --mode fast --changed --no-cache
python3 tools/turbo/turbo.py benchmark --run-fast --json
```

| Medida | Valor |
| --- | --- |
| alterações | 4 arquivos do worktree |
| testes executados | 2 (`linblux_manipulation`, `linblux_studio`) |
| cobertura | `known` |
| build | 0,9 s |
| FAST real | 1,345 s |
| benchmark: inventário | 0,602 s |
| benchmark: build | 0,715 s |
| benchmark: testes | 0,164 s |
| benchmark: total | **1,508 s** |
| saída compacta | 353 caracteres |
| estimativa CTest do plano | 0,302 s |

Uma segunda chamada sem `--no-cache`, com o mesmo fingerprint, retornou
`REUSED` em vez de executar os testes. O relatório imprime que a compilação foi
reutilizada e que os números de testes são histórico, não uma execução atual.

## IMPACT real

```bash
python3 tools/turbo/turbo.py test --mode impact --changed --no-cache
```

| Medida | Valor |
| --- | --- |
| alterações | 20 arquivos (incluindo CMake ainda não commitado) |
| testes selecionados/executados | 19 |
| testes gráficos | 4 (limite padrão do IMPACT) |
| build | 1,5 s |
| duração total | 58,5 s |
| resultado | 19 aprovados; **PASSED_PARTIAL** por limite de gráficos e cobertura estrutural |

O plano sem limite indicava aproximadamente 563 s para 13 testes, com 11
gráficos, naquele estado do editor. O limite de quatro é um controle de custo e
aparece explicitamente no relatório; não é uma aprovação FULL.

### IMPACT final após o commit

Com as quatro alterações locais do editor/harness e a documentação de benchmark
modificada:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed --no-cache
```

| Medida | Valor |
| --- | --- |
| alterações | 5 arquivos |
| testes executados | 6 |
| testes gráficos | 4 (limite padrão) |
| demais gráficos filtrados | 8 |
| build | 1,666 s |
| duração total | 45,320 s |
| resultado | 6 aprovados; `PASSED_PARTIAL` por cobertura limitada |
| cobertura | `partial` |

Os testes executados foram `linblux_engine_default_restart`,
`linblux_editor_restart`, `linblux_gizmo_visual`, `linblux_manipulation`,
`linblux_rig_image` e `linblux_studio`. O harness
`tests/engine_default_restart.py` não tem associação semântica segura, então o
plano também o marca como sem mapeamento; isso mantém o estado parcial em vez de
declarar cobertura plena.

## Build incremental

```bash
python3 tools/turbo/turbo.py build --target auto
```

- targets: `linblux_manipulation_tests`, `linblux_studio`, `linblux_studio_tests`;
- duração medida: **1,881 s** com o build aquecido (sem recompilação);
- a primeira execução FAST com os arquivos do editor ainda pendentes levou
  **4,634 s** de build, incluindo configure.

## Por que a suíte completa não é executada por padrão

Os testes gráficos de UI dominam o custo: há registros de 40 s a mais de 150 s
por teste isolado em `CTestCostData.txt`, com Xvfb e software rendering. Para uma
mudança pequena, executar todos eles em toda iteração não é uma validação
proporcional. O TURBO:

1. limita FAST a headless;
2. inclui integração no IMPACT com limite explícito;
3. deixa FULL disponível para release/estrutura;
4. nunca transforma um resultado parcial em aprovação completa.

## Reprodução e limitações

- Os tempos variam com carga da máquina e cache do sistema de arquivos.
- Não há GPU física; os valores gráficos não representam uma máquina com GPU.
- `ninja` e `ccache` não estão instalados; não foi possível medir essas
  otimizações.
- A suíte FULL não foi executada, portanto não há comparação de tempo de parede
  FULL medida contra FAST medida. A comparação disponível é FAST/IMPACT medidos
  versus a estimativa CTest da suíte completa.
- O relatório compacto existe para reduzir tokens do agente; o log completo
  permanece em `.turbo/logs/<run-id>/` e pode ser aberto apenas quando necessário.
