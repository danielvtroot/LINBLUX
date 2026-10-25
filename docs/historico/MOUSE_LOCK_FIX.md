> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Mouse Lock — 2026-10-23

Base: `7f315c4`. Escopo: câmera/cursor de gameplay. EditorCamera, aparência
do menu/Health, Lua, rig, animações, mundo e assets originais preservados.

Causas reproduzidas antes da correção:

- Yaw da câmera por frame, yaw do R6 apenas no tick Bullet de 60 Hz. O teste
  com input a 240 Hz falhava em frames sem passo físico.
- rlImGui podia chamar ShowCursor ao mudar o formato do ponteiro, removendo
  o modo relativo GLFW enquanto gameplayCapture continuava true. Teste real
  forçando cursor Hand reproduziu IsCursorHidden=false durante o lock.
- Captura decidida antes do toggle: ON/OFF atrasava um frame. O ícone dependia
  apenas do pedido de lock, não da captura real. Ambos falhavam no teste X11.

Contrato atual:

- Runtime consome Shift uma vez; captura segue o resultado no mesmo frame.
  DisableCursor mantém deltas relativos, sem RMB. A captura impede alterações
  de cursor pelo backend ImGui; soltá-la restaura flags, ponteiro e posição.
  Perda de foco libera sem warp explícito para a janela inativa.
- GameplayCursorState separa captura, visual (System/Hidden/MouseLock) e
  posição virtual em coordenadas lógicas da viewport. RML desenha lock.png
  apenas com captura efetiva e foco; não há reticle/HUD nem objeto 3D para isso.
- PNG original: 1600x1600 RGBA; bounds alfa (148,152)-(1436,1470). O cache
  existente corta as margens e preserva proporção, mipmaps/alfa/MSAA, em 28 px.
- Posição renderizada entre ticks é compartilhada por avatar e câmera. A
  interpolação tem uma amostra de física de atraso (até 16,7 ms), sem atrasar
  yaw/pitch. Pausa conserva a fase; respawn reinicializa ambas as amostras.
- Durante lock, o pivô segue essa raiz com altura neutra do rig, sem pose da
  cabeça e sem um segundo filtro de posição. Só a entrada desde a câmera livre
  tem correção transitória. Yaw do corpo tem um único escritor por frame;
  AutoRotate e Follow não competem com ele. Pitch nunca inclina o corpo.
- Offset lateral padrão 1,75 unidades no eixo X da câmera, aplicado uma vez.
  `PlayerCamera::setMouseLockShoulderOffset(float)` configura -5..5 na sessão
  (API C++, sem novo controle/persistência de configuração). Primeira pessoa
  zera o offset. Sweep lateral + sweep para trás preservam oclusão por paredes.
- Esc/foco suspendem captura/visual; Resume conserva o pedido e descarta Shift
  mantido na UI. Resize/fullscreen descartam o delta da mudança de geometria.
  Stop limpa o lock e devolve a câmera do Studio.

Validação: teste headless com 360 frames alternando 240/144/60/30 Hz, giro
completo, WASD, pulo/queda, offset, parede lateral, pausa, respawn e FP/TP.
Teste `loblox_mouse_lock`: eventos X11 reais sem RMB, interferência de cursor
ImGui, sequência PNG/CSV, Esc/Resume, reset, fullscreen, foco e Stop.
Primeira sequência: 72 frames, giro 371,28 graus, cursor (639;457,5) constante,
erro yaw câmera/corpo 0 no CSV. Artefatos locais não versionados:
`build/mouse-lock-validation/` (PNG, CSV e MP4 com timestamps reais).
Gravação por software é lenta; não comprova fluidez no notebook físico.

Regressão final: 21/21 CTest aprovados em uma execução completa (`-j2`,
551 s com llvmpipe), incluindo integração RML de 1050 checks, Mouse Lock de
644 checks e gameplay headless de 2862 checks. `git diff --check` aprovado.
Build principal atualizado pelo CMake. Comando de retomada:
`cmake --build build --target loblox -j2 && ctest --test-dir build --output-on-failure`.
Smoke do próprio `build/loblox`: Play, Shift, mouse sem RMB, W/Space,
Esc/Resume, desligar Shift e Stop. Capturas `main-*.png` no mesmo diretório
de evidências, executadas com projeto/configuração isolados.
Pendente: sensação tátil, Alt+Tab do compositor e DPI no Sway/Vega 8 real.

Referências consultadas (implementação C++ própria):

- [UserInputService / MouseBehavior / GetMouseDelta](https://create.roblox.com/docs/reference/engine/classes/UserInputService)
- [StarterPlayer / EnableMouseLockOption](https://create.roblox.com/docs/reference/engine/classes/StarterPlayer)
- [Mouse Lock clássico e offset](https://devforum.roblox.com/t/custom-center-locked-mouse-camera-control-toggle/205323)
- [Discussão de jitter e CameraOffset](https://devforum.roblox.com/t/camera-jitter-while-using-cameraoffset-shiftlock/3194268)

As páginas do fórum bloquearam a leitura HTML automatizada; os posts públicos
foram consultados pelo endpoint JSON correspondente. Nenhum código foi copiado.
