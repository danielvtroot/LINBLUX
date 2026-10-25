---
description: Calcula consumidores, testes e riscos de alterar um arquivo ou símbolo do LINBLUX.
---

Alvo: $ARGUMENTS

1. `python3 tools/atlas/atlas.py update`
2. `python3 tools/atlas/atlas.py impact "$ARGUMENTS" --depth 2 --limit 30`
3. `python3 tools/atlas/atlas.py tests-for "$ARGUMENTS"`
4. Leia `docs/atlas/module-contracts/<módulo>.md`.
5. Verifique consumidores que o grafo não vê: callbacks, virtuais, Lua, eventos,
   macros e dados persistidos.
6. Proponha o plano mínimo de compilação/testes.

Diga claramente que includes/CMake são estruturais e não provam chamadas em
runtime. Se o BFS atingir o limite, sinalize que o resultado é parcial.
