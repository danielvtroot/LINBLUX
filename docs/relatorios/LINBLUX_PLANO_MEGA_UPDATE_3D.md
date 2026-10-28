# LINBLUX — Plano de Mega Update 3D (execução controlada)

> **Estado:** plano técnico, NÃO implementação. Base: auditoria `AUDITORIA_COMPLETA_MOTOR_3D_LINBLUX.md` de 27/10/2026, commit `2aebae4`, árvore `dirty` com alterações em `assets/ui/documents/studio.rml` e `assets/ui/styles/studio.rcss`. Reconferir tudo no início: a auditoria não executou testes nem mediu GPU. Revisão externa: documentação oficial Godot, Blender, Unity, Unreal, raylib e glTF. **Não supor HEAD atual igual ao auditado.**

## Resultado de produto pretendido

- **Visual diferente, moderno e limpo:** sem linhas pretas tracejadas por padrão; materiais 3D com luz calculada por pixel; sombras realistas e ajustáveis; textura nítida e coerente; bordas suaves; profundidade perceptível; cor correta; modos de qualidade para hardware fraco.
- **Autoria fácil:** mover/rotacionar/escalar previsível; encaixar paredes e pisos sem calcular manualmente; selecionar corretamente peças longas ou sobrepostas; visualizar colisões reais sob comando; feedback claro quando snap não é possível.
- **Compatibilidade:** Studio e Player usando o mesmo caminho de aparência, com passes do Editor separados; arquivos `.lo`, JSON v1–v6, Lua, som, decals, rig, NPC, inventário e materiais antigos mantidos.
- **Desempenho:** medir e preservar o baseline em GPU integrada/GL3.3. Recursos caros desligáveis, sem exigir Vulkan, GPU dedicada ou >3 GB RAM só para abrir o projeto.
- **Proteção:** cada etapa é funcional isoladamente, testada, commitada e publicada sem `force`.

## Diagnóstico verificado na auditoria (não inventar nova causa)

| ID | Constatação | Código relevante |
|---|---|---|
| F1 | Arestas pontilhadas = `drawEditorGuides`, ligadas por padrão, com linha coincidente com faces/depth test sem bias. F8 alterna. | `src/rendering/SceneRenderer.cpp`, `SceneRenderer.h`, `src/editor/EditorUI.*` |
| F2 | Blocos renderizados imediatamente por CPU/rlgl, com Lambert + hemisférica por face, sem sombras/PBR/AO/culling/ordenação transparente. | `src/rendering/SceneRenderer.cpp` |
| F3 | Part nova usa cor `(163,162,165)` e `SmoothPlastic`; superfícies grandes ficam planas por falta de gradiente e sombra. | `src/objects/Part.h`, `SceneRenderer.cpp` |
| F4 | Mipmaps, trilinear e anisotropia 4× JÁ EXISTEM; não reimplementar. | `src/game/ImageAssets.cpp` |
| F5 | Snap de grade arredonda DELTA, não posição absoluta; snap por face escolhe menor correção, não face sob cursor; não alinha rotação; veta silenciosamente interseção. | `src/editor/GizmoMath.cpp`, `Gizmo.cpp`, `EditorUI.cpp` |
| F6 | Gizmos são overlays 2D projetados com direções fallback; nem sempre representam claramente eixos 3D. | `src/editor/GizmoLayout.cpp`, `Gizmo.cpp` |
| F7 | `Model` tem filhos com transform em espaço-mundo (transformação “assada”); grandes mudanças em hierarquia implicam risco alto. | `src/scene/Scene.cpp`, `src/objects/Instance.h` |
| F8 | Bullet já possui CCD, solver 16, corpo por Part e correção de margem; NPC tem velocidade horizontal reescrita, sem aceleração e descompasso com passada visual. | `src/physics/PhysicsWorld.cpp`, `src/game/Npc.cpp` |
| F9 | Editor desenha todos os rigs de `GameAssets` mesmo sem instância no mundo. | `SceneRenderer::drawScene`, `EditorUI.cpp` |
| F10 | `ServerStorage` e `ReplicatedStorage` existem, mas hierarquia bloqueia Part fora de Workspace; botões não são recurso completo. | `Scene::accepts`, `Scene::canParent` |
| F11 | Cores/transparência são emitidas sem ordenação de objetos translúcidos; céu possui pipeline gamma diferente do restante. | `SceneRenderer.cpp` |
| F12 | Testes de serialização e reinício do default já estiveram vermelhos em relatórios históricos; precisam de nova verificação. | `tests/`, `CONCLUSAO_ENGINE.md` |

