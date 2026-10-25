# Workflow: validar uma alteração

## Pré-requisitos

- Build configurado em `build-linblux/`.
- Índice ATLAS atualizado.
- Testes de UI podem exigir Xvfb; a suíte completa é lenta.

## Validação do índice

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py verify
python3 tools/atlas/atlas.py doctor
```

- `verify` precisa retornar `ok`.
- `doctor` não deve ter severidade `alta`; `media` exige justificativa.
- Avisos de includes de sistema, testes condicionais e símbolos homônimos são
  esperados em alguns casos.

## Testes do ATLAS

```bash
python3 -m unittest discover -s tests/atlas -p 'test_*.py' -v
ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure
```

## Build

```bash
cmake -S . -B build-linblux
cmake --build build-linblux -j2
```

Se o build existente estiver inconsistente, reconfigure; não apague árvores de
outros desenvolvedores sem autorização.

## Teste focado

```bash
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
ctest --test-dir build-linblux -R '^linblux_(rml_ui|editor_input)$' --output-on-failure
```

Escolha pelo `impact` e `tests-for`, não por palpite. Para renderização/UI, um
teste sob Xvfb pode demorar minutos; registre o tempo.

## Regressões e documentação

1. Rode os testes afetados e os de integração do módulo.
2. Confirme consumidores com `impact`.
3. Atualize contratos manuais se o comportamento público mudou.
4. Rode `atlas.py docs` se o inventário precisar refletir a mudança.
5. Rode `atlas.py update` ao final para deixar o índice coerente.

## Entrega

Inclua:

- resumo do comportamento;
- arquivos alterados;
- comandos de validação e resultados;
- falhas, limitações e testes não executados;
- se aplicável, medições de benchmark (sem inventar números).
