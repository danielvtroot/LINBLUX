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
- Câmera: commit `58586cc`. Health: cartão rotulado introduzido por `5538899`;
  restaurada a composição minimalista da base `a10f2fb`, sem texto/cartão
  permanente. Mantidos runtime, suavização, três cores e MSAA 4×.
  `referencias/topbar.jpeg` foi reaberta e não mostra Health; o diretório
  complementar `referenciaspngs/` não está disponível nesta retomada.
- Resultado final: 18/18 CTest (excluindo a sequência RML completa); integração
  RML focada posterior: 377 checks, zero falhas, incluindo seletor FPS/Focus,
  RMB com/sem seleção, troca sem teleporte, modo/olho após Play/Stop e Health
  100/50/20. Testes gráficos de AA/nitidez passaram em ambas as resoluções.
- Capturas inspecionadas: `build/camera-hud-fixes/`. Menu/topbar novos não
  foram redesenhados. Validação física Sway/notebook continua pendente.
- Comandos: `cmake --build build --target loblox -j2`;
  `ctest --test-dir build --output-on-failure -E '^loblox_rml_ui$' -j2`;
  `LOBLOX_PLAYER_UI_ONLY=1 LOBLOX_PLAYER_UI_SIZE=1366 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a -s '-screen 0 1920x1080x24 -noreset' ./build/loblox_rml_ui_tests`.
