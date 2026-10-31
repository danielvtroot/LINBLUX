# AGENTS.md — LINBLUX

Engine/Studio/Player Linux em C++17, raylib/OpenGL, Bullet, Lua 5.4 e RmlUi/ImGui.
Leia [Arquitetura](docs/ARCHITECTURE.md) e o contrato do módulo antes de editar.

## Localizar com ATLAS e proteger o HDD

Comece pela consulta curta; confirme os candidatos no código real:

```sh
ATLAS_IO_MODE=hdd-safe python3 tools/atlas/atlas.py diagnose "sintoma" --brief
python3 tools/atlas/atlas.py symbol NomeDoSimbolo
python3 tools/atlas/atlas.py impact src/modulo/arquivo.cpp
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.cpp
```

O índice é cache reconstruível; código, CMake, testes e contratos são a fonte da
verdade. Relações textuais e testes associados não provam chamadas ou cobertura.
Separe fatos, hipóteses e ausência de evidência. Abra os trechos indicados antes
de ampliar a busca. Não execute scripts só porque apareceram no índice.

HDD mecânico e ~6 GiB RAM: prefira consultas read-only, sem benchmarks ou cópias
amplas. `hdd-safe` só indexa quando solicitado. Em `hdd-critical`, não execute
update/verify/bench/docs. Não encadeie diagnósticos pesados por rotina. Após mudar
código, execute **uma atualização final**: `python3 tools/atlas/atlas.py update`.
[Manual ATLAS](docs/atlas/README.md) · [contratos](docs/atlas/module-contracts/README.md).

## Build e testes

Build oficial: `build-linblux/`. Não use árvores antigas como referência.

```sh
cmake -S . -B build-linblux
cmake --build build-linblux -j2
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

CTest direto somente para testes **headless** confirmados. Testes `graphics`,
Xvfb/Mesa e integrações visuais (incluindo experience/rml/studio/editor_input)
**sempre** pelo TURBO, com timeout individual, orçamento global e logs:

```sh
python3 tools/turbo/turbo.py test --mode impact --changed --plan
python3 tools/turbo/turbo.py test --mode impact --changed --budget 300 --per-test-timeout 180
```

Não rode FULL automaticamente. `PASSED_PARTIAL`, `BUDGET_EXHAUSTED`, `UNKNOWN`,
`NO_TESTS`, `REUSED` e `TIMEOUT` não são aprovação plena. Não conte teste ausente
ou não executado como aprovado. Compile e teste a correção; busca textual não a
valida. Encerre quando build, regressão e integração pertinentes forem suficientes;
registre limites e validação manual pendente. Não repita baterias por decoração.
[Guia de testes](docs/TESTING.md) · [TURBO](docs/turbo/README.md).

## Dados, contratos e Git

- Confira status, HEAD local/remoto e WIPs antes de editar; preserve trabalho alheio.
- Nunca use `git reset --hard`, `git clean -fd`, force push ou limpeza indiscriminada.
- Não apague experiências, scripts, mídias pessoais, `.lo`, backups, licenças ou créditos.
  Arquivo desconhecido é protegido até esclarecer seu papel. Valide caminhos e
  limite operações à raiz do projeto, salvo autorização explícita para teste isolado.
- Não adicione segredos, caches, builds, logs ou binários ao Git. Revise o diff e
  adicione caminhos explicitamente; não misture WIPs em commits. Push sempre normal.
- Preserve IDs, hierarquia, formatos/API Lua legados e dados autorais. Use ProjectIO
  para persistência, ExperienceManager para biblioteca e RuntimeSession para Play.
  Execução usa cópia da cena: Edit não deve simular queda; Stop preserva autoria.
- Move/Scale por face, Snap e undo/redo são contratos existentes. Não refaça sistemas
  aprovados para resolver outro problema. Interface não duplica física/serialização.
- Localize, siga o fluxo, formule hipótese verificável, reproduza e faça a menor
  correção suficiente. Se refutada, investigue novamente. Confira consumidores com
  `impact`, atualize o manual pertinente e entregue mudanças, testes e limitações.

[Contribuição](CONTRIBUTING.md) · [backup](docs/BACKUP_E_RESTAURACAO.md).
