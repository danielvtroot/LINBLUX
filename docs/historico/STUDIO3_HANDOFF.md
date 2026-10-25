> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Studio 3 — continuidade verificada em 21/10/2026

## Atualização prioritária — código validado em 8e89c73

Checkpoints: 50e1b1c (pulo/exclusão de clips), 0f52f11 (Snap/Baseplate/HUD),
8e89c73 (toques rápidos no backend). Principal `build/loblox` atualizado;
Debug não foi recompilado nesta rodada. Nenhum push ou projeto pessoal alterado.

- Pulo: apoio Bullet na borda sem raio central; buffer consumido também após
  contato no mesmo passo; toque Space press/release entre frames preservado
  pela fila raylib→ImGui. Sem cooldown novo ou impulso duplicado. Regressão
  física reproduziu quatro falhas antes; toque rápido reproduziu outra.
  Normalização inicial atrasava texto: corrigida separando InputText de atalhos.
- Delete Animation: clip sem vínculo excluído; atribuído pede confirmação e
  limpa Game Config. Preview/timeline/seleção reinicializados, vazio usa pose
  neutra/fallback nativo; pode criar novo clip após excluir todos. Lua customizado
  não é reescrito. Jump/Fall permanecem distintos. Save/reload testados.
- Baseplate: excluível/editável como Part, defaults ainda ancorada/sólida;
  removidos studs, grade embutida e seus caches. Load não recria a base apagada.
- Snap ON/OFF + incrementos Move/Scale/Rotate na topbar, compartilhados por
  Parts/rigs. Quantização usa o deslocamento/ângulo desde o início do drag;
  OFF permite ajuste fino mesmo com incrementos configurados não nulos.
- Shift Lock sem banner; retículo/captura preservados. Health compacta no alto
  à direita da viewport, preenchimento suave, cor da vida real, oculta na pausa.

Release final: CTest 13/13 (154,48 s); Studio 1.433 checks, rig/Parts Snap 318,
Visual Studio 218, Game UI 212. Sway privado: rig 318 e Game UI 212 passaram,
mais reinícios reais. Cenários físicos incluem saltos consecutivos em plataformas
pequenas, 30/60 FPS, pouso+buffer no mesmo intervalo e rejeição de pulo no ar.
Comando: `ctest --test-dir build --output-on-failure -j2`.
Capturas inspecionadas em `build/priority-fixes-captures/`: aplicativo principal
F5/Play/Esc/Shift+F5, HUD 75/100 e 25/100, Snap e Baseplate lisa.
Log completo: `build/priority-fixes-full-validation.log` (artefatos não versionados).

Pendente: validação manual do obby no notebook físico; sanitizers não foram
reexecutados nesta rodada (falha Mesa anterior continua registrada abaixo).
Céu procedural, Lua, R6 e quatro rascunhos preexistentes foram preservados.
Fechar/reabrir o aplicativo: um processo anterior à compilação conserva código antigo.

## Estado e preservação

Branch `fix/unified-3d-gizmos`; entrada desta retomada: `7afd682`.
Código de produção validado: `617a1d5`; checkpoint seguinte contém somente
ajuste da automação Sway e documentação. A missão completa permanece parcial.
Backup do worktree misto: diretório externo `resume-TbA2zhIY`, com patch,
HEAD e fontes originais. Nenhum push, reset, clean ou projeto pessoal alterado.

As alterações locais preexistentes em `Instance.cpp`, `Instance.h`,
`Lighting.h` e `Scene.h` foram preservadas fora dos commits. Incluem rascunho
de Surface/Decal/Texture e skyMode sem integração completa. Não anunciar como
recurso funcional. PNGs, referências, assets e Testing/ também não foram
adicionados ao Git. Logo e rosto originais mantiveram seus SHA256.

## Causas confirmadas e correções

- Rig: combo usava índices incompatíveis com ToolMode; picking manipulava
  bodyProxy enquanto desenho usava jointProxy; rest pose aplicada duas vezes;
  composição de childAnchor sem inversa; escala ajustava attachments duas
  vezes; cancelamento sem snapshot correto. Agora tool_ é fonte única e o
  mesmo jointProxy serve desenho/picking. Solver bind/rest/pose correto,
  transação opcional com snapshot e cancelamento em Esc/tool/context/foco.
  Scale só em Edit Rig, 0,2..10, face oposta fixa; Move/Rotate também em Pose.
  Os keyframes existentes não são reescritos para compensar binds.
- Explorer: rig tem ID estável independente do nome, Rename/Duplicate/Delete
  reais; delete cancela drag antes de remover. Popup único verificado com
  Lighting; seu Properties específico continua antes do caso de serviço.
- Fall: asset novo vazio e independente; Jump somente ao subir, Fall ao
  descer; Fall vazio/ausente usa pose neutra. Game Config e Lua aceitam Fall.
  Exclusão de animação atribuída é recusada com instrução de reassociação.
  Não há recriação automática de CharacterAnimations.server.lua.
