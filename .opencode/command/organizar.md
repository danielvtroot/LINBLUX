---
description: Audita a estrutura e possíveis duplicatas sem mover nem excluir arquivos.
---

Execute a auditoria somente leitura:

```bash
python3 tools/turbo/turbo.py organize --dry-run
```

Analise os achados com evidências: arquivos gerados rastreados, candidatos a
duplicata, caminhos absolutos, módulos grandes, testes condicionais e diretórios
de raiz.

Regras:

- Não execute `--apply`; a migração automática está desabilitada.
- Não faça movimentos em massa.
- Escolha no máximo um grupo pequeno por vez; confirme consumidores no ATLAS,
  CMake e testes antes de mover.
- Use `git mv` e valide compilação/testes depois.
- Preserve experiências, assets e alterações locais do usuário.

Entregue os achados, o que é apenas candidato e o que exige decisão humana.
