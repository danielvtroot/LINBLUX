---
name: turbo
description: Use for fast, impact or full validation in LINBLUX, build selection, test timeouts, logs, cache reuse, benchmark or structural audit with the ATLAS TURBO.
---

# ATLAS TURBO

Use o TURBO como executor de validação. Ele lê Git, CMake, CTest e ATLAS; não é
uma autoridade semântica sobre C++ e não substitui os testes da engine.

## Fluxo

1. Para uma alteração local:

```bash
python3 tools/turbo/turbo.py test --mode fast --changed
```

2. Antes da entrega normal:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed
```

3. Para limitar explicitamente uma validação maior:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed --budget 300 --per-test-timeout 180
```

4. Para release, CMake, serialização, header amplamente incluído ou cobertura
   desconhecida, com decisão explícita:

```bash
python3 tools/turbo/turbo.py test --mode full --no-cache
```

5. Se precisar entender a seleção sem executar:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed --plan
```

6. Para ambiente, inventário ou medição:

```bash
python3 tools/turbo/turbo.py doctor
python3 tools/turbo/turbo.py inventory
python3 tools/turbo/turbo.py benchmark --run-fast
```

## Supervisão obrigatória

Nunca execute `ctest` diretamente para testes gráficos (`graphics`, Xvfb,
`LIBGL_ALWAYS_SOFTWARE`, UI). O TURBO executa cada teste em uma invocação
supervisionada, com timeout individual, orçamento global e encerramento do grupo
de processos. Testes headless focados ainda podem usar `ctest -R`.

- O orçamento padrão é FAST=180s, IMPACT=300s, FULL=1800s; o timeout individual
  padrão é 180s para gráficos e 90s para headless.
- `--budget`/`--timeout` e `--per-test-timeout` permitem ajustar a tarefa.
- Testes obrigatórios (regressão direta) rodam antes dos adicionais; o que não
  couber no orçamento fica pendente e **não** é aprovado.
- Se um teste travar, o processo e seus filhos são encerrados. Não reinicie a
  bateria inteira; consulte o log do teste interrompido.

## Encerramento inteligente

Quando correção, build incremental, regressão específica, integração relevante e
teste manual já estiverem satisfeitos, avalie a cobertura e encerre. Não inicie
uma nova bateria pesada apenas para obter `PASSED` total. Se um teste adicional
for necessário por risco real, justifique e execute com orçamento.

## Como interpretar

- `PASSED`: o escopo do modo passou agora; FAST/IMPACT não são FULL.
- `PASSED_PARTIAL`: passou no escopo, mas a cobertura é desconhecida/limitada.
  Não declare entrega validada; amplie com IMPACT/FULL.
- `BUDGET_EXHAUSTED`: o limite global encerrou a execução; há testes pendentes.
  Não declare aprovação; execute apenas os pendentes se forem necessários ou
  registre a limitação.
- `REUSED`: resultado anterior aceito; nada foi executado agora. Use
  `--no-cache` se precisar de execução real.
- `FAILED`, `BUILD_FAILED`, `CONFIG_FAILED`: corrija a primeira causa; preserve o
  log e o código de saída.
- `TIMEOUT`: o limite individual encerrou o teste/grupo; não trate como sucesso.
- `DOCS_OK`/`ASSET_OK`: apenas documentação/JSON validado; não alegue teste da
  engine.
- Saída 2 = cobertura desconhecida/parcial, orçamento esgotado ou nenhum teste;
  124 = timeout individual; 1 = falha.

## Evidências e limites

- O plano tem `reasons`, prioridade `required`/`recommended` e `filtered` para o
  que não foi executado.
- CTest fornece labels, timeouts, `RESOURCE_LOCK` e `PROCESSORS`; ATLAS fornece
  targets/includes com confiança textual `medium` (sem clangd/ctags).
- FAST exclui gráficos/lentos. IMPACT limita gráficos por padrão a 4; use
  `--max-graphics N` para ampliar e registre a cobertura parcial.
- Logs completos ficam em `.turbo/logs/<run-id>/tests/<teste>/tests.log`; o
  índice `tests.log` lista concluídos, interrompidos e pendentes. Não despeje o
  log inteiro no diálogo. Leia somente o excerpt/trecho relevante.
- `status` consulta o estado sem polling; se outro TURBO estiver ativo, espere ou
  use o resultado existente.
- `.turbo/` é reconstruível e ignorado pelo Git. Nunca adicione builds, caches,
  logs ou binários ao commit.
- `organize --dry-run` é somente leitura; `--apply` é recusado. Não mova arquivos
  em massa.
- Não use `git reset --hard`, `git clean -fd` ou push forçado.
- Sem GPU física, testes gráficos usam Xvfb/software; sem Ninja/ccache, não
  invente ganhos dessas ferramentas.

## Testes do próprio TURBO

```bash
python3 -m unittest discover -s tests/turbo -p 'test_*.py' -v
ctest --test-dir build-linblux -R '^linblux_turbo$' --output-on-failure
```

Ao final de uma alteração de código, atualize o ATLAS e relate o que foi
realmente executado, o que foi reutilizado e o que permanece sem cobertura.
