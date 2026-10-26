---
description: Valida uma alteração do LINBLUX com índice, testes ATLAS, build, testes focados e documentação.
---

Valide: $ARGUMENTS

1. `python3 tools/atlas/atlas.py update`
2. `python3 tools/atlas/atlas.py verify`
3. `python3 tools/atlas/atlas.py doctor`
4. `python3 -m unittest discover -s tests/atlas -p 'test_*.py' -v`
5. `ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure`
6. `cmake --build build-linblux -j2`
7. Rode os testes focados headless indicados por `impact`/`tests-for`.
8. Para UI/render, use `python3 tools/turbo/turbo.py test` com `--budget` e
   `--per-test-timeout`; nunca invoque `ctest` gráfico diretamente.
9. Se correção, build incremental, regressão específica, integração relevante e
   teste manual já estiverem satisfeitos, encerre e declare o que ficou pendente.
   Não busque aprovação FULL decorativa.
10. Atualize contratos/documentação se o comportamento público mudou.
11. Rode `atlas.py docs` se o inventário mudou e `atlas.py update` ao final.

Entregue comandos, resultados, falhas e limitações. Não invente números.
