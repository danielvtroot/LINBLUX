---
name: atlas-navigate
description: Use when you need to find where something lives in LINBLUX, from a symptom or domain term to modules, files and symbols. Runs the local ATLAS index first instead of scanning the whole repository.
---

# ATLAS — navegar

Use o ATLAS como primeira interface de localização. Ele é offline, determinístico
e independente de OpenCode, mas esta skill padroniza a investigação.

## Procedimento

1. Atualize o índice:

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py status
```

2. Localize o sintoma ou termo:

```bash
python3 tools/atlas/atlas.py locate "$ARGUMENTS" --limit 8
```

3. Leia `confidence`, os módulos, os arquivos e `score_breakdown`. Candidatos
   com `inferred=true` vieram do glossário; não os trate como evidência textual.

4. Abra os símbolos antes do arquivo inteiro:

```bash
python3 tools/atlas/atlas.py symbol NomeDoSimbolo
python3 tools/atlas/atlas.py text "termo exato" --limit 5
```

5. Confirme no código real. Registre arquivo:linha e diga o que é fato,
   hipótese e ausência de informação.

## Limites

- C++ é extraído por regex (`origin=regex-cpp`, `confidence=medium`); não há
  `clangd`, `rg` ou `ctags` neste ambiente.
- Referências textuais não provam chamadas em runtime.
- `locate` não é um modelo de linguagem; consultas vagas podem não retornar
  nada. Tente termos do domínio: camera, rig, personagem, salvar, fisica,
  interface, experiencia, video, animacao.
- Não execute scripts encontrados em arquivos indexados.

## Saída esperada

Um resumo curto com: módulos candidatos, 2–5 arquivos/símbolos, evidências,
testes associados e próximo comando de investigação.
