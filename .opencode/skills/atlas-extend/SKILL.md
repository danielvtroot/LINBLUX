---
name: atlas-extend
description: Use when adding a module, command, parser, metric or integration to the LINBLUX ATLAS itself. Preserves the offline, deterministic, explainable and conservative design.
---

# ATLAS — estender

## Antes de mudar

1. Leia `docs/atlas/architecture.md`, `docs/atlas/json.md` e o contrato do
   módulo afetado.
2. Rode:

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py verify
python3 tools/atlas/atlas.py doctor
python3 -m unittest discover -s tests/atlas -p 'test_*.py' -v
```

3. Preserve o princípio central: o índice é cache; a fonte da verdade é o código,
   CMake, testes e contratos manuais.

## Regras de implementação

- Python stdlib + SQLite/FTS5; nada de dependência obrigatória nova.
- Toda relação incerta deve carregar `origin`/`confidence`; não alegue precisão
  semântica de C++ sem ferramenta semântica.
- Consultas SQL usam parâmetros e caminhos validados dentro da raiz.
- `update` deve ser transacional, incremental por SHA-256 e capaz de recuperar
  índice corrompido.
- Respeite `.atlas/` como cache descartável e gitignored.
- Mudanças de schema exigem migração/`INDEX_VERSION` e documentação.
- Adicione teste no fixture `tests/atlas/fixture/` para cada comportamento novo.
- Atualize `docs/atlas/json.md` se o envelope/campos mudarem.
- Rode `atlas.py docs` para regenerar o inventário e `atlas.py update` ao final.

## Critérios de aceite

- `python3 -m unittest discover -s tests/atlas -v` passa;
- `ctest -R '^linblux_atlas$'` passa;
- `verify` retorna OK;
- `doctor` não tem severidade alta;
- benchmark/eval são reais e não inventados;
- nenhuma alteração destrutiva de Git ou dados do usuário.

## Limitações atuais

Sem `clangd`, `rg` e `ctags`, a extração C++ é regex e textual. Não “conserte”
isso fingindo precisão; documente a limitação e mantenha confiança `medium`.
