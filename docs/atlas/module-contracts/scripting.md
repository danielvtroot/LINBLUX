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

**API v1 LINBLUX:** `linblux.scene`, `linblux.events`, `linblux.time`,
`linblux.vec3`, contexto por script (`self`/`script`) e handles próprios com
propriedades minúsculas. Os bindings legados preservam seus nomes. Veja
[SCRIPTING_LUA](../../SCRIPTING_LUA.md) para contrato, cotas e exemplos executados
por `linblux_studio`. Listeners/tarefas de scripts removidos ou desabilitados e
escopos destruídos são cancelados nos pontos seguros do scheduler. Lua roda
antes de `PhysicsWorld::syncScene`, nunca durante o passo de Bullet.
Scripts clonados habilitados entram no scheduler; reativação inicia um contexto
novo, sem herdar listeners/tarefas antigos. `destroy` entrega uma única notificação
cancelável ao observador ainda existente, após a remoção do escopo.
`fixedUpdate` executa callbacks antes de cada passo, compartilhando a cota do
quadro. `contact` apenas enfileira dados coletados depois do passo; nenhum script
executa durante a enumeração de manifolds. Os pares são limitados a 1024 e a fila
de notificações respeita a política de tarefas do host.
`linblux.input` recebe amostras do frontend pelo RuntimeSession; consultas e eventos
de borda não fazem polling nativo nem consomem Escape/atalhos de saída. `ScriptInput`
é transitório, fora da cena serializada. Testes: linblux_studio (amostras injetadas)
e linblux_script_input_ui (teclado X11 real, gameplay e pausa).

**Limites:** não é um sandbox de segurança contra código nativo arbitrário;
não há multiplayer/rede. Coleta de lixo/limites são os do estado Lua e do
runtime, não um isolamento de processos.
