# Fase 2 — incremento 2D: passo fixo e contatos Bullet

Implementado physics_update antes dos passos de 1/60 s e collision_enter/exit
depois da coleta real de manifolds. Lua nunca executa durante a enumeração de
contatos ou a simulação Bullet. Eventos são enfileirados e canceláveis, filtrados
por objeto/grupo. Respawn encerra os contatos do avatar anterior. Forças contínuas
agora podem ser aplicadas por passo físico sem depender do FPS de apresentação.

Arquivos: Runtime.{h,cpp}, LuaRuntime.{h,cpp}, studio_main.cpp, guia Lua e contrato
de scripting. ATLAS localizou runtime/física, confirmou consumidores dos headers;
tests-for Runtime.cpp não mapeou teste diretamente. O impacto do header encontrou
os testes de game, experiências, defaults, spawn e studio, cobertos pelo FAST.
ATLAS também listou cópias em .turbo como referências: são ruído do índice,
não fontes compiladas; não foram alteradas.

TURBO FAST 20261025T080624Z-7885: 12/12, zero falhas/timeouts, build 40,4 s,
testes 9,3 s, total 50 s. Depois foi acrescentada a regressão de novo contato
após respawn: Studio/Player/teste recompilados em 13,03 s; regressão pelo supervisor
TURBO em 0,365 s, 1621 verificações, zero falhas. Timeout individual de 10 s.
Não houve FULL, captura visual ou nova bateria gráfica: este incremento não altera
a apresentação e foi exercitado com cena, scripts, controlador e Bullet reais.

Testado: 60 callbacks físicos por segundo a 30 e 120 FPS com mesma velocidade;
entrada/saída em sensor sem resposta sólida; callback que move ou destrói peça;
contato do jogador antes e depois de respawn; autoria intacta e runtime válido.

Limites explícitos: até 1024 pares, fila compartilhada com cotas Lua; excedentes
podem ser descartados, com diagnóstico de fila cheia. Não é detecção contínua de
regiões, nem inclui pares estático-estático. Navegação, entrada roteada, áudio,
UI Lua e as fases posteriores continuam pendentes. O guia documenta a API real.