## Decisão arquitetural

**Manter raylib + OpenGL 3.3 e Bullet neste ciclo.** Introduzir um `RenderPath`/`RenderSettings` ou fachada equivalente apenas onde ajudar a transição: `Legacy` fallback e `ModernGL33` novo. Os dois renderizam a mesma cena, usam IDs e assets existentes, e o Player não depende de RmlUi/ImGui.

Renderização sugerida em passos progressivos: `culling -> opaque geometry -> directional/ambient lighting -> optional shadow map -> transparent/decal sorted -> editor selection/hover overlays -> UI` com recursos alocados por viewport e restauração cuidadosa de estados OpenGL. **Não inventar que raylib já faz isso automaticamente.**

### Bibliotecas (decisão justificada, não instalação em massa)

| Candidato | Decisão | Quando |
|---|---|---|
| raylib + rlgl + shaders GLSL330 | **Usar já**; exemplos oficiais de basic PBR, shadow map, basic lighting, instancing | núcleo do visual novo |
| OpenGL 3.3 (via backend já existente) | **Usar já**; depth, FBO, shader, blend, culling; restaurar estado | núcleo do visual novo |
| glTF 2.0 / metallic-roughness (especificação Khronos) | **Adotar como referência de material** (cor base, metallic, roughness, normal, occlusion, emissive) | fase material |
| cgltf (MIT) | **Opcional** só no incremento de importação glTF/GLB, após prova de que `raylib LoadModel` não atende | depois do render básico |
| meshoptimizer (MIT) | **Opcional** apenas quando existirem malhas próprias/mesh caching e benchmark justificar | otimização de mapas grandes |
| Recast & Detour (zlib) | **Não agora**; navmesh é outra frente | fase futura de NPCs |
| Jolt/PhysX | **Não agora**; trocar Bullet não resolve gizmos ou arrasto de animação | só após benchmark e prova |
| bgfx/Filament/Dawn/wgpu/Vulkan | **Não agora**; refatoração de backend completa, alto custo/risco | RFC futuro separado |

Toda biblioteca adicionada exige versão fixa, licença auditada, aviso de terceiros, build reproduzível e teste Linux limpo. Não importar recursos/arte/GUI de terceiros sem licença.

## Regras de economia de tokens e tempo — obrigatórias

1. **Uma execução do desenvolvimento = uma fase pequena**, nunca “faça o mega update inteiro”. O agente lê esta seção, a fase atual e APENAS os arquivos indicados. Não relê 2.519 linhas da auditoria inteira; usar seções e `ATLAS locate`.
2. Antes: `git status --short`, `git log -1 --oneline`, `git diff --check`. Não sobrescrever WIP. Se conflitante, parar edição e documentar.
3. Localização: `python3 tools/atlas/atlas.py locate 'termo'` e `impact` quando disponível. Não varrer o repositório sem alvo.
4. Verificação por fase: target mínimo + testes de domínio + `python3 tools/turbo/turbo.py test --mode fast --changed`. `impact` no fechamento de fase grande; **FULL** só no candidato Alpha, supervisionado. `PASSED_PARTIAL` != regressão global aprovada.
5. Nada de screenshots a cada mudança de 1 linha; **uma captura antes/depois por fase visual**, mesma câmera/resolução. Se GL/Xvfb inviável, registrar pendência, sem aprovação fictícia.
6. Limites: uma fase por contexto; no máximo dois ciclos de tentativa do MESMO teste sem novos indícios. Se bloqueio persistir, salvar log, documentar hipótese e publicar somente parte verde.
7. Commits atômicos + push normal + hash remoto confirmado; não marcar fase concluída apenas porque código compila. Nunca executar `reset --hard`, `clean -fd`, `force push`, nem apagar mundos/assets do usuário.
8. Ao terminar: relatório de **até 12 linhas** com fatos, arquivos, testes, screenshots, tempo, status `DONE/PARTIAL/BLOCKED`, commit/push e próxima fase. Guardar evidências e retomada em `docs/relatorios/MEGA_UPDATE_3D_PROGRESSO.md`.
9. Não publicar nova Release, tag, migration ou mudar versão automaticamente. Se ficar sem crédito, concluir incremento verde e deixar um comando claro de retomada.

