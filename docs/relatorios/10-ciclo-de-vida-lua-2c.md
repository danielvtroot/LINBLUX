# Fase 2 — incremento 2C: ciclo de vida dos componentes Lua

Scripts clonados habilitados agora entram em execução. Desativar cancela tarefas
e listeners; reativar reinicia o contexto, inclusive no mesmo quadro, sem duplicar
callbacks. O evento start é direcionado ao script novo. Deslocar um script para
fora da árvore executável interrompe sua execução. ModuleScript preserva require.

O evento destroy requer escopo e entrega o ID removido a um observador ainda
existente. A conexão é removida depois da entrega; desconectar antes cancela a
notificação enfileirada. Nenhuma dessas operações executa Lua dentro de Bullet.

ATLAS localizou LuaRuntime e RuntimeSession. O impacto de LuaRuntime.cpp não
mapeou diretamente o teste; a inspeção dirigida confirmou linblux_studio. Foram
alterados LuaRuntime.cpp, studio_main.cpp, o guia Lua e o contrato de scripting.

Validação final: compilação pelo TURBO de Studio, Player e teste Studio em
37,02 s. FAST 20261025T080212Z-5393: 12/12 testes aprovados, zero falhas/timeouts,
11,9 s em testes, 22,5 s totais incluindo build incremental de 5,5 s. A regressão
verifica clonagem, ativação, reinício no mesmo quadro, uma única assinatura ativa,
destruição, cancelamento e preservação da cena de autoria. Não foi executado FULL
nem validação visual, pois este incremento altera o scheduler de runtime.

Alterações locais de outras tarefas foram preservadas. Próximo incremento:
passo fixo e eventos de contato reais; áudio, UI Lua e demais fases seguem pendentes.
