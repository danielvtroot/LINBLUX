> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Progresso — fundação do editor

Data desta etapa: 2026-10-19

## 1. O que existia antes

- Repositório remoto `abismroot/loblox-open-source` (privado), branch `main`,
  com **um único commit** e apenas um `README.md` de 20 bytes. Não havia
  código, build ou documentação no remoto.
- Na pasta local havia um protótipo single-file não versionado (`src/main.cpp`,
  ~7,9 KB) com personagem em blocos, câmera em terceira pessoa e física
  simplificada própria. Esse protótipo foi **preservado** em
  `prototype/legacy_main.cpp` e não era compilado. O arquivo foi removido na
  limpeza da Alpha; a revisão original permanece no Git (veja README.md deste histórico).
- `CMakeLists.txt` mínimo e `LICENSE` MIT também estavam apenas no local.

Nada do protótipo atendia ao pedido (não tinha editor, hierarquia, ImGui,
Bullet, salvamento ou painéis). A fundação foi construída do zero, mantendo o
arquivo histórico.

## 2. O que foi implementado nesta etapa

### Núcleo e cena

- `Instance` com id estável de 64 bits, nome, classe, pai, filhos e proteção
  contra exclusão.
- `Part` com Block/Sphere, transformação, tamanho mínimo, cor, 6 materiais,
  Anchored e CanCollide.
- Serviços `Workspace` e `Lighting`; classes `Baseplate` e `SpawnLocation`.
- `Scene`: posse única, hierarquia real, busca por id, nomes únicos, seleção
  única, remoção de subárvore e listeners de remoção.
- Cena padrão: Workspace > Baseplate + SpawnLocation; Lighting.

### Física (Bullet 3D)

- Um corpo rígido por Part, criado/removido por eventos.
- Massa dinâmica, corpo estático quando ancorado.
- `CanCollide` via `CF_NO_CONTACT_RESPONSE`.
- Alterações do editor via flags (`transform`, `shape`, `anchored`, `collide`),
  aplicadas no próximo passo de simulação.
- Redimensionamento atualiza o collider **sem recriar o corpo**.
- Alternância de âncora reconstrói o corpo com a massa correta.
- Passo fixo de 1/60 s, até 5 subpassos, delta limitado.
- CCD em corpos dinâmicos para não atravessar a Baseplate.
- Raycast usado pela seleção da viewport.
- Remoção de objeto dispara remoção do corpo antes da destruição.

### Renderização e ferramentas

- Viewport 3D com render texture redimensionável.
- Céu azul em gradiente, luz direcional simples por face/quad.
- Baseplate 160×1×160 com studs em malha única cacheada.
- Grade espacial e contorno de seleção.
- Câmera orbital com pan, zoom e foco (`F`).
- Gizmos de Move, Scale e Rotate nos eixos X/Y/Z.
- Raycast de seleção sincronizado com Explorer/Properties.

### Interface

- Docking layout determinístico: toolbar no topo, Toolbox à esquerda,
  viewport no centro, Explorer no topo direito e Properties abaixo.
- Toolbar com ícones desenhados e rótulos: Select, Move, Scale, Rotate, Part,
  Sphere, Spawn, Color, Material, Anchor, Explorer, Properties, Save, Open.
  Nenhum botão Play/Stop/Test.
- Toolbox com Block, Sphere e SpawnLocation.
- Explorer com árvore expansível, seleção e menu de contexto (focar/excluir).
- Properties com Name, ClassName, Position, Size, Rotation, Color, Material,
  Anchored e CanCollide; edição imediata.
- Popup de cor com paleta + color picker; popup de materiais.
- Diálogos Save/Open com caminho, mensagens de erro e status na toolbar.
- Atalhos: Delete/Backspace, F, Ctrl+S, Ctrl+O, Esc.

### Persistência

- Formato JSON `loblox-project` versão 1 com serviços, hierarquia e todas as
  propriedades.