## FASE 0 — baseline e proteção (sem recurso novo)

**Meta:** saber o que está sendo comparado, evitar “mudou no GitHub mas não no executável”.

- Verificar WIP `studio.rml`/`studio.rcss` da auditoria e quaisquer mudanças posteriores; integrar somente com autoria definida. Atualizar `ATLAS status`.
- Verificar por que o `project(LINBLUX VERSION 0.3.0)` diverge da proposta Alpha 0.1.0. **Registrar**, sem renomear versão ainda.
- Criar 3 fixtures *sem alterar experiência pessoal*: `grey-room` (paredes longas/piso/decal), `material-gallery` (seis materiais, esferas e transparência), `editor-snap` (partes giradas, pivô distante, encaixe).
- Capturas em 1366×768 e 1920×1080 com câmera e luz fixas; contar draw calls/memória/frametime se já existir instrumentação; caso contrário acrescentar diagnóstico mínimo (não profiler gigante).
- Verificar testes historicamente vermelhos (`linblux_tests` JSON; `linblux_engine_default_restart`) por nome/erro atual, sem supor que continuam falhando.
- **DONE:** fixtures reprodutíveis e baseline registrados; árvore protegida.

## FASE 1 — remover artefatos feios e devolver legibilidade imediata (primeira implementação obrigatória)

**Alterar:** `SceneRenderer.{h,cpp}`, `EditorUI.{h,cpp}`, ribbon RML/RCSS se preciso; testes de `gizmo_visual`/editor.

- Desligar `EditorGeometryView.edges` **por padrão**; F8 continua. Expor controle discreto “Arestas” (off por padrão), com estado/tooltip. Hover e seleção destacados apenas quando solicitados; evitar cores de debug em cada peça.
- Quando “Arestas” estiver ON, verificar e corrigir tracejado por *depth conflict* (bias tecnicamente adequado ou passe separado); não usar linhas sem teste de profundidade que desenhem através de paredes por acidente.
- Corrigir inconsistência de bounds de esfera: depuração e navegação devem refletir `sphereRadius`, após teste de compatibilidade.
- Garantir que wireframe/colisores/normais nunca apareçam no Player por padrão.
- **DONE:** sem linhas pretas pontilhadas no Editor normal; F8 funciona; captura 2 resoluções; nenhuma regressão em decal/seleção/Player.

## FASE 2 — tornar o editor realmente fácil de construir (gizmos e snap)

**Alterar:** `GizmoMath.{h,cpp}`, `Gizmo.{h,cpp}`, `GizmoLayout.*`, `EditorUI.*`, `TransformSession.*`, `ModelTransformSession.*`, testes.

- Separar Grade Absoluta (`round(world_position/step)*step`) de Incremento Relativo (`round(delta/step)*step`), com opções explícitas; preservar projetos e atalhos antigos.
- Snap por superfície/face **sob o cursor** em vez de menor delta global; escolher face e ponto de contato com raycast OBB, selecionar candidato de modo estável/histerese e prévia translúcida.
- Configuração opt-in “Alinhar rotação à normal”; prever orientação e offset; não mudar rotação sem comando explícito.
- Snap de borda e vértice **para caixas** primeiro; esfera usa normal e raio verdadeiros ou recurso marcado não suportado (não caixa fictícia invisível).
- Feedback de veto de interseção com nome da peça alvo, motivo e ação clara; não mover automaticamente outras peças. Undo/cancel atômicos.
- Corrigir alças de parede grande: eixo com legenda, tamanho de tela consistente, preview numérico, manter hit test = draw layout, fallback determinístico quando projeção degenerada.
- Testar OBB 0/15/30/45/90°, parede de 192 u, caixa de 0,2 u, vários objetos concorrendo, models com pivô deslocado, zoom extremo, arraste cancelado, 100x transformação.
- **DONE:** montar recinto com 4 paredes e piso sem inserir manualmente XYZ; nenhuma penetração além da tolerância numérica; resultado persistente após save/reopen.

