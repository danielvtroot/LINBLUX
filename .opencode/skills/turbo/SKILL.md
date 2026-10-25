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

3. Para release, CMake, serialização, header amplamente incluído ou cobertura
   desconhecida, com decisão explícita:

```bash
python3 tools/turbo/turbo.py test --mode full --no-cache
```

4. Se precisar entender a seleção sem executar:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed --plan
```

5. Para ambiente, inventário ou medição:

```bash
python3 tools/turbo/turbo.py doctor
python3 tools/turbo/turbo.py inventory
python3 tools/turbo/turbo.py benchmark --run-fast
```

## Como interpretar

- `PASSED`: o escopo do modo passou agora; FAST/IMPACT não são FULL.
- `PASSED_PARTIAL`: passou no escopo, mas a cobertura é desconhecida/limitada.
  Não declare entrega validada; amplie com IMPACT/FULL.
- `REUSED`: resultado anterior aceito; nada foi executado agora. Use
  `--no-cache` se precisar de execução real.
- `FAILED`, `BUILD_FAILED`, `CONFIG_FAILED`: corrija a primeira causa; preserve o
  log e o código de saída.
- `TIMEOUT`: o grupo de processos foi encerrado; não trate como sucesso.
- `DOCS_OK`/`ASSET_OK`: apenas documentação/JSON validado; não alegue teste da
  engine.
- Saída 2 = cobertura desconhecida/parcial ou nenhum teste; 124 = timeout; 1 =
  falha.

## Evidências e limites

- O plano tem `reasons` por teste e `filtered` para o que não foi executado.
- CTest fornece labels, timeouts, `RESOURCE_LOCK` e `PROCESSORS`; ATLAS fornece
  targets/includes com confiança textual `medium` (sem clangd/ctags).
- FAST exclui gráficos/lentos. IMPACT limita gráficos por padrão a 4; use
  `--max-graphics N` para ampliar e registre a cobertura parcial.
- Logs completos ficam em `.turbo/logs/<run-id>/`; não despeje o log inteiro no
  diálogo. Leia somente o excerpt/trecho relevante.
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
