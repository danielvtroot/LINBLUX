---
name: atlas-impact
description: Use before changing a header, scene, serialization, physics, Lua or shared UI in LINBLUX. Computes structural consumers, related tests, risks and a minimal validation plan with the ATLAS.
---

# ATLAS — impacto

## Procedimento

1. Atualize o índice:

```bash
python3 tools/atlas/atlas.py update
```

2. Calcule o impacto reverso:

```bash
python3 tools/atlas/atlas.py impact "$ARGUMENTS" --depth 2 --limit 30
```

3. Liste testes associados:

```bash
python3 tools/atlas/atlas.py tests-for "$ARGUMENTS"
```

4. Leia o contrato do módulo em `docs/atlas/module-contracts/<módulo>.md`.

5. Confirme os consumidores reais que o grafo não vê: callbacks, virtuais,
   eventos, Lua, macros, dependências condicionais e dados persistidos.

6. Monte o plano mínimo:

- compilar os targets afetados;
- rodar testes diretos e um teste de integração por módulo;
- UI/render: teste sob Xvfb ou inspeção visual;
- dados persistidos: ciclo de vida/round-trip;
- atualizar índice e rodar `doctor`/`verify`.

## Limites

- Include e CMake são estruturais, não chamadas comprovadas.
- O BFS tem profundidade/limite; resultados podem ser parciais.
- Não execute testes automaticamente a partir do ATLAS; escolha e registre os
  comandos.
