# Progresso — fundação do editor

Data desta etapa: 2026-10-19

## 1. O que existia antes

- Repositório remoto `abismroot/loblox-open-source` (privado), branch `main`,
  com **um único commit** e apenas um `README.md` de 20 bytes. Não havia
  código, build ou documentação no remoto.
- Na pasta local havia um protótipo single-file não versionado (`src/main.cpp`,
  ~7,9 KB) com personagem em blocos, câmera em terceira pessoa e física
  simplificada própria. Esse protótipo foi **preservado** em
  `prototype/legacy_main.cpp` e não é compilado.
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
- Suíte headless com 147 checagens.
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
prototype/legacy_main.cpp      (protótipo anterior preservado)
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
# 147 checagens, 0 falhas
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