- Sky: quad DrawMesh com Material vazio e projeção sem direção da câmera
  falhavam no teste de pixels. Agora clip-space + raios/aspect/FOV reais,
  uniforms de nuvens/rotação enviados, fallback se shader falhar. Mantido
  céu procedural, sem retorno aos PNGs sky1. Ruído reduzido de 15 para 3
  camadas; recursos destruídos antes do contexto GL.
- Coyote/buffer: timers existiam, mas condição exigia chão e tecla no mesmo
  passo. Agora graça + buffer executam uma vez; tempos 100/150 ms preservados.
- Script Editor: whitespace do lexer, scroll em pixels, horizontal/caret,
  indentação, fonte/gutter e navegação por linha/busca corrigidos; Output
  não dereferencia script excluído. Logo real no cabeçalho/ícone e HUD,
  botão de menu integrado ao roteador Pause/Resume.
- Esc/paleta/docking do checkpoint anterior 2adcf45 preservados. Menu usa
  retângulo atual da viewport, dim restrito ao mundo, tabs e footer fixo.
- Fixture rig: cliques XTest absolutos falharam no Sway que desloca a janela.
  Coordenadas relativas ao cliente corrigem a automação, não mascaram gizmos.

## Checkpoints desta retomada

- 25e8704 — gizmos do rig e edição transacional de binds.
- a6c99fe — Fall independente e proteção de assets atribuídos.
- 2740ed2 — céu procedural e custo limitado de ruído.
- 3ffa4f1 — execução real de coyote time/jump buffer.
- 617a1d5 — ações de rig, navegação Lua e controles de Play com branding.

## Build e testes

Principal atualizado: `build/loblox` (Release).
Debug atualizado: `build/stabilization-debug/loblox`.
Comandos a partir da raiz:
```sh
cmake -E env TMPDIR="$PWD/build/compiler-tmp" cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
runuser -u labubu -- bash tests/run_sway_input.sh ./build/loblox_rig_input_tests ./build/loblox
```
No usuário comum, omitir runuser. Nunca enviar XTest à sessão pessoal.

Release completo: 13/13, 128,15 s. Debug completo: 13/13; última revisão
também passou rig/Studio UI focados. Contagens: rig 192, Studio UI 117,
Visual Studio 147, input 223, Game UI 205, Studio headless 1.393, Game 252.
Restart usa seis processos, incluindo R6/Walk editada/configurações salvos.
Logs arquivados em build/studio3-*-validation.log (não versionados).

Sway privado headless/pixman/XWayland: Studio UI 117 e rig 192 passaram,
cada fluxo seguido de quatro reinícios do aplicativo principal. Último rig:
`/tmp/loblox-sway-vfR5qF`, `/tmp/loblox-rig-input-Mgx8NI`.
A primeira rodada rig falhou por coordenadas da fixture; não foi omitida.

ASan/UBSan headless 3/3. Rig gráfico: 192 verificações funcionais passaram,
porém LeakSanitizer REPROVOU: 194.140 bytes/865 alocações, stacks em
libGLX_mesa.so.0. Sem supressões ou desativação de leak detection.
Não declarar sanitizer gráfico verde; investigar separadamente no driver/alvo.

## Evidência visual e desempenho

Nove referências locais abertas individualmente; classificação, matriz e
fontes oficiais em [LOBLOX_VISUAL_REFERENCES.md](LOBLOX_VISUAL_REFERENCES.md).
Capturas reais examinadas e copiadas para `build/validation-studio3/`:
application/{studio,play,pause}.png; rig/{Move,Rotate,Scale}.png;
gallery/ com Home/Avatar/Script, código, animação, frente/perspectiva R6,
HUD/Shift Lock e Esc Game/Settings em 1280/1366/1600/1920.
Menu contido/centralizado e rosto com proporção original verificados.
A cabeça autoral 2×1×1 foi preservada; aprovação estética do usuário pendente.

Última Release em llvmpipe concorrente: startup 1,243 s/RSS 132.632 kB;
idle mediana 57,08 ms; Play mediana 108,99 ms, p95 144,86, pico 152,6 MiB;
1.000 nós: 88,63 ms/frame, pico 152,5 MiB. Sway rig startup 0,975 s,
RSS 145.876 kB. Não representam GPU/HDD/DPI/3 GB do notebook nem prometem FPS.

## Pendências reais / próximo passo

Autocomplete e IDE completo; maior polimento responsivo da ribbon/timeline;
Decal/Texture filhos de primeira classe (atualmente atributos legados);
importação portátil completa e materiais/sombras avançados; rigs como
Instances reparentáveis; aprovação da cabeça e estética global. Colisão do
personagem é uma cápsula configurável, não seis colliders de membros.

Próximo passo: validar manualmente o principal no notebook: Right Arm →
Move XYZ → Rotate → Scale → Esc/cancel → Save/reabrir → Play/Walk/Jump/Fall →
Shift/Esc/Resume/Respawn/Stop. Coletar
`$XDG_STATE_HOME/loblox/loblox.jsonl` ou `~/.local/state/loblox/loblox.jsonl`.
Não chamar a missão inteira de concluída nem apagar os rascunhos preservados.
