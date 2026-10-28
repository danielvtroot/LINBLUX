# Módulo `rendering`

**Responsabilidade:** renderizar a cena, personagem, seleção, colisões e céu
procedural; fornecer a câmera de edição.

**Fonte:** `src/rendering/{SceneRenderer,EditorCamera}.{h,cpp}`,
`ProceduralSkyShader.glsl`.

**Entradas:** `Scene`, `GameAssets`/rig, câmera (`Camera3D` ou `EditorCamera`),
viewport, seleção, flags de depuração e configurações de céu/fog.

**Saídas:** desenho OpenGL/raylib, projeção de tela, raycast de câmera e imagens
para testes visuais.

**APIs principais:** `SceneRenderer::drawScene`, `drawRig`, `drawSelection`,
`drawCollisionShapes`; `EditorCamera::camera`, `rayFromScreen`, `project`,
`handleInput`, `focusOn`.

**Autoria:** `drawEditorGuides` recebe `EditorGeometryView` (arestas/AABB/normais)
e hover opcional. Passe com teste de profundidade e sem escrita de profundidade;
não altera materiais nem é chamado pelo Player. O Studio alterna as opções por
F8/Shift+F8/Ctrl+F8. A seleção existente continua separada do contorno discreto.
As arestas começam desligadas; Visualizar → Arestas expõe o mesmo estado de F8.
O passe de guias usa bias em clip depth (`projection.m14 -= 0.0001`), apropriado
para GL_LINES, com flush e restauração da projeção; mantém o depth test e a
oclusão por paredes. Não se aplica polygon offset de triângulos a linhas.
AABB de esfera usa `position ± sphereRadius`, independente de rotação/tamanho
dos outros eixos, consistente com desenho, picking e Bullet.
As guias de esfera compartilham pontos e tesselação (7 faixas/12 setores) com a
superfície existente, evitando cordas de outra tesselação dentro das faces.

**Invariantes:** `linblux_renderer` não depende de ImGui/RmlUi; a câmera do
runtime é `Camera3D`; adaptadores de `EditorCamera` ficam na edição; render não
é dono da cena nem da física.

**Dependências:** raylib/OpenGL, `linblux_engine`; o target do renderer não
inclui Studio. O shader de céu é carregado como recurso.

**Ciclo de vida e erros:** renderer é criado/possuído pelo chamador; recursos
de GPU são liberados no destrutor; ausência de contexto gráfico deve permitir
skip/fallback nos testes.

**Testes:** `linblux_gizmo_visual`, `linblux_sky_capture`,
`linblux_graphics_quality`, `linblux_graphics_quality_1080`,
`linblux_rig_image`, `linblux_visual_studio` e testes de UI/render sob Xvfb.

**Como estender:** adicione passes sem introduzir dependência de UI; mantenha
separação câmera autoral/runtime; testes visuais devem comparar pixels e tolerar
software rendering.
`linblux_gizmo_visual` também verifica ausência de guias no padrão, continuidade
de aresta coplanar, hover totalmente oculto e AABB de esfera não uniforme.
Fixtures/capturas do executável real: `tests/mega3d_capture.py`, fases 0/1 em
`docs/relatorios/MEGA_UPDATE_3D_PROGRESSO.md`.

**Limites:** desempenho de GPU integrada não foi medido neste ambiente; o
renderer usa raylib/OpenGL e não há pipeline Vulkan. Testes visuais são lentos.
