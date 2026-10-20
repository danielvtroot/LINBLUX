# Entrada e validação integrada

## Contrato e atalhos

`EditorUI::tool_` é a fonte da ferramenta ativa. `setTool` é o único ponto de
troca; registra origem (`toolbar`, `menu`, `shortcut`), ferramenta anterior e
nova. Reativar a ferramenta atual é uma operação sem efeito, inclusive durante
drag. A tecla de ferramenta não repete automaticamente.

| Entrada | Contexto e ação |
| --- | --- |
| 1 / 2 / 3 / 4 | Viewport focada: Select / Move / Scale / Rotate |
| LMB | Handle inicia drag; fora dos handles seleciona por raycast Bullet |
| RMB + mouse | Inicia Look dentro da imagem, mantém captura até soltar |
| MMB + mouse | Inicia Pan dentro da imagem, mantém captura até soltar |
| WASD / Q E | Câmera com viewport focada ou navegação ativa; Ctrl/Alt/Super bloqueiam movimento |
| Shift | Movimento rápido de câmera; desativa snap durante transformação |
| Scroll | Zoom sobre a imagem, sem transformação/navegação ativa |
| F | Enquadra seleção com viewport focada, fora de drag |
| Delete / Backspace | Remove seleção com viewport focada; durante drag cancela antes de remover |
| Ctrl+S | Salva; confirma uma transformação ativa antes da escrita |
| Ctrl+D | Duplica a Part selecionada na mesma posição, com novo ID/nome; seleciona a cópia. Confirma drag ativo antes de copiar. |
| Ctrl+O | Cancela transformação e abre o modal de projeto |
| Esc | Cancela transformação ou captura; campos/modais conservam o tratamento do ImGui |

Texto, widgets ativos e popups bloqueiam atalhos e navegação. Uma sessão ativa
é encerrada ao perder foco, ocultar a viewport ou abrir UI bloqueante. Navegação
ativa consome seus botões/teclas: LMB não seleciona e 1–4 não trocam ferramentas.
Hover de um gizmo sozinho não impede RMB/MMB nem WASD.

## Caminho efetivo dos eventos

1. GLFW/raylib coleta eventos em `EndDrawing`; rlImGui os entrega a ImGui antes
   de `NewFrame`, incluindo perda de foco (que limpa as teclas/botões do ImGui).
2. Menus, ribbon, Explorer, Properties e modais registram suas ações/itens.
3. A viewport registra `##viewport_input`, um InvisibleButton com a imagem
   desenhada por AddImage. Diferentemente de Image sem ID, isso impede o ImGui
   de atribuir o clique ao MoveId da janela. Seu ActiveId pertence à viewport.
4. `ReadEditorInput` coleta uma vez teclas/botões/scroll do ImGui e delta relativo
   da raylib; `InputContext` reúne foco, hover, widget e popup. WantCaptureMouse
   é diagnóstico: a própria imagem pertence ao ImGui e precisa receber entrada.
5. `RouteInput` escolhe cancelamento, sessão ativa, câmera, viewport ou UI.
   Os atalhos passam pela mesma decisão e usam o foco deste frame.
6. `TransformSession` processa o gizmo; um clique consumido nunca chega à seleção.
   Câmera recebe o snapshot; seleção só recebe LMB pertencente à viewport.

Coordenadas de mouse e imagem são lógicas de janela. O raio usa posição relativa
à imagem e o tamanho da RenderTexture. AddImage inverte apenas UV Y; o picking
continua com origem no topo. rlImGui aplica DisplayFramebufferScale na saída.
O aplicativo Linux não habilita FLAG_WINDOW_HIGHDPI; escala real de compositor
e transições entre monitores ainda requerem teste no ambiente de destino.

Desenho e picking usam `gizmolayout::Build`. A profundidade é isolada só no passe
do gizmo, com flush antes/depois; nenhuma política de depth da cena foi alterada.
Pivô atrás da câmera/near plane de 0,05 não gera overlay/picking. Scale continua
com seis handles separados e guias para faces; Rotate com três anéis/marcadores.
Em vistas axiais/anel de perfil, uma projeção em tela congelada no Begin evita
interseções singulares. Fora delas, conserva-se a interseção no mundo.

## Diagnóstico

Execução normal registra transições, sem posição de mouse por frame. Ative
`LOBLOX_TRACE_INPUT=1` para dispatch de botões, atalhos, scroll e mudança de hover.
Arquivo padrão: `$XDG_STATE_HOME/loblox/loblox.jsonl`, ou
`$HOME/.local/state/loblox/loblox.jsonl`. Rotação em 1 MiB, backup `.1`.

Os nomes são pares `subsystem.event`, armazenados em duas colunas JSON:

