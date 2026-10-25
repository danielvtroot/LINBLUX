---
description: Executa a validação de impacto com consumidores, módulos e integrações associadas, dentro de um limite de custo explícito.
---

Execute:

```bash
python3 tools/turbo/turbo.py test --mode impact --changed
```

O IMPACT inclui FAST, consumidores de target/módulo e até 4 testes gráficos por
padrão. Para uma mudança que exija mais cobertura gráfica, use
`--max-graphics N`; para excluir gráficos, use `--max-graphics 0`. O relatório
mostra o que foi filtrado.

Procedimento:

1. Leia o plano e os avisos; confirme os consumidores reais que o grafo textual
   não vê (callbacks, Lua, dados persistidos e macros).
2. Se o estado for `PASSED`, registre que ainda é IMPACT, não FULL.
3. Se for `PASSED_PARTIAL`, explique o limite/cobertura e proponha FULL antes de
   entrega estrutural.
4. Se falhar ou der timeout, use o primeiro detalhe e o log completo. Não
   reexecute cegamente nem mascare o código de saída.
5. Atualize o ATLAS ao final de uma alteração de código:

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py verify
python3 tools/atlas/atlas.py doctor
```

Relate mudanças, evidências, testes, duração e limitações.
