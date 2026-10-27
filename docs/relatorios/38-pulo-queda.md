# 38 — Apresentação de pulo e queda — 27/10/2026

O estado físico/API legado Freefall continua significando descida no ar.
O clipe de queda agora exige velocidade vertical<-8, tempo no ar>0,3s,
perda de altura desde o ápice>4,5 unidades e ausência de chão próximo
(raio Bullet de meia altura da cápsula+max(3,meia altura)).

Pulos próximos do chão mantêm o clipe de pulo durante a descida; uma queda
significativa fica travada até aterrissar, evitando alternância de clipes.
Entrada de clipe e blend de0,12s existentes são reutilizados. Respawn e Stop
limpam o rastreador. Sem novos estados/clipes serializados ou mudança na API Lua.
Não é uma nova máquina completa JumpStart/JumpAir/Land com clipes próprios.

Fontes: Runtime.h/cpp; testes studio/IndependentFall/SignificantFallPresentation.
Testes a30/60/144FPS: pulos2/7/20, descida física, pouso, degrau2, queda30,
amostra do clipe de queda e respawn durante queda. Primeiro teste apontou3
falhas reais de flag após respawn: corrigido reset no local adequado.

Build Studio/Player/game/studio_tests128,13s; correção final20,39s.
Regressão studio+game+Player RifleDemo3/3 em21,920s, log
`.turbo/logs/significant-fall-final.log`. Sem captura específica de salto;
validação de pose é numérica, Player gráfico verifica integração existente.
FAST `20261027T211806Z-18220`:17/17, testes8,1s, build83,1s; cobertura
desconhecida. IMPACT será executado junto da recuperação do backend de música.
WIP de áudio preservado separado deste commit.