| Par | Evidência |
| --- | --- |
| tool.change | Origem, ferramenta anterior/nova |
| input.dispatch | Consumidor final, motivo, resultado, bordas de botões, ação de teclado, captura/foco |
| camera.capture_begin / capture_end | Início/fim de Look/Pan |
| gizmo.hover / drag_begin / drag_commit / drag_cancel | Handle, entidade, ferramenta, motivo, correlação da sessão |
| gizmo.drag_update_error | Interseção indisponível; mantém transform anterior; não repete o mesmo erro continuamente |
| physics.edit_begin / edit_end | Suspensão/liberação de corpo dinâmico |
| scene.selection_change | IDs anterior e novo, inclusive exclusão |
| persistence.save_begin / save_end / load_begin / load_end | Arquivo, formato, duração, resultado e erro específico |
| invariants.violation | Operação e invariante violada |

`save_end`/`load_end` com `result:error` são os eventos de falha existentes.
Todo evento tem frame e correlation_id; begin/end de drag compartilham a
correlação da sessão, operações de arquivo têm correlação própria. Eventos
sem correlação explícita compartilham a correlação do frame. Para investigar
uma troca inesperada, comece em tool.change e compare input.dispatch do mesmo
frame. O texto digitado em campos não é registrado.

## Regressão automatizada

`cmake --build build -j2 && ctest --test-dir build --output-on-failure`

Além da suíte headless e do render/picking, `loblox_editor_input` envia eventos
X11 reais por xdotool: toolbar, três drags, câmera, teclado repetido, Esc, troca
de ferramenta, perda de foco para a janela raiz, modal, digitação, save e delete
durante drag. Verifica transforms, seleção, câmera e estado Bullet. Fixture:
1280×800, fonte padrão ImGui, layout sem ini; screenshot/log em `/tmp/loblox-input-*`.

`loblox_editor_restart` inicia o executável real quatro vezes em diretório isolado,
salva por Ctrl+S e encerra por WM_DELETE_WINDOW. Compara IDs, transforms,
propriedades, exclusão e nextId com tolerância numérica. Nenhum projeto pessoal
é utilizado. Requer Python 3; testes de entrada requerem Xvfb, xdotool e headers
X11. A ausência dessas ferramentas deixa a camada gráfica fora da validação.

Sway/XWayland também foi exercitado em compositor privado headless/pixman:
204 verificações de entrada e os quatro reinícios passaram. Para repetir como
usuário comum: `bash tests/run_sway_input.sh`. O script cria runtime próprio,
usa somente seu display XWayland e encerra seu compositor ao terminar.

ASan/UBSan headless passou com detect_leaks=1. A validação gráfica com LSan
encontrou e permitiu corrigir o material dos studs não liberado (336 bytes).
Persistem ~194 kB na pilha Mesa/GLX, reproduzidos inclusive sem Loblox, apenas
com InitWindow/CloseWindow. Não houve supressões nem detect_leaks=0. Portanto
a suíte completa de sanitizers permanece reprovada nessa instalação gráfica.
O teste funcional normal e o Debug completo passam. O pkg-config reporta raylib
6.2.0, enquanto os headers/runtime instalados se identificam como 6.5-dev.

## Procedimento manual no notebook (pendente)

Abra uma cena de teste isolada, no mesmo terminal:

```bash
session_dir="$(mktemp -d /tmp/loblox-manual.XXXXXX)"
XDG_DATA_HOME="$session_dir/data" XDG_STATE_HOME="$session_dir/state" \
LOBLOX_TRACE_INPUT=1 ./build/loblox 2>&1 | tee "$session_dir/console.log"
```

1. Crie um Block. Alterne Move → Scale → Rotate → Select → Move pela ribbon e
   por 2 → 3 → 4 → 1 → 2. Confira indicação, overlay e Properties.
2. Arraste cada eixo de Move, cada uma das seis faces de Scale e os três anéis.
   Use zoom próximo/distante e vistas quase alinhadas aos eixos. A face oposta
   de Scale deve ficar fixa; Move/Rotate não mudam Size.
3. Faça Begin → Esc → Begin → soltar → trocar ferramenta. Durante drag,
   pressione RMB/WASD; a câmera não deve responder. Navegue com RMB sobre um
   handle; clique LMB e pressione 1–4 durante navegação: seleção/ferramenta ficam.
4. Durante drag, abra Ctrl+O, cancele, depois repita e alterne para outro app no
   Sway. Volte e arraste novamente. Nenhum corpo/cursor deve ficar preso.
5. Digite `1234wasd` em Name e números em Position/Rotation/Size; ferramenta e
   câmera não devem reagir. Termine o campo e faça Ctrl+S sem mover a câmera.
6. Feche normalmente. Execute novamente o mesmo comando (sem redefinir
   session_dir); confirme objetos, nomes, Position, Rotation, Size, cor/material.

Colete console.log, state/loblox/loblox.jsonl e eventual `.1`, a sequência exata
de ações e um screenshot do conflito. O projeto de teste fica em
`data/loblox/projects/default.loblox.json`. A sessão física Sway/XWayland, GPU, DPI,
sessões longas e cenas grandes ainda não podem ser declarados aprovados.