- Validação completa antes de alterar a cena.
- Escrita temporária + rename atômico.
- Reconstrução de corpos físicos após carregar (callback da UI → física).
- Mensagens de erro para JSON inválido, versão desconhecida, ids duplicados,
  pais inválidos, tamanhos inválidos etc.

### Build, testes e documentação

- `CMakeLists.txt` com `loblox_core`, `loblox_imgui`, executável `loblox`,
  executável `loblox_tests` e CTest.
- Suíte headless com 149 checagens.
- Documentação completa em `docs/` e README reescrito.
- `.gitignore` para `build/`, projetos salvos e temporários.

## 3. Arquivos criados/modificados

Criados:

```
CMakeLists.txt                 (reescrito)
src/main.cpp                   (novo entry point)
src/app/Application.h/.cpp
src/core/Math.h
src/core/Config.h
src/objects/Instance.h/.cpp
src/objects/Part.h/.cpp
src/objects/Workspace.h/.cpp
src/objects/Lighting.h/.cpp
src/objects/Baseplate.h/.cpp
src/objects/SpawnLocation.h/.cpp
src/scene/Scene.h/.cpp
src/physics/PhysicsWorld.h/.cpp
src/serialization/ProjectIO.h/.cpp
src/rendering/EditorCamera.h/.cpp
src/rendering/SceneRenderer.h/.cpp
src/editor/Tool.h
src/editor/Gizmo.h/.cpp
src/editor/EditorUI.h/.cpp
tests/test_main.cpp
docs/ARCHITECTURE.md
docs/PROGRESS.md
docs/DECISIONS.md
docs/TESTING.md
docs/ROADMAP.md
third_party/imgui/*            (vendorizado)
third_party/rlimgui/*          (vendorizado)
third_party/nlohmann/json.hpp  (vendorizado)
prototype/legacy_main.cpp      (caminho histórico; removido do ramo principal na limpeza)
```

Modificados: `README.md`, `LICENSE` (mantido MIT), `.gitignore`.

## 4. Compilação e verificação

Compilado neste ambiente com sucesso:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

- raylib 6.2.0, Bullet 3.24, GCC, Linux.
- Sem erros; sem warnings nos alvos próprios (`-Wall -Wextra`).

Testes:

```bash
ctest --test-dir build --output-on-failure
# 1/1 Test #1: loblox_tests ... Passed
./build/loblox_tests
# 149 checagens, 0 falhas
```

Smoke test gráfico em Xvfb + llvmpipe (software), janela 1280×720:
criação de Block/Sphere/Spawn, seleção, Anchor, Move, Scale, Rotate, Delete,
Color, Material, Save (Ctrl+S) e Open (diálogo → Abrir) verificados com
inspeção do JSON resultante. Detalhes em `docs/TESTING.md`.

## 5. Problemas encontrados e correções

1. **rlImGui exigia ícones Font Awesome não vendorizados.** Solução:
   `NO_FONT_AWESOME` e ícones desenhados com primitivas do ImGui.
2. **`IsRenderTextureReady` não existe na raylib 6.2.** Trocado por
   `IsRenderTextureValid`.
3. **Sombreamento das caixas incorreto (tudo com brilho 0.5).** Faces com
   winding invertido e backface culling ligado: a face de baixo aparecia no
   lugar da de cima. Corrigido desligando culling no passe 3D customizado.
4. **Corpo físico recriado a cada frame durante o Scale.** Substituído por
   atualização in-place do shape, massa, inércia e AABB.
5. **Primeiro Ctrl+S do teste automatizado não era capturado.** Não era bug da
   aplicação: o teste enviava teclas entre frames com renderização por software
   muito lenta. O teste passou a manter as teclas pressionadas por mais tempo.
6. **Teste de Open sobrescrevia o arquivo antes de carregar.** Corrigido o
   roteiro: salvar, alterar a cena sem salvar, abrir o arquivo antigo e conferir
   que a cena voltou ao estado salvo.

## 6. O que ainda não funciona / não existe

