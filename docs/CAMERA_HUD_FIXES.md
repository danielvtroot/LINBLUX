# Correções focadas — Studio camera / Health

- Base: `30134a2`; menu Esc/topbar preservados.
- Causa da câmera: posição derivada de target + offset orbital; RMB alterava
  os ângulos com target fixo em todos os casos.
- FPS padrão: compensa o look target ao girar, preservando o olho world.
  Focus explícito mantém o pivô fixo. Trocar modo não altera o transform.
  Seletor na barra da viewport; seleção não escolhe modo. F continua enquadrando.
- PlayerCamera/Runtime não foram modificados.
- Testes da primeira etapa: headless passou; input gráfico 273 verificações;
  RML focado 363 verificações, zero falhas. Olho fixo com tolerância 0,0001,
  seleção presente/ausente, Focus, WASD/pan, perda de foco e Play/Stop.
- Referência oficial consultada:
  https://devforum.roblox.com/t/studio-beta-updates-to-studio-camera-that-simplify-navigation/4751512/1
  (a página direta bloqueou JS; o conteúdo oficial estava disponível na busca).
- Health: cartão rotulado introduzido por `5538899`; a base `a10f2fb` tinha
  uma barra minimalista. Restauração focada dessa composição é a próxima etapa.
  `referencias/topbar.jpeg` foi reaberta e não mostra Health; o diretório
  complementar `referenciaspngs/` não está disponível nesta retomada.