## FASE 3 — NOVO material e pipeline de cor (grande mudança estética)

**Alterar:** fachada de render/`SceneRenderer.cpp` e novos módulos pequenos em `src/rendering/`, `Part`/assets/persistência de forma aditiva, UI propriedades, testes.

- **Shaders GLSL 330** para iluminação por pixel com posições/normais corretas e especular, UV por face e mapas repetíveis. Compartilhar pipeline entre Studio e Player; fallback CPU legacy em falha de shader.
- Separar sRGB de espaço linear: decodificar textura base color, combinar luz linear, aplicar tonemap/gamma uma vez; evitar dupla conversão do céu. Usar testes/capturas contra transparência/decals pré-multiplicados.
- Definir **`Material3D` aditivo** com `baseColor`, `metallic`, `roughness`, `emissive`, `normalScale`, opcional `baseColorTexture`, `normalTexture`, `metallicRoughnessTexture`, `occlusionTexture`. Presets autorais: Plástico, Concreto, Madeira, Metal, Vidro simples, Emissivo. `PartMaterial` antigo é mapeado de forma compatível e preserva a aparência legada quando modo legacy selecionado.
- Materiais independentes de decals: um decal não deve substituir o material base. Suportar texturas de acabamento repetidas por dimensão (studs/tile), controlar UV e orientação.
- Não adicionar biblioteca externa até demonstrar que o shader e modelo de dados exigem; para primitivas raylib/rlgl basta. Se introduzir serialização, campo opcional versionado + ida/volta de projeto antigo.
- Perfis `Baixo` (simples), `Médio` (por pixel), `Alto` (PBR leve) e fallback; não considerar “PBR” completo se ambiente/BRDF não estiver implementado.
- **DONE:** galeria material claramente distinta, esferas iluminadas suavemente, textura legível e estável, legacy round-trip + renderização correta no Player; screenshots idênticos em lógica entre Editor e Player (sem overlays).

## FASE 4 — sombras e profundidade de qualidade, com desempenho controlado

**Alterar:** renderer e shaders FBO/GL, `Lighting`, configuração gráfica e testes.

- Shadow map direcional **opcional** (primeiro 1 mapa 1024², PCF 3×3); bias dependente de normal + depth bias, limite de distância de sombra, frustum ajustado; evitar acne/peter panning.
- Contato visual: primeiro shadow map; AO/SSAO half-resolution só se benchmark mostrar viabilidade. Não usar quad escuro arbitrário que atravesse paredes/decals.
- Separar passes opacos, transparentes e decals; ordenar objetos transparentes simples por distância da câmera e documentar limites de interseção; depth write off, blend correto e GL state restaurado.
- Melhorar antialiasing de cena: avaliar MSAA 3D real (render target apropriado) vs FXAA existente; evitar duplicar MSAA da camada RmlUi. Configurar qualidade com fallback.
- Garantir sky/sol/fog e pós-processo em espaço de cor coerente, sem tela lavada. Comparar em mapa cinza, 6 materiais, vidro e decal.
- **DONE:** sombras reais sob paredes/objeto, cantos com leitura melhor, sem manchas/tracejados; `Baixo` continua utilizável em hardware fraco.

## FASE 5 — geometria persistente, seleção e desempenho de mapas grandes

- Criar malhas de cubo/esfera reutilizáveis com normals/tangents/UV (sem gerar vértices procedurais por Part por frame), após medir; preservar transform de Part, hitboxes, IDs, seleção e decals.
- Cache/batching por material e tipo; frustum culling por bounds conservadores; habilitar backface culling em geometrias com winding validado. Não aplicar culling em peças transparentes de forma errada.
- Medir 100/500/2000 Parts no mesmo hardware baseline. `meshoptimizer` **somente se** importação de malhas ou índices complexos justificarem.
- Picking por raycast OBB/esfera existente, sem depender da cor do framebuffer; outline de seleção/hover somente no Editor. Diagnóstico de colisores é toggle dedicado com formas reais do Bullet.
- **DONE:** custo de frame medido, sem piora injustificada >25% no perfil Baixo; consumo de RAM e desenhos OOB documentados; sprites/decals/texturas persistem.

## FASE 6 — personagens, física e rigs (depois de estabilizar o motor gráfico)