- Undo/redo, duplicação, multi-seleção, agrupamento e snap.
- Personagem R6, animações, scripts, modo de jogo/publicação.
- Terreno, ferramentas de união/subtração, catálogo de modelos.
- Materiais PBR, sombras, iluminação global.
- Backend Wayland nativo (usa X11/XWayland).
- Autosave, arquivos recentes e recuperação de sessão.
- Benchmark em GPU real e no notebook Loc-OS de destino. A medição de FPS
  disponível é de renderização por software (9–19 FPS), não representativa de
  uma GPU.

## 7. Próximo passo exato

Implementar **undo/redo** para criação, edição de propriedades, transformações
e exclusão (comandos que guardem o estado anterior e restaurem cena + corpos
físicos), seguido de **duplicar seleção com Ctrl+D**. Isso solidifica o fluxo
de construção antes de introduzir o personagem R6.

## 8. Redesign visual do editor — 2026-10-20

- Tema claro padrão com paleta consistente de superfícies, texto, campos,
  divisores, hover e azul de destaque. Uma fonte sans local é usada quando
  instalada, com a fonte padrão do ImGui como fallback.
- Barra de menus File/Home/Model/View com ações de salvar, salvar como, abrir,
  seleção, transformações, inserção e visibilidade dos painéis.
- Mantidas as abas Home/Model/View da ribbon e reduzida a faixa superior de
  docking para devolver altura à viewport.
- Layout de painéis salvo em `loblox_imgui.ini`; primeira execução usa o layout
  padrão e sessões seguintes preservam o docking existente.
- Toolbox e Explorer continuam filtrando os dados existentes da cena. Inserção,
  seleção, propriedades, gizmos, renderização, física e ProjectIO permanecem
  conectados aos componentes originais.
- Arquivos alterados nesta etapa: `src/app/Application.cpp`,
  `src/editor/EditorUI.cpp`, `docs/ARCHITECTURE.md`, `docs/PROGRESS.md` e
  `docs/TESTING.md`.
- Build final concluído (`cmake --build build -j2`); CTest passou (1/1, 0 falhas)
  e `git diff --check` passou.
- Smoke gráfico em Xvfb/llvmpipe 1280×720 iniciou e renderizou a viewport
  775×538. A captura `/tmp/loblox-ui-final.png` foi inspecionada visualmente:
  tema claro, ribbon, Toolbox, viewport, Explorer e Properties aparecem nas
  regiões planejadas, com cena real (céu, studs e SpawnLocation).
- Abrir/salvar e comandos por mouse não foram reexercitados nesta etapa; os
  testes headless existentes verificam a cena, física e serialização.
- Limitações restantes: a aba Model mantém comandos contextuais repetidos para
  transformação/aparência; ícones continuam desenhados com primitivas ImGui,
  sem uma fonte de ícones dedicada.

## 9. Câmera, gizmos e formato declarativo de cena — 2026-10-20

- Causa da navegação presa ao botão direito: o movimento WASD estava dentro da
  mesma condição de RMB usada para órbita. A câmera agora separa movimento livre
  com viewport focada e órbita temporária com cursor capturado. A captura é
  liberada ao soltar RMB, perder foco, sair/cancelar interação e destruir a UI;
  a posição anterior do cursor é restaurada.
- Entrada de câmera é bloqueada quando ImGui está editando texto, um gizmo está
  ativo ou outro painel tem foco. RMB não seleciona; MMB faz pan; roda aproxima
  ao foco; F enquadra; WASD/QE movem com delta time e Shift acelera.
- Causa da escala incorreta: o antigo manipulador alterava tamanho simetricamente
  ao redor do centro. O novo cálculo guarda snapshot e aplica a escala da face
  escolhida, mantendo a face oposta parada. Move inclui três eixos e três planos;
  Rotate usa ângulo entre vetores no plano do anel. Há modo global/local e snap.
- Durante manipulação, o corpo Bullet é suspenso sem recriação por frame e
  reativado ao fim. A edição in-place atualiza shape e AABB. Sphere mantém escala
  uniforme por ser representada por esfera Bullet, não elipsoide.
