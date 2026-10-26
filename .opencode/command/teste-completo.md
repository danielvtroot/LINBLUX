---
description: Executa a suíte completa somente quando há decisão explícita, mudança estrutural ou preparação de release.
---

Antes de executar, confirme que FULL é realmente necessário: release,
serialização, CMake, header amplamente incluído ou cobertura desconhecida.

```bash
python3 tools/turbo/turbo.py test --mode full --no-cache --budget 1800 --per-test-timeout 180
```

A suíte completa foi estimada em aproximadamente 14,5 minutos nesta máquina sem
GPU; o teste pode demorar mais sob carga. O comando tem timeout por teste e
orçamento global supervisionado. Não execute `ctest` gráfico diretamente.

1. Não interrompa um teste gráfico sem verificar o estado/log; use `status`.
2. Preserve todos os resultados: falhas, timeouts, orçamento esgotado e testes
   não executados não podem ser convertidos em aprovação.
3. Em falha, mostre apenas o primeiro erro relevante e o caminho do log.
4. Ao final, informe estado real, contagem executada/aprovada, pendentes,
   duração e limitações. Se o FULL não foi executado, diga isso explicitamente.
