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