- `SceneLua` importa/exporta uma tabela literal restrita, validada antes de
  substituir a cena. Não executa Lua/Luau. JSON nativo v1 permanece compatível.
- Arquivos principais: `EditorCamera`, `Gizmo`, `GizmoMath`, `EditorUI`,
  `PhysicsWorld`, `ProjectIO`, `SceneLua`, testes e documentação.
- Validação: `cmake --build build -j2` passou; CTest 1/1 passou; executável de
  testes reportou 241 verificações e 0 falhas. Xvfb/llvmpipe iniciou o app em
  1280×720 e a captura `/tmp/loblox-camera-gizmo-lua.png` foi inspecionada.
- Limites: captura/teclas dependem do backend GLFW/X11 e não foram automatizadas
  em teste headless. A interface foi inspecionada graficamente; essa captura
  valida layout/inicialização, não substitui teste interativo no Sway/Loc-OS.

## 10. Próximo passo

Adicionar undo/redo transacional para criação, propriedades, transformações e
exclusão. Depois, testar a captura de cursor e a edição de gizmos em uma sessão
interativa real no Sway/XWayland.

## 11. Auditoria e estabilização da edição — 2026-10-20

- Auditoria encontrou alças de Scale antigas posicionadas em `centro ± tamanho
  fixo do gizmo`; por isso entravam em peças grandes. Além disso, eixos opostos
  projetavam-se um sobre o outro perto de ângulos diagonais. Agora as âncoras
  partem do suporte da caixa orientada atual, recebem offset por profundidade e
  os ícones de tela são separados em caso de colisão; o picking reutiliza os
  mesmos centros e os tether lines ligam o ícone à âncora 3D.
- Scale Global em peça rotacionada projeta o movimento do eixo mundial para a
  dimensão local mais alinhada, mantendo apenas um componente de tamanho e a
  face local oposta fixa. Uma escala global exata produziria shear não
  representável pelo modelo atual de `Part`; esta limitação está documentada.
- `WrappedAngleDelta` e teste de borda ±180° foram centralizados na matemática
  do gizmo. Arraste gráfico de anel atualizou rotação para ~30° Z no JSON.
- A sensibilidade/velocidade da câmera agora são ajustáveis na aba Model;
  trocar ferramenta libera captura. Em Xvfb, RMB + movimento mudou a câmera e
  a soltura restaurou a posição do cursor (700,440).
- Causa raiz de “arquivo salvo sumiu”: o caminho inicial era relativo ao cwd.
  Agora usa `$XDG_DATA_HOME` ou `~/.local/share/loblox`; Save abre diálogo
  preenchido e Save As mantém o caminho atual. JSON v1 permanece o formato.
- `ProjectIO::save` valida a serialização, cria temporário único no mesmo
  diretório, escreve/sincroniza/fecha e faz rename POSIX. Falhas pré-rename
  limpam o temporário e preservam o destino. A UI reporta caminho e causa.
- Arquivos desta etapa: `GizmoMath`, `Gizmo`, `EditorUI`, `EditorCamera`,
  `Config`, `ProjectIO`, `tests/test_main.cpp`, README e documentação.
- Build passou, CTest 1/1 e suíte direta 306 verificações/0 falhas. Smoke Xvfb
  validou seis alças, arraste X e salvamento, anel Rotate, órbita RMB e
  restauração do cursor. Capturas em `/tmp/loblox-scale-stabilized.png`,
  `/tmp/loblox-rotate-rings.png` e `/tmp/loblox-camera-after.png`.
- Não validado: execução no Sway/Loc-OS físico, as outras duas rotações por
  interação gráfica, captura ao alternar foco real de janela, e salvar em mídia
  com falha real de I/O. Testes headless cobrem os caminhos matemáticos e erro
  de rename; não simulam o subsistema de janela.

## 12. Próximo passo

Testar no Sway/XWayland real os seis handles, três anéis, perda de foco e
digitação simultânea em Properties/Toolbox; em seguida adicionar undo/redo
transacional para operações de edição.

## 13. Unificação dos gizmos 3D — 2026-10-20

