# Fase 1 — API Lua LINBLUX v1 (25/10/2026)

Implementação real: namespace linblux, objetos part/group/model, criação validada,
consulta por nome/ID, transformações, material, cor, transparência, atributos,
hierarquia, clone/exclusão; eventos próprios com conexão/desconexão/escopo e
ciclo start/update/stop; tarefas/espera/timers; cotas configuráveis pelo host.
Scripts vinculados recebem contexto próprio e os bindings antigos permanecem.

ATLAS localizou LuaRuntime/Runtime/Scene. Não mapeou diretamente os testes de
LuaRuntime.cpp: busca direcionada identificou linblux_studio. Essa limitação foi
registrada no contrato. Não houve varredura global nem alteração do formato salvo.

Testes: FAST inicial aprovou 11/12 e encontrou uma regressão de entrega de morte
antes de respawn. Corrigida a distinção entre sinais legados já emitidos e novas
conexões canceláveis; regressão específica passou pelo supervisor TURBO em 0,82 s.
IMPACT aprovou 13/13 selecionados em 24,7 s (17,3 s de testes), sem falhas/timeouts,
com limite global de 120 s e individual de 45 s. Cobertura foi marcada PARCIAL:
apenas uma integração gráfica foi selecionada; outras onze ficaram excluídas.
Não é aprovação FULL. A suíte Studio registrou 1555 verificações e zero falhas,
incluindo os exemplos Lua originais executados e recarregados do disco.

As verificações incluem criação, alterações, eventos, timers, contexto por peça,
remoção de listeners e tarefas órfãs, cleanup, erro com linha/traceback, recusa de
bibliotecas/caminhos do sistema, loops infinitos, falta de memória, limites do
host, preservação da autoria e scripts legados. Studio e Player compilaram.

Backup: status do AutoBackup foi consultado: monitor parado, nenhum checkpoint
próprio/push registrado. systemctl não está disponível. O estado prévio foi
preservado em .turbo/phase1-scripting-baseline: bundle verificado, patch binário,
estado Git e tar dos arquivos não rastreados. Esse backup local não protege
contra perda do computador. Código rastreado desta fase é protegido por commits
e push normal; alterações preexistentes de outras tarefas não foram incluídas.

Limites: não há inicialização automática de scripts clonados durante Play nem
callback público de destruição, colisão ou interação na API nova. Física avançada,
personagens, NPCs, áudio, assets e UI continuam nas fases seguintes. As cotas da
VM não representam isolamento de processos nem toda a memória/trabalho nativo.
API e exemplos documentados em docs/SCRIPTING_LUA.md.

Commit de implementação: 3fd2f03. Push normal para origin/main e hash remoto
confirmados. Compilação incremental final de Studio, Player e teste Studio:
8,87 s, aprovada. Documentação registrada em commit separado.
