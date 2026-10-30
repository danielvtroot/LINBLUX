# Compatibilidade LINBLUX

A interface e os targets novos usam LINBLUX. O namespace C++ `loblox`, a
metatable privada Lua `Loblox.Handle`, a assinatura JSON `loblox-project`, o
manifesto `loblox-experience`, o pacote `LOEXP` e a extensão `.loblox.json` são
mantidos temporariamente para preservar projetos, plugins e testes existentes.
Esses identificadores de formato não identificam o produto na interface.

O resolvedor aceita os caminhos antigos `loblox.png` e `rosto1.png` como aliases
para a logo oficial e para o visor original do LINBLUX. Imagens incorporadas em
experiências antigas continuam sendo carregadas a partir do próprio projeto.
A pasta de dados nova é `linblux`; quando somente a biblioteca legada `loblox`
existe, ela é reutilizada, sem apagar nem sobrescrever experiências.
Variáveis `LOBLOX_*` de diagnóstico ainda são aceitas por compatibilidade.

A troca de marca não altera nomes de serviços, propriedades nem APIs Lua.
Os rigging e os clipes do formato R6 permanecem legíveis. Suporte a malhas e
esqueletos arbitrários exige uma evolução do formato e do runtime; não se deve
renomear o tipo serializado e declarar essa evolução pronta.

## Plataforma e versão Alpha

A distribuição é identificada como LINBLUX Alpha 0.1.0. O build oficial é
`build-linblux/`. Linux x86_64 é o ambiente validado nesta missão; testes gráficos
usam Xvfb/llvmpipe. Windows, macOS e outras distribuições Linux não foram validados.
O pacote depende de glibc/libstdc++, OpenGL/X11 e bibliotecas listadas no manifesto.
Configurações desconhecidas legadas não são convertidas em mudança de objetos.
A remoção da grade visual não migra cenas nem altera o Snap.

Novas Parts usam nomes compactos sem espaços; a renomeação manual recusa espaços
Unicode e controles, mantendo o limite de 128 bytes. A criação compacta palavras
e a duplicação gera um nome único. A carga por ProjectIO preserva literalmente
nomes legados, inclusive espaços, sem alterar IDs. Não há migração automática
de cenas nem substituição de strings Lua. Código que cria novas peças e depois
as busca por nome deve usar o nome retornado/handle, em vez de pressupor espaços.

Parts em ServerStorage/ReplicatedStorage exigem esta revisão de armazenamento ou
posterior. Builds anteriores podem recusar essa hierarquia; preserve uma cópia
antes de alternar versões. A assinatura dos formatos e os IDs não mudaram.
Cenas antigas continuam carregando sem conversão automática de nomes ou serviços.
