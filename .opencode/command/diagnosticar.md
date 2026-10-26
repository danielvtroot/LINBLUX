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
7. Compile e rode testes focados; para UI/Xvfb, use o TURBO com `--budget` e
   `--per-test-timeout`, nunca `ctest` gráfico direto.
8. Encerre quando a regressão específica e a integração relevante estiverem
   cobertas; registre pendências em vez de iniciar uma bateria pesada sem risco.
9. Rode `atlas.py update`, `verify` e `doctor`.

Separe fato, hipótese e ausência. Não use reset/clean destrutivo. Informe
comandos, resultados, limitações e o que não foi testado.
