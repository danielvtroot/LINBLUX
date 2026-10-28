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
No caminho Compatível, guias e esfera compartilham 7 faixas/12 setores. No
caminho moderno, as guias usam arestas espaçadas da própria malha suave,
com a mesma rotação, evitando cordas de outra tesselação dentro das faces.

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


**Fase 3 / ModernMaterials:** GLSL 330 compartilhado por Studio e Player para
Parts/solo; normais por inversa transposta, iluminação por pixel, especular e
esfera suave. Presets Plástico/Concreto/Madeira/Metal/Vidro simples/Emissivo;
textura de cor, normal, metallic-roughness (G/B), oclusão R, UVs por dimensão da
face, tile/offset/rotação. Texturas da ImageAssets são premultiplicadas: o shader
recupera RGB antes de decodificar sRGB (apenas cor); mapas de dados ficam lineares.
Iluminação linear + tonemap + saída sRGB uma vez, fog em display-space. Céu,
decals premultiplicados e rigs permanecem nos seus passes existentes.

Médio é o padrão (difuso/especular por pixel); Baixo omite especular/normais/MR;
Alto usa GGX/Schlick com ambiente aproximado, **não PBR completo/IBL**.
Visualizar → Qualidade 3D ou `LINBLUX_RENDER_QUALITY=legacy|low|medium|high`;
Player também aceita `--quality`. Shader inválido/indisponível cai no caminho
CPU anterior. `LINBLUX_DISABLE_MATERIAL_SHADER=1` é injeção de falha para teste.
Malhas/shader são do renderer; mapas/texturas são da ImageAssets e não são
liberados ao destruir Material. Flush e restauração isolam shader, textura e
profundidade. Rigs/SpawnLocation decorativo ainda usam a aparência anterior.
Não inclui sombras, IBL, SSAO, batching nem ordenação de transparência (fase 4/5).

Regressão `linblux_material_render`: shader real, gradiente da esfera, quatro
mapas, alfa, restauração de estado/Legacy, fallback forçado e igualdade de pixels
entre chamadas Studio/Player com mesma câmera sem overlays; perfis em duas
resoluções. Fontes primárias consultadas para o contrato de matrizes/DrawMesh:
[vertex shader raylib 5.5](https://github.com/raysan5/raylib/blob/5.5/examples/shaders/resources/shaders/glsl330/lighting.vs),
[rmodels](https://github.com/raysan5/raylib/blob/5.5/src/rmodels.c).

**Fase 4 / profundidade e qualidade (28/10/2026):** `DirectionalShadow` possui
FBO/depth texture 1024², ajusta o frustum aos casters próximos da câmera e
estabiliza seu centro em texels. PCF 3×3, normal bias e depth bias proporcional
à inclinação/texel e profundidade do frustum; fade no limite de distância.
Parts/solo recebem sombras; Parts opacas e corpos dos rigs projetam sombras.
Rigs e marcadores SpawnLocation ainda recebem iluminação CPU, sem consultar
shadow map. Vidro não projeta sombra opaca; textura alfa usa corte de 0.5 no
passe de sombra (não há transmissão/colorização de sombras).

Lighting expõe `shadowsEnabled`, `shadowDistance` (16–300, padrão 80) e
`shadowStrength` (0–1). Campos JSON aditivos/opcionais, omitidos no padrão,
validação antes de modificar a cena; sem aumento de versão do formato.
Sombras em Médio/Alto, sem alocação/passe no Baixo/Compatível. Desligamento e
falha de shader/FBO mantêm a renderização disponível. `LINBLUX_DISABLE_SHADOWS=1`
permite testar a indisponibilidade. Contato vem da geometria real, sem quads
escuros, SSAO ou outro passe de oclusão acrescentado.

Passes: opacos e rigs → decals dos opacos → Parts translúcidas por distância
(do longe para perto), com decals associados → seleção. Transparência/decals
não gravam depth; transparência usa somente a superfície frontal para evitar
alfa duplicado. Mapas de cor são tratados conservadoramente no passe alfa.
Não há OIT: objetos translúcidos que se cruzam, grandes superfícies sobrepostas
e transparência de corpos de rigs ainda têm limites; a ordenação é por objeto,
não por triângulo. Decals mantêm o sombreamento CPU anterior.

`SceneTarget` suaviza **somente a cena**, compartilhada por Studio/Player:
Auto = Baixo/Compatível sem AA, Médio FXAA, Alto MSAA 4× real em renderbuffers
multisample, com resolve de cor/profundidade. Fallback MSAA → FXAA → sem AA;
indisponibilidade é memorizada, evitando recriação de MSAA a cada frame.
Visualizar → Antialiasing ou `LINBLUX_ANTIALIASING=auto|off|fxaa|msaa4`.
O FXAA antigo do Editor foi removido: UI/gizmos posteriores não recebem um
segundo filtro, RmlUi mantém seu próprio caminho. Sem conversão de gama extra.
O resolve copia depth entre FBOs compatíveis do Editor para preservar F8/hover;
o framebuffer padrão do Player recebe cor via quad, sem depender de depth
compatível com a janela. `FrameState` restaura FBOs de leitura/escrita,
matrizes, viewport, depth mask/test, culling, scissor, sRGB e multisample.
`LINBLUX_DISABLE_MSAA=1` / `LINBLUX_DISABLE_FXAA=1`: injeção de fallback.

`linblux_depth_quality`: sombra de contato, direção/distância/intensidade,
ausência de acne em piso vazio, custo desligado no Baixo, transparência
independente da inserção, seleção, depth de guias após resolve, AA real,
cores planas, fallback e redimensionamento nas duas resoluções. Teste gráfico
sempre pelo TURBO; mini benchmark llvmpipe não representa GPU integrada.
Contrato do depth FBO baseado na API pública e no
[exemplo oficial raylib 5.5](https://github.com/raysan5/raylib/blob/5.5/examples/shaders/shaders_shadowmap.c).
