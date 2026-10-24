# Etapa 1 — segurança (24/10/2026)

- Branch original `fix/unified-3d-gizmos`, HEAD `0e088fd`, alterações locais
  preservadas em `eb19e90`; branches antigas e histórico preservados em bundle.
- Remoto anterior preservado como `legacy`; origin configurado para o oficial.
- Histórico inicial remoto unido por merge `5c68449`, push confirmado para main.
- Clone independente em `/tmp/linblux-restore-check` usado para verificar restauração.
- .gitignore ampliado; nenhuma fonte/dependência necessária excluída. Varredura
  por padrões de chaves privadas, tokens GitHub e AWS sem achados no conteúdo próprio.
- Vídeo oficial de 2,7 MB: abaixo dos limites usuais; Git LFS indisponível e
  desnecessário nesse tamanho. midia é rastreado, duplicata na raiz é ignorada.
- Validação inicial: build antigo falha pois CMakeCache referencia o diretório
  removido `/home/labubu/projeto/loblox`. Será criado build novo. Os PNGs antigos
  removidos pelo usuário também precisam de migração dos caminhos no CMake.
- Limitação: arquivos pessoais ignorados e /tmp não têm proteção remota.
- Próxima etapa: identidade, mídia oficial e compilação a partir de configuração limpa.
