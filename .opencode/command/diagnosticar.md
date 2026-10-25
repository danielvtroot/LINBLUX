---
description: Investiga um bug no LINBLUX com localização, hipótese, reprodução, correção mínima e validação.
---

Diagnostique: $ARGUMENTS

Siga o protocolo:

1. `python3 tools/atlas/atlas.py update`
2. `python3 tools/atlas/atlas.py locate "$ARGUMENTS" --limit 8`
3. `python3 tools/atlas/atlas.py trace "$ARGUMENTS" --limit 4`
4. `python3 tools/atlas/atlas.py tests-for <arquivo>` e `impact <arquivo>`
5. Reproduza com um teste focado; formule hipótese verificável.
6. Faça a menor correção suficiente, respeitando contratos.
7. Compile e rode testes focados; depois os afetados.
8. Rode `atlas.py update`, `verify` e `doctor`.

Separe fato, hipótese e ausência. Não use reset/clean destrutivo. Informe
comandos, resultados, limitações e o que não foi testado.