- Corrigir `NpcSystem` com aceleração/turn coordination, travamento de rotação consistente, distância de parada, y do piso e velocidade de animação proporcional a deslocamento; medir patinação da sola. Não culpar Bullet sem evidência.
- Medir escala do rig 5 u vs colisor 5,8 u e corrigir offsets sem quebrar rigs existentes. Estado de queda em pulo curto exige teste baseado em altura/tempo, não apenas `vy < 0`.
- Rigs visíveis apenas quando **instância** de mundo ou preview explícito; não renderizar todos os GameAssets automaticamente; preservar templates.
- `ServerStorage`/`ReplicatedStorage`: verificar hierarquia aceita modelos/Parts guardados sem corpos físicos, e instanciação com IDs novos; `StarterItems`/template do personagem e missões Lua em incremento distinto com testes de save/respawn.
- Melhoria de colisões/HUD runtime somente com testes determinísticos; não migrar Bullet.
- **DONE:** NPC perseguidor da demo caminha, gira, colide e ataca; item recebido/removido por Lua; nenhuma regressão de RifleDemo, personagens ou mundo padrão.

## FASE 7 — polish do Studio e release safety

- Ribbon responsivo: remover reserva fixa de 216 px e `overflow:hidden` que corta ferramentas; reorganizar Grade/Encaixe/Mundo em grupo de largura intrínseca/scroll acessível. UI em 1366×768 e 1920×1080.
- Biblioteca de materiais com miniaturas reais; botões “aplicar”, “editar”, “salvar preset”, “restaurar”. Não permitir nomes técnicos como experiência de uso padrão.
- Seletor visual e árvore: realçar Modelo/Part, serviços de armazenamento funcionais; deixar scripts Lua, áudio e inventário preservados.
- Atualizar guia do Editor e `EDITOR_PRECISAO.md` conforme comportamento real (não deixar docs de snap desatualizadas).
- Regredir falhas de serialização JSON e `linblux_engine_default_restart`; testar `.lo`/assets/experiências antigas. FAST+IMPACT+suite ampla supervisionada; imagens por comparação fixa; pacote Linux fora checkout; licença de material externo e terceiros; outra distro/hardware real quando disponível.
- Não criar Release com falhas críticas desconhecidas. Escolher versão com base em CMake (`0.3.0`) e anúncio de Alpha (`0.1.0`) após decisão explícita.

## Gate por fase: evidência mínima padronizada

```text
PHASE: X
CODE: [nomes reais + diffstat]
BEFORE / AFTER: [caminhos de screenshots geradas]
BUILD: [targets + status]
TESTS: [selecionados, passes, fails, tempos; parcial ≠ total]
BENCHMARK: [HW/renderer/resolution/ms/frame/RSS ou 'não medido']
COMPAT: [projetos antigos, .lo, scripts, Player]
GIT: [commit + push + ls-remote origin main]
STATE: DONE | PARTIAL | BLOCKED
NEXT: [fase e comando concreto]
```

## Referências oficiais externas (para justificar decisões, não para copiar código/arte)

- raylib shaders: https://github.com/raysan5/raylib/tree/master/examples/shaders
- raylib API: https://www.raylib.com/cheatsheet/cheatsheet.html
- Godot renderers: https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html
- Godot materials: https://docs.godotengine.org/en/stable/tutorials/3d/standard_material_3d.html
- Unreal snapping: https://dev.epicgames.com/documentation/unreal-engine/actor-snapping-in-unreal-engine
- Unity snapping: https://docs.unity.com/en-us/engine/6000.3/manual/working-with-scenes/scenes-manage-gameobjects/positioning-game-objects
- Blender snapping: https://docs.blender.org/manual/en/latest/editors/3dview/controls/snapping.html
- Khronos glTF material spec: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
- cgltf: https://github.com/jkuhlmann/cgltf
- meshoptimizer: https://github.com/zeux/meshoptimizer
- Recast: https://github.com/recastnavigation/recastnavigation

## Regra final

**Não entregar como “mega update pronto” após corrigir 1 shader.** Este arquivo é roadmap. Cada fase é uma missão isolada, com gates e sem novos temas até a fase corrente estar verde. A **primeira tarefa de implementação do desenvolvimento é só FASE 0 + FASE 1**. Em execuções futuras, entregar **exatamente uma fase por vez**.
