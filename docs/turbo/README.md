# LINBLUX ATLAS TURBO

O TURBO é a camada local de compilação incremental e validação explicável do
LINBLUX. Ele usa dados reais do Git, CMake, CTest e do índice
[ATLAS](../atlas/README.md), sem rede, servidor ou banco externo. Use pela CLI:

```bash
python3 tools/turbo/turbo.py doctor
python3 tools/turbo/turbo.py inventory
python3 tools/turbo/turbo.py build --target auto
python3 tools/turbo/turbo.py test --mode fast --changed
python3 tools/turbo/turbo.py test --mode impact --changed
python3 tools/turbo/turbo.py test --mode full
python3 tools/turbo/turbo.py report
python3 tools/turbo/turbo.py status
python3 tools/turbo/turbo.py benchmark --run-fast
python3 tools/turbo/turbo.py organize --dry-run
```

Todas as consultas aceitam `--json`, `--root`, `--build-dir`, `--jobs` e
`--no-atlas-update`. `test --plan` mostra a seleção sem compilar nem executar.

## Modos

| Modo | Uso | O que executa |
| --- | --- | --- |
| `fast` | edição local | targets afetados e testes headless diretamente associados; não inicia gráficos pesados |
| `impact` | antes da entrega normal | FAST + consumidores de target/módulo + integrações gráficas relevantes, com limite padrão de 4 gráficos |
| `full` | mudança estrutural, release ou pedido explícito | todos os testes registrados compatíveis com o ambiente |

FAST aprovado **não** aprova FULL. IMPACT limitado por gráficos é explicitamente
marcado como parcial. O sistema nunca transforma ausência de testes em "100%
aprovado".

## Seleção explicável

O plano é formado por:

1. alterações reais do worktree (`git status`/`git diff`, sem builds/caches);
2. módulos e consumidores reversos do ATLAS;
3. fontes, dependências e targets do CMake;
4. testes registrados no CTest, propriedades, labels, ambiente e duração medida
   em `CTestCostData.txt`;
5. regras conservadoras para assets/JSON, CMake, documentação e ferramentas.

Cada teste selecionado carrega `reasons`; cada teste omitido por modo ou limite
aparece em `filtered`. Headers amplamente incluídos, mudanças de CMake, cobertura
desconhecida e limites de gráficos geram avisos. O TURBO não infere chamadas
semânticas de C++: o ATLAS continua com confiança textual/regex `medium`.

## Estados

| Estado | Significado |
| --- | --- |
| `PASSED` | os testes selecionados passaram agora; o escopo é o do modo |
| `PASSED_PARTIAL` | passaram, mas a cobertura é desconhecida/limitada; não é aprovação plena |
| `REUSED` | resultado anterior reutilizado; **não** foi executado agora |
| `FAILED` | falha de teste ou validação |
| `TIMEOUT` | processo e grupo encerrados por limite |
| `BUILD_FAILED` / `CONFIG_FAILED` | falha de compilação/configuração |
| `NOT_RUN` / `UNKNOWN` / `NO_TESTS` | nada foi aprovado |
| `DOCS_OK` / `ASSET_OK` | somente documentação/JSON validado, sem alegar teste da engine |

O cache só reutiliza resultado aprovado quando código, configuração, ferramentas,
seleção e cobertura são idênticos. Testes gráficos, de vídeo, resultados parciais
e falhas nunca são reutilizados como aprovação.

## Logs e supervisão

Os logs completos ficam em `.turbo/logs/<run-id>/`:

- `configure.log`, `build.log`, `tests.log`, `test-results.xml`;
- o relatório compacto da CLI não imprime a saída inteira;
- em falha, mostra o primeiro teste/detalhe relevante e o caminho do log;
- timeouts encerram o grupo de processos (SIGTERM, depois SIGKILL) e registram
  `TIMEOUT`;
- `.turbo/` é ignorado pelo Git; não publique logs, caches ou builds.

`status` lê `.turbo/state.json` e não precisa repetir o comando em loop. A trava
`.turbo/run.lock` impede dois builds/testes TURBO simultâneos no mesmo checkout.

## Auditoria real deste ambiente

Medições registradas em [benchmarks.md](benchmarks.md). Fatos iniciais:

- 8 CPUs, cerca de 5,7 GiB de RAM (≈2,2–2,7 GiB disponíveis durante a auditoria);
- build `build-linblux/`, Release, gerador **Unix Makefiles**;
- `ninja`, `ccache`, `clangd`, `rg` e `ctags` ausentes;
- CMake/CTest 3.31.6, `xvfb-run` disponível, renderização por software;
- 35 testes CTest após a integração do TURBO; a suíte completa tem estimativa
  histórica de aproximadamente 14,5 minutos nesta máquina;
- testes gráficos e de UI concentram o custo; os unitários headless levam
  geralmente menos de 0,2 s cada.

## Organização estrutural

`organize --dry-run` audita somente leitura: arquivos gerados rastreados,
candidatos a duplicata, caminhos absolutos, módulos grandes, testes condicionais
e diretórios de raiz. Ele **não move nem exclui** arquivos. Migrações exigem
revisão humana, `git mv`, atualização de referências e validação separada.

## Testes do TURBO

```bash
python3 -m unittest discover -s tests/turbo -p 'test_*.py' -v
ctest --test-dir build-linblux -R '^linblux_turbo$' --output-on-failure
```

A matriz cobre seleção, labels, alterações, invalidação de cache, timeout e
processos filhos, códigos de saída, logs, truncamento, JSON, concorrência,
falhas de CMake/compilação, ausência de testes e auditoria somente leitura.
Detalhes em [testing.md](testing.md); decisões de desenho em
[architecture.md](architecture.md); números medidos em
[benchmarks.md](benchmarks.md).
