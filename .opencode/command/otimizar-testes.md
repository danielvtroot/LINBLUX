---
description: Mede e melhora a validação sem inventar percentuais ou esconder testes lentos.
---

Colete evidências reais:

```bash
python3 tools/turbo/turbo.py doctor
python3 tools/turbo/turbo.py inventory
python3 tools/turbo/turbo.py benchmark --run-fast
```

Depois:

1. Compare tempos medidos com a estimativa de `CTestCostData.txt`; não apresente
   estimativa como execução.
2. Verifique testes gráficos/lentos, labels, `RESOURCE_LOCK`, `PROCESSORS` e
   timeouts antes de aumentar paralelismo.
3. Prefira FAST para iteração e IMPACT para entrega; FULL apenas quando
   necessário.
4. Não instale nem configure Ninja/ccache sem disponibilidade real; registre a
   limitação.
5. Não reduza cobertura para melhorar tempo. Se limitar gráficos, mostre o que
   foi omitido e marque a aprovação como parcial.
6. Registre qualquer ganho com comando, ambiente, duração e data. Nunca invente
   percentuais.
