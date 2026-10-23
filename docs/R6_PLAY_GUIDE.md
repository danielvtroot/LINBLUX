# R6, animação e Play local

Implementação própria, original, de um jogador local. Não há execução Luau,
compatibilidade de assets Roblox, multiplayer, ragdoll ou engine Roblox embutida.
Scripts Lua 5.4 foram adicionados posteriormente; veja [Studio 2](STUDIO2_GUIDE.md).

## Testar no notebook

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/loblox
```

1. Use **Game → Insert R6** (menu superior ou ribbon Game). O rig aparece no
   Explorer, com seis membros, raiz interna e joints; Walk/Idle/Jump são geradas uma
   única vez como assets do projeto. Novos rigs reutilizam os clips existentes.
2. No **Rig Editor**, selecione Left Arm e altere Body color/Material.
   **Edit Rig** altera dimensões/attachments/rest; **Pose Rig** altera apenas
   a prévia. Selecionar uma junta ativa Rotate, no pivô correspondente.
   Root pode ser posicionada numericamente; F enquadra o rig. Dimensões mudam
   os anchors proporcionalmente, mantendo as conexões.
3. Abra **Game → Animation Editor**. A janela abre abaixo da viewport e pode
   ser redimensionada/destacada como os outros painéis. Clip settings contém
   nome, duração, FPS, loop e prioridade. Role o painel quando faltar espaço.
4. Escolha `R6_Walk`, Pause/Stop preview, digite `0.25` em Exact time,
   selecione Right Shoulder e altere Rotation (degrees). Também pode rotacionar
   o gizmo na viewport. Pressione **Insert / Update key**, depois
   **Save Animation + Project**. Editar pose sem inserir chave não muda o clip.
5. Diamantes selecionam keys; arrastá-los altera o tempo com snap ao FPS.
   Esc cancela o retiming. O scrubber/faixas não movimentam a câmera/Parts.
   Copy/Paste copia a pose; Remove exclui a key; New/Duplicate criam assets.
   Use as Walk atribui o clip; Revert descarta explicitamente a cópia de edição.
6. **Game Settings** define personagem padrão, Walk/Idle, velocidade,
   aceleração, gravidade, JumpHeight, cápsula e câmera. Proporções visuais não
   recalculam automaticamente a cápsula: ajuste CollisionDimensions quando
   alterar muito o tamanho do personagem.
7. Clique na viewport e pressione **F5**, ou use Play no menu superior.
   WASD é relativo à câmera; Space pula; RMB arrasta a câmera; scroll faz zoom.
   O editor não recebe comandos enquanto o runtime está ativo.
8. **Esc** pausa e libera o cursor. Settings oferece Classic, Follow,
   First Person, sensibilidade, camera lock e zoom. Esc em Settings volta ao
   menu; Resume/Esc retoma; Reset Character retorna ao spawn sem resetar mapa.
   **Shift+F5** ou Stop Test retorna ao editor. Perda de foco pausa o jogo.
9. Salve com Ctrl+S, feche e reabra o JSON. Rigs, cores, offsets, keys,
   atribuições e defaults do jogo são preservados. Preferências alteradas no
   menu Esc duram apenas a sessão; defaults em Game Settings pertencem ao projeto.

Sem rig salvo, Play cria um R6 temporário apenas no runtime. O mapa autoral não
é modificado para inserir esse fallback. Rig inserido é um template de autoria,
não um NPC com física; o Play instancia somente o personagem padrão escolhido.
Sem spawn: usa X/Z=0 e o sólido encontrado verticalmente; sem chão, Y=20.
Com múltiplos spawns sólidos, escolhe o menor ID. Obstáculo sobre o spawn pode
elevar a posição inicial para sua superfície. Abaixo de Y=-500 ocorre respawn.

## Contratos internos

- `Rig`: Y-up, frente -Z. Seis caixas visíveis; RootJoint → Torso → Neck,
  Shoulders e Hips. Root não é uma caixa visível. Transform global é
  `parent * parentAnchor * restPose * animationPose * inverse(childAnchor)`.
  Nenhuma pose escreve acidentalmente em offsets de repouso.
- `AnimationClip`: versão 1, IDs string estáveis, R6, tracks por índice de junta,
  keys crescentes, quaternion unitário, SLERP, Linear/Smooth/Constant. Sparse
  loops interpolam entre última e primeira key. Walk original de 0,8 s tem
  contatos opostos e fechamento; Idle é neutra. Jump/Fall usam Jump editável
  quando atribuído, com fallback nativo para mapas antigos sem esse asset.
  Fall não tem atribuição independente. Não existe blending por camadas de
  prioridades Action; prioridade é metadado e a seleção atual é locomotora.
  Rotação de keyframe é interpolada de forma determinística: o destino é
  alinhado ao hemisfério da origem (`q` e `-q` são a mesma pose), o caso de
  exatamente 180° tem desempate fixo pelo eixo relativo e um campo opcional
  `arc` (graus, com sinal) preserva um giro intencional maior que 180° criado
  com o gizmo. Sem `arc`, vale o arco mais curto — dados antigos não mudam.
  O `arc` só é aplicado se terminar na orientação da próxima key; keys
  retimadas ou editadas à mão caem no arco curto.
- Reprodução no runtime: a pose visível é sempre reconstruída a partir de uma
  pose de entrada fixa e do sample do próprio clip (crossfade de 0,12 s).
  Após esse intervalo a pose é exatamente a dos keyframes, sem acumular o
  frame anterior. Cada decolagem reinicia o tempo do clip mesmo quando um pulo
  enfileirado sai do chão e volta no mesmo passo fixo, então pular repetidas
  vezes reproduz o mesmo caminho, na mesma direção.
- Animation Editor: os campos numéricos usam o par Euler ZYX (roll X, pitch Y,
  yaw Z) que é inverso de si mesmo, então exibir e redigitar um valor não
  altera a orientação salva. Arrastar uma junta mais de 180° a partir de uma
  pose com key e inserir a próxima key grava o sentido do giro.
- `RuntimeSession`: Editing → StartingPlay → Playing/Paused → StoppingPlay →
  Editing. Cópia profunda por JSON validado uma vez ao iniciar, Scene e Bullet
  próprios. O avatar usa uma cápsula dinâmica com CCD, rotação física travada,
  aceleração horizontal e impulso `sqrt(2 * gravity * jumpHeight)`.
  Passos fixos 60 Hz, no máximo 6 por frame, delta limitado a 0,1 s.
  Não há teleporte por frame, pernas físicas ou autocolisão.
- Chão: raycast vertical sem atingir o avatar, ignora CanCollide=false,
  normal.y >= 0,65; apenas em chão permite pulo. Não há auto-step de escadas,
  solver dedicado para plataformas móveis nem garantia para terrenos complexos.
  Use pulo para subir degraus. Walk depende do deslocamento real e chão, não
  apenas de W; velocidade de animação acompanha velocidade horizontal.
- PlayerCamera separada de EditorCamera. Zoom e seguimento suavizados,
  limites de pitch, sphere sweep contra sólidos e foco na cabeça articulada,
  incluindo proporções editadas. Com primeira pessoa desligada, o zoom respeita
  a saída da geometria do avatar. Se parede/limite de zoom ainda obrigar a câmera
  a entrar num membro, o avatar é ocultado sem mudar o modo de controle.
  Em primeira pessoa o avatar é ocultado integralmente.
  Follow alinha suavemente após um intervalo sem mouse.
  Camera Lock é uma opção explícita; não há atalho Shift Lock separado.
- `InputRouting.h`: um snapshot/contexto decide também gameplay/pausa.
  UI/modal/texto bloqueiam gameplay; pausa bloqueia WASD/scroll/câmera; Stop
  permanece disponível como saída de segurança. Câmeras nunca compartilham mouse.

## Formato e segurança

Mapas sem gameplay continuam JSON v1. JSON v2 acrescenta `game` versão 1:
`rigs`, `animations`, `defaultRig`, `player`. Dados antigos recebem defaults em
memória. O campo opcional `arc` de um keyframe só é gravado quando existe um
giro autoral maior que 180°; clips sem ele continuam byte a byte iguais. Carregamento usa staging e só troca a cena após validar tudo; save usa
o mesmo arquivo temporário/fsync/rename atômico. Arquivos com referências,
hierarquias, keys, quaternions ou valores inválidos são rejeitados sem apagar
a cena atual. Limites: 128 rigs, 256 clips, 10.000 keys/track e 100.000/projeto.

Lua declarativo v1 não possui schema de gameplay. Exportação com assets ou
configurações de jogo é recusada com orientação para JSON v2, evitando perda
silenciosa. Projetos JSON v2 não são compatíveis com versões antigas do editor.

## Diagnóstico e validação

Logs JSONL: `$XDG_STATE_HOME/loblox/loblox.jsonl`, ou
`$HOME/.local/state/loblox/loblox.jsonl`. Há rotação limitada; copie também `.1`
se existir. `LOBLOX_TRACE_INPUT=1 ./build/loblox` ativa dispatch por ação, sem
registrar posição de mouse continuamente. Campos `subsystem`/`event` compõem
nomes como `runtime.start`, `player.jump`, `animation.play`, `camera.capture_change`.
Cada evento tem frame e correlation_id; uma sessão Play usa correlação própria.

```sh
ctest --test-dir build --output-on-failure -j2
./build/loblox_game_tests
# Como usuário comum; inicia compositor privado, não usa o desktop pessoal:
bash tests/run_sway_input.sh ./build/loblox_game_ui_tests ./build/loblox
```

O teste gráfico usa fixture 1280×800, fonte ImGui padrão, janelas com posições
definidas, eventos xdotool reais e inspeção dos estados de produção. Compartilha
a mesma biblioteca de editor/renderização do aplicativo. Capturas e logs ficam
em `/tmp/loblox-game-ui-*`; restart usa `/tmp/loblox-restart-*` e seis processos.
O teste headless cobre matemática, animação, persistência, input, Bullet,
colisões, pulo, reset e repetição de Play/Stop. Não é uma certificação de todos
os mapas, placas de vídeo ou dispositivos de entrada.

Ambiente desta validação: Bullet 3.24; pkg-config anuncia raylib 6.2.0, mas os
headers e a biblioteca instalada identificam-se como 6.5-dev. Não foi trocada
a biblioteca do sistema para esconder essa divergência. GPU física, DPI do
notebook e orçamento real de 3 GB ainda precisam de validação no alvo.

### Checklist manual no Sway físico

Execute o fluxo 1–9 acima, repita Play/Stop cinco vezes e teste também:
RMB → Esc → Resume; primeira pessoa → perda de foco → retorno → Resume;
digitação numérica com teclas 1–4/F5/Ctrl+D; key drag → Esc → novo drag;
parede entre câmera/avatar; CanCollide desligado; salvar/fechar/reabrir.
Colete log JSONL, captura, resolução, escala Sway, renderizador e passos exatos
se houver conflito. O compositor headless não comprova mouse físico/DPI/GPU.

Referências funcionais consultadas, sem código/recursos proprietários:
[Rig Builder](https://create.roblox.com/docs/studio/rig-builder),
[Motor6D](https://create.roblox.com/docs/reference/engine/classes/Motor6D),
[Animation Editor](https://create.roblox.com/docs/animation/editor),
[Humanoid](https://create.roblox.com/docs/reference/engine/classes/Humanoid),
[câmeras](https://create.roblox.com/docs/workspace/camera),
[character controllers](https://create.roblox.com/docs/characters/character-controller-library),
[playtesting](https://create.roblox.com/docs/studio/testing-modes).
