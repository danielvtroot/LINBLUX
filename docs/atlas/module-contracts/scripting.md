# Módulo `scripting`

**Responsabilidade:** hospedar Lua 5.4 no runtime, expor bindings nativos da
cena/instâncias, agendar tarefas e entregar mensagens/eventos.

**Fonte:** `src/scripting/LuaRuntime.{h,cpp}`.

**Entradas:** scripts da cena, eventos de runtime (personagem, dano, conexão),
delta de tempo, mensagens e estado nativo validado.

**Saídas:** mutações permitidas na cena de runtime, mensagens, tarefas
agendadas, memória/contadores e erros de script com linha.

**APIs principais:** `start`, `stop`, `update`, `Allocate`, `Children`,
`Descendants`, `Find`, `GetService`, `GetAttribute`, `Connect`, `Emit`,
`Damage`, `Heal`, `LoadAsset`.

**Invariantes:** Lua executa apenas na cópia de runtime, nunca no mundo autoral;
handles são validados antes de acessar objetos; scripts não podem usar rede;
erros de script não devem corromper a cena; bindings nativos respeitam a
validação de `RuntimeSession`.

**Dependências:** Lua 5.4, `Scene`, `RuntimeSession`/game, `objects`,
serialização de scripts (Lua textual). Não depende de ImGui.

**Ciclo de vida e erros:** `start` cria estado/thread; `stop` libera; `update`
processa tarefas e callbacks; erros incluem linha e mensagem. `LuaRuntime`
pertence à sessão de runtime; não é reutilizado entre sessões.

**Testes:** `linblux_game`, `linblux_engine_default_lifecycle`, testes de
experiências/defaults que carregam scripts e testes de UI que executam Play.

**Como estender:** adicione bindings com validação de tipo/handle e teste Lua;
não exponha ponteiros de autoria. Mantenha mensagens e erros observáveis.

**Limites:** não é um sandbox de segurança contra código nativo arbitrário;
não há multiplayer/rede. Coleta de lixo/limites são os do estado Lua e do
runtime, não um isolamento de processos.