- Auditoria dos caminhos reais confirmou que a seleção de Scale já desenhava e
  percorria seis centros, mas não existia identidade de handle persistente: o
  drag reconstruía eixo e sinal em campos separados. O caminho Global também
  dividia o deslocamento pela projeção e descartava seu sinal ao mapear um eixo
  mundial para o eixo local. Isso exagerava a escala quando os eixos apontavam
  no mesmo sentido e fazia faces correspondentes a alinhamento negativo
  diminuírem ao arrastar para fora. Os testes gráficos anteriores só tinham
  arrastado X−, então não haviam revelado a assimetria.
- `GizmoMath::HandleId` agora identifica cada face, eixo, plano e anel. O
  retângulo usado no hit test é o mesmo 24×20 px do overlay; um clique sem
  handle válido não inicia transformação.
- Unifiquei a aplicação das seis faces em `ScaleFaceForHandle`, sempre a
  partir do snapshot inicial. Global usa projeção (multiplicação), Local usa o
  eixo local direto; o lado oposto permanece fixo. O overlay passa por etapa
  sem depth test, restrita aos handles de tela, e as âncoras usam o suporte
  atualizado da caixa.
- A matemática recusa delta/eixo/tamanho inválidos. Move e Rotate continuam
  usando o mesmo snapshot e os mesmos eixos normalizados; cada operação mantém
  sua semântica própria.
- Testes automatizados incluem os seis lados em caixas alinhadas e rotacionadas,
  nos modos Global e Local, dimensões não selecionadas, face oposta,
  identidade/picking dos seis ícones, ausência de seleção e valores mínimos;
  build e CTest passaram após a mudança.
- Xvfb permitiu abrir o editor e inspecionar a composição clara e a viewport.
  A tentativa de clique via `xdotool` neste contêiner sem gerenciador de janelas
  não acionou o botão Toolbox; assim, não declaro validação gráfica interativa
  dos seis drags nesta etapa.

Próximo passo: executar a matriz de seis drags em sessão XWayland/Sway com foco
de janela real e comparar os JSON antes/depois. Os testes headless verificam a
matemática compartilhada, mas não substituem essa interação gráfica.

## 14. Realce e visibilidade unificados dos gizmos — 2026-10-20

- Move, Scale e Rotate agora consultam o `HandleId` selecionado para desenhar o
  estado de hover/drag; o realce já não depende de comparar eixos/planos em
  estado separado.
- Eixos, planos, anéis e faces são desenhados em passe x-ray, passando
  visualmente sobre a Part. O depth test fica desligado só durante o desenho
  do gizmo e é restaurado ao fim; o restante da cena mantém a profundidade
  normal. Scale conserva os alvos de tela e hit test coincidentes.
- Build passou; CTest 1/1 passou; suíte direta: 506 verificações, 0 falhas.
- A abertura/captura visual Xvfb anterior confirma a viewport e painéis, mas
  não mostra um gizmo selecionado. Interação gráfica em janela com foco
  continua pendente: o Xvfb local não possui window manager e os cliques de
  `xdotool` não ativaram os controles.

## 15. Controles visíveis sobre a geometria — 2026-10-20

- A inspeção confirmou que as linhas 3D finas ainda pareciam iguais à versão
  anterior. Substituí o desenho dos gizmos por controles screen-space no passe
  overlay: Move mostra eixos grossos com botões X/Y/Z e botões XY/XZ/YZ;
  Rotate mostra anéis espessos com contorno escuro e botões X/Y/Z conectados
  ao anel; Scale mantém os seis botões X±/Y±/Z±. Todos são renderizados acima
  da Part, com depth test desligado somente nesse passe.
- O picking dos botões de eixos e anéis usa as posições dos próprios controles
  visuais; os anéis continuam tendo também picking geométrico ao longo do arco.
  Hover e drag destacam o `HandleId` correto.
