# Módulo `editor`

**Responsabilidade:** lógica de autoria do Studio — UI ImGui, painéis, seleção,
histórico, gizmos, transformações, câmera de edição e adaptadores de render.

**Fonte:** `src/editor/`, `src/rendering/EditorCamera.{h,cpp}`; target
`linblux_editing` + parte de `linblux_studio`.

**Entradas:** cena autoral, seleção, eventos de teclado/mouse, ações de UI,
snapshots de transformação e configurações de câmera/gizmo.

**Saídas:** mutações na cena (via comandos/transformações), histórico de
desfazer/refazer, câmera e estado de UI. Não possui física runtime nem Lua.

**APIs principais:** `EditHistory`, `TransformSession`, `ModelTransformSession`, `Gizmo`,
`GizmoLayout`, `TransformMetrics`, `EditorCamera`, `EditorUI`.

**Encaixe de construção:** `gizmomath::SnapSurfaces` usa caixas orientadas e SAT;
Ctrl durante Mover fornece os volumes reais da seleção/filhos e dos alvos.
Correções respeitam eixo/plano, rejeitam interseções adicionais e participam do
histórico existente. Shift libera a grade. Limites em `docs/EDITOR_PRECISAO.md`;
sem rotação automática, malhas ou encaixe dedicado de vértices. Testes numéricos
em `linblux_studio`, marcador e gizmos em `linblux_gizmo_visual`.

**Invariantes:** transformações preservam hierarquia e IDs; undo/redo restaura
estado anterior; edição não cria corpos físicos nem executa scripts; a câmera
de edição não é a câmera do Player.

**Dependências:** `linblux_engine`, raylib, ImGui/RmlUi na camada de Studio.
A câmera de edição é convertida para `Camera3D` no ponto de desenho.

**Ciclo de vida e erros:** `EditorUI` pertence ao `Application`; operações de
transformação podem ser canceladas; validações retornam `bool`/mensagem. Não há
thread própria.

**Testes:** `linblux_gizmo_visual`, `linblux_editor_input`, `linblux_visual_studio`,
`linblux_studio_ui` e testes de manipulação/inputs; exigem Xvfb para o caminho
gráfico.

**Como estender:** adicione uma operação como comando de histórico, com estado
antes/depois e teste de undo; não escreva diretamente em `Scene` sem listener de
remoção/física. Mantenha a separação `editing` → `engine`.

**Limites:** a UI ainda compartilha `Scene` e listeners com o domínio; testes
visuais dependem de software rendering e podem ser lentos. O parser ATLAS não
prova chamadas de UI.

**Modelos:** `ModelTransformSession` reutiliza o gizmo com proxy fora da cena e
do Bullet. Captura Parts e pivôs aninhados para um único undo/redo por gesto;
cancelamento restaura inclusive pivôs implícitos. Valida todas as referências
antes da restauração e suspende/libera os corpos filhos durante o arraste.
Histórico limitado a 128 operações/32.768 poses. Regressão headless em
`linblux_studio` (`ModelGizmoSession`); interação X11, propriedades, salvar/Play
e undo composto em `linblux_models_ui` (TURBO obrigatório, timeout de 60 s).
Agrupar/desagrupar não oferece undo estrutural; não cria soldas físicas.
