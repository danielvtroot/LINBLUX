# Contratos de módulo

Contratos manuais complementam o inventário gerado. Eles registram
responsabilidade, entradas/saídas, invariantes, dependências, ciclo de vida,
testes e limites que o ATLAS não consegue inferir com segurança.

Use estes documentos antes de alterar APIs públicas, serialização, física, Lua
ou UI compartilhada. O inventário automático fica em
`docs/atlas/generated/inventory.md` e **não** substitui estes contratos.

Módulos cobertos: `app`, `core`, `editor`, `experiences`, `game`, `media`,
`objects`, `physics`, `player`, `rendering`, `scene`, `scripting`,
`serialization`, `ui`.

Regras:

- o contrato descreve o comportamento atual, não uma aspiração;
- hipóteses e limitações devem ser explícitas;
- mudanças de contrato exigem atualização do documento e dos testes;
- o ATLAS pode apontar consumidores, mas não prova chamadas em runtime;
- não copie inventários gigantes para cá; mantenha o texto navegável.

O `doctor` acusa módulos sem contrato. O ATLAS não valida semanticamente o
conteúdo dos contratos.
