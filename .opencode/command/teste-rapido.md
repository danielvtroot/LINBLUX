---
description: Valida a alteração atual com FAST, preservando cobertura desconhecida e sem executar a suíte gráfica pesada.
---

Execute a validação rápida:

```bash
python3 tools/turbo/turbo.py test --mode fast --changed
```

Depois:

1. Leia apenas o resumo e o caminho do log impressos pelo TURBO.
2. Se `PASSED`, não trate como aprovação FULL: a próxima etapa normal é
   `/teste-impacto` antes da entrega.
3. Se `PASSED_PARTIAL`, `UNKNOWN` ou `NO_TESTS`, não declare sucesso. Mostre o
   plano (`test --mode fast --changed --plan`) e proponha IMPACT/FULL.
4. Se `FAILED`, `BUILD_FAILED`, `CONFIG_FAILED` ou `TIMEOUT`, abra somente o
   trecho relevante do log indicado e faça a menor correção. Repita o FAST uma
   vez; não faça polling de `status`.
5. Nunca execute FULL automaticamente por causa de uma mudança pequena.

Ao final, informe: estado real, testes executados, duração, cobertura e o que não
foi validado. Preserve arquivos e experiências do usuário.
