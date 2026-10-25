# Testes do ATLAS TURBO

## Comandos

```bash
python3 -m unittest discover -s tests/turbo -p 'test_*.py' -v
ctest --test-dir build-linblux -R '^linblux_turbo$' --output-on-failure
```

O teste CTest `linblux_turbo` é headless, usa apenas a biblioteca padrão do
Python, tem timeout de 180 s e não depende de GPU, Xvfb ou rede. Ele não executa
a suíte gráfica da engine.

## Matriz de cobertura

| Área | O que é verificado |
| --- | --- |
| inventário | classificação por comando/ambiente/custo; smoke, vídeo, gráfico, timeout; build ausente não inventa testes |
| seleção | FAST exclui gráficos; IMPACT inclui integração associada; sem testes não vira aprovação; header amplia consumidores; CMake é estrutural |
| explicabilidade | `reasons` por teste, `filtered`, avisos, serialização JSON do plano |
| cache | rejeita falha, cobertura parcial, fingerprint diferente e teste volátil; `REUSED` não é `PASSED` |
| alterações | fingerprint muda com conteúdo do worktree em repositório Git temporário |
| supervisão | timeout mata o grupo e o processo filho; código de saída é preservado; excerpt é limitado |
| falhas de build | configuração CMake inválida e compilação C++ inválida em diretórios temporários, com log preservado |
| documentação/assets | link interno quebrado e JSON inválido falham; links válidos passam |
| organização | auditoria encontra duplicatas sem mover arquivos; `apply_supported` é falso |
| contrato CLI | envelope `--json` é JSON válido; `--help` documenta fast/impact/full |
| concorrência | trava de execução é adquirida e liberada |

A matriz cobre os cenários que o TURBO precisa distinguir para não aprovar algo
que não foi executado. Ela não substitui os testes da engine.

## Validação executada

Comandos reais usados na integração:

```bash
python3 -m unittest discover -s tests/turbo -p 'test_*.py'
# Ran 22 tests ... OK

ctest --test-dir build-linblux -R '^linblux_turbo$' --output-on-failure
# 100% tests passed, 0 tests failed out of 1
# ~5,9 s

python3 tools/turbo/turbo.py test --mode fast --changed --no-cache
# testes headless selecionados passaram; cobertura registrada no relatório

python3 tools/turbo/turbo.py test --mode impact --changed --no-cache
# 19 testes executados em ~58,5 s, incluindo 4 gráficos, todos passaram;
# estado parcial por limite de gráficos e CMake ainda não commitado
```

## O que os testes não cobrem

- A suíte gráfica completa e a suíte FULL não são executadas pelo teste do
  TURBO; isso é deliberado e deve ser feito em uma validação separada.
- Não há GPU física; testes gráficos usam Xvfb/renderização por software.
- Relações C++ são textuais/regex porque `clangd`, `rg` e `ctags` não estão
  disponíveis.
- Não há medição de `ccache`/Ninja: essas ferramentas não estão instaladas.
- Um teste não prova que o cache é seguro em outra arquitetura ou toolchain; o
  fingerprint inclui a assinatura do `CMakeCache.txt` e as versões disponíveis,
  mas limites do ambiente ainda devem ser considerados.

## Interpretação dos estados

- `PASSED_PARTIAL` exige ampliação de escopo; não é aprovação para entrega.
- `REUSED` significa que o resultado anterior foi aceito, não que processos
  foram executados agora.
- `TIMEOUT` nunca deve ser convertido em sucesso mesmo que o log termine após o
  encerramento do grupo.
- Uma saída 2 da CLI indica cobertura desconhecida/parcial ou ausência de testes;
  uma saída 124 indica timeout; uma saída 1 indica falha real.
