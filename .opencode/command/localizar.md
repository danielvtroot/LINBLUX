---
description: Localiza módulos, arquivos e símbolos para um sintoma ou termo usando o ATLAS local.
---

Use o ATLAS antes de varrer o repositório.

1. Atualize o índice: `python3 tools/atlas/atlas.py update`.
2. Localize: `python3 tools/atlas/atlas.py locate "$ARGUMENTS" --limit 8`.
3. Abra os símbolos mais promissores: `python3 tools/atlas/atlas.py symbol <Nome>`.
4. Confirme no código real.

Leia `confidence` e `score_breakdown`. Candidatos `inferred=true` vêm do
glossário, não de correspondência textual. C++ é regex/medium; não há clangd.
Entregue módulos candidatos, arquivos:linhas, evidências e próximos comandos.