- Interação gráfica real em Xvfb/llvmpipe 1280×720: arrastei o botão Move X e
  confirmei `position.x = 3`; arrastei o botão de Rotate X e Properties mostrou
  `rotation.x = 15°`; arrastei Scale Y+ e Properties mostrou `size.y = 5` e
  `position.y = 6.5` (face oposta em Y=4). Capturas: `/tmp/loblox-move-overlay.png`,
  `/tmp/loblox-rotate-overlay.png`, `/tmp/loblox-scale-overlay.png`,
  `/tmp/loblox-rotate-drag.png` e `/tmp/loblox-scale-drag-check.png`.
- Build, CTest e suíte direta aprovados (506 verificações, 0 falhas). Os drags
  visuais confirmam um eixo de Move, um anel de Rotate e um lado de Scale; os
  seis lados de Scale permanecem cobertos pela matemática automatizada, mas
  não foram individualmente arrastados no GUI.
- O teste foi feito em Xvfb/GLFW/X11 com renderização llvmpipe, não em Sway ou
  no notebook de destino.

## 16. Estabilização B–E — 2026-10-20

- Preservei o trabalho local no commit `c218a78` antes das alterações, sem
  reset/clean e sem push. O backup verificado permanece fora do repositório.
- **B — rotação:** o renderer ignorava `Part::rotation()` em boxes, seleção,
  Baseplate/studs/grade e decorações da SpawnLocation. O collider já rotacionava,
  por isso a silhueta ficava 214×54 px após Y=90° enquanto o AABB mudava para
  x≈4/z≈8. Vértices e normais agora usam o mesmo quaternion em todos esses
  caminhos. O teste gráfico 8×2×4 em 0/45/90/arbitrário compara silhueta,
  wireframe e collider com os oito vértices projetados.
- **C — sessão de drag:** `gizmoEditingId_` era uma segunda fonte de verdade e a
  câmera processava RMB antes do gizmo. `GizmoInput`/`GizmoUpdateResult` tornaram
  o ciclo determinístico; `TransformSession` suspende e libera o corpo em
  começo/fim/cancelamento. Clique no handle é consumido antes da seleção;
  câmera não navega durante o arraste; Escape/perda de foco/troca de ferramenta/
  seleção/modal/exclusão encerram a sessão; soltar confirma. Testes cobrem
  hover, confirmação, foco perdido, troca de ferramenta, cancelamento explícito,
  destruição e ausência de corpo suspenso.
- **D — persistência/inicialização:** `ProjectIO::fromJson` construía a cena
  depois de `clearObjects`, podendo deixar estado parcial. Agora uma cena
  candidata é construída e validada; `replaceContentsFrom` faz a troca O(1) e
  avisa a física sobre as Parts antigas. Save valida cena+JSON antes do destino;
  o escritor atômico foi extraído e reutilizado pelo Lua, com `fsync` do
  diretório. `--project`, caminho padrão e cena padrão têm policy documentada;
  projeto inválido não destrói a cena. `IsWindowReady` elimina o segfault sem
  display (agora exit 1).
- **E — invariantes/diagnóstico:** `Scene::validate` e `PhysicsWorld::validate`
  cobrem ids, hierarquia, seleção, transform finito/normalizado, tamanho mínimo,
  Part↔corpo, órfãos e edição contida. `Part` rejeita NaN/infinito. O logger
  JSONL registra inicialização, persistência e transformações com correlação,
  duração, resultado e erro, sem conteúdo de projeto, rotacionando em 1 MiB.
- Resultado vigente: build Release, CTest 4/4, **845 checagens headless**, 300
  casos / **7.509 checagens gráficas**; build ASan/UBSan e CTest 4/4 sem
  relatórios. Um use-after-free no teste de invariantes foi encontrado pelo ASan
  e corrigido.
- A validação gráfica continua em Xvfb/llvmpipe, não em Sway/XWayland, GPU
  física ou HDD. Não há CI versionado nem teste de falha de energia/fsync.

Próximo passo: repetir a matriz de drags e o startup no Sway/XWayland real,
adicionar undo/redo transacional e considerar um teste de injeção de falhas de
escrita/fsync em ambiente controlado.
