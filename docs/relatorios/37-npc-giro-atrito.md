# 37 — NPC: giro completo e atrito do controlador — 27/10/2026

Implementação real em `Npc.cpp` e `RuntimeSession::partMotion`; sem mudar cenas,
propriedades autorais ou formato. Testes em `NpcHeadingRegression` (studio).

Reprodução antes da correção:11 falhas. O heading135° alinhava0,7275 e180°
alinhava0,0292, pois Euler Y (asin) perdia o quadrante e oscilava perto de90°.
Agora o heading vem de atan2 do vetor frontal, respeitando o limite turnSpeed.
Reprodução após corrigir somente giro:8 falhas, todas de deslocamento. Em todas
as direções, velocidade2 percorria1,9059 unidade em2s no piso de atrito0,8.

O NPC usa corpo Bullet dinâmico conduzido por velocidade, como o jogador,
não um bloco solto: atrito de contato zero enquanto recebe velocidade horizontal
do controlador, restaurado ao parar. Densidade, massa, damping e propriedades
da Part continuam intactos. `halt` também restaura quando a velocidade já é zero.
Parts comuns não recebem esta política. Não é navegação/movimento universal.

Testes verificam8 direções, giro por passo <= turnSpeed×dt, deslocamento3,8–4,1
unidades em2s com speed2, parada, rigidez vertical, material preservado e
invariantes do runtime. Rodam em Bullet real, não só matemática de quaternions.

Evidências:
- Antes: `.turbo/logs/npc-heading-before.log`,11 falhas; heading corrigido:
  `npc-heading-only.log`,8 falhas com medição do arrasto.
- Depois: studio+Player RifleDemo2/2 em19,792s. Player real inclui patrulha/clip,
  coleta/equipar/disparar/recarga/drop/recoleta. `npc-movement-final.log`.
- Build Studio/Player/studio_tests:35,37s. FAST `20261027T211050Z-12170`:
  17/17, testes12,1s, build25,1s; cobertura desconhecida.
- IMPACT deste incremento registrado no checkpoint ao concluir.

As mudanças locais de música/áudio preexistentes não integram o commit: somente
o hunk de partMotion é indexado. Sem claims de colisão arbitrária perfeita,
massas em SI, física de multidões ou fechamento integral da missão.
