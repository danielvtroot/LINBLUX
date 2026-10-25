---
name: atlas-debug
description: Use when diagnosing a LINBLUX bug or unexpected behavior. Guides a workflow from symptom localization through evidence, hypothesis, focused reproduction, minimal fix and validation using the ATLAS.
---

# ATLAS — depurar

## Fluxo

1. **Descreva o sintoma** em uma frase: observado, esperado e contexto
   (Studio, Player, Play, UI, físico, Lua, importação).

2. **Atualize e localize**:

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py locate "$ARGUMENTS" --limit 8
```

3. **Siga o fluxo provável**:

```bash
python3 tools/atlas/atlas.py trace "$ARGUMENTS" --limit 4
python3 tools/atlas/atlas.py symbol NomeDoSimbolo
```

4. **Encontre testes**:

```bash
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.h
python3 tools/atlas/atlas.py impact src/modulo/arquivo.h --depth 2
```

5. **Reproduza** com o teste mais próximo. Se não houver teste, crie um caso
   mínimo antes de alterar código.

6. **Formule uma hipótese verificável** e faça a menor correção suficiente.

7. **Valide**:

```bash
cmake --build build-linblux -j2
ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

Para UI/render, use o teste focado sob Xvfb; não rode a suíte completa
casualmente.

## Regras

- Não altere arquivos ao acaso quando a hipótese falhar; volte à investigação.
- Preserve alterações e experiências do usuário.
- Separe fato confirmado de inferência.
- Não use `git reset --hard`, `git clean -fd` ou push forçado.
- Ao final, rode `atlas.py update` e informe evidências, testes e limitações.
