# UI por Lua — `linblux.ui`

Interfaces de experiência construídas por scripts, sem C++ e sem tocar na UI
protegida do Studio. O modelo é declarativo, pertence ao Play e é desenhado pelo
mesmo renderizador no Studio (dentro da textura do viewport) e no Player (na
tela), então a posição de um botão e a área de clique são idênticas nos dois.

## Modelo

- Até **128 elementos** por sessão, **profundidade 4**, **64 alterações por
  quadro**, **64 eventos** e **512 bytes** por texto.
- Unidades de projeto em uma tela virtual **1280×720**; a escala é
  `min(largura/1280, altura/720)`. Âncoras relativas ao viewport (ou ao pai)
  mantêm o layout em qualquer resolução.
- Cada elemento pertence ao script que o criou. Outro script não pode ler,
  alterar, destruir nem registrar eventos nele.
- `id` é monotônico: um handle antigo nunca aponta para outro elemento; ele
  falha com erro explícito depois de destruído.
- Destruir um pai destrói os filhos. Desabilitar/remover o script (ou terminar
  o Play) apaga seus elementos e callbacks.

## Tipos

| tipo | uso |
| --- | --- |
| `panel` | fundo arredondado, agrupa filhos |
| `label` | texto simples |
| `button` | texto + área clicável/hover |
| `image` | imagem dos recursos da experiência |
| `bar` | indicador com `value`/`max` |

## API

```lua
local id = linblux.ui.create(tipo, propriedades?)  -- devolve handle inteiro
linblux.ui.set(id, chave, valor)
linblux.ui.get(id, chave)
linblux.ui.destroy(id)
linblux.ui.on(id, "click" | "hover", função)
linblux.ui.off(id, evento)
linblux.ui.count()                                 -- elementos do script atual
```

`create` aplica as propriedades na mesma chamada. `parent` cria hierarquia.

### Propriedades

| chave | valores |
| --- | --- |
| `x`, `y` | deslocamento em unidades de projeto, aplicado depois da âncora |
| `width`, `height` | 0..4000 (padrão por tipo quando omitido) |
| `anchor` | `top-left` (padrão), `top`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right` |
| `text` | string até 512 bytes |
| `color` | `#RRGGBB`/`#RRGGBBAA` ou `{r=,g=,b=,a=}` (0..255) |
| `background` | idem, para painéis/botões/barras |
| `opacity` | 0..1; `transparency` é o inverso |
| `visible` | booleano |
| `font_size` | 8..96 |
| `value`, `max` | indicador (`bar`) |
| `z` | ordem visual, -1000..1000 (empate pela ordem de criação) |
| `image` | caminho `assets/...` existente nos recursos da experiência |

### Eventos

`click` recebe `{id, kind, event="click", text}`.
`hover` recebe `{id, kind, event="hover_enter"|"hover_leave", hovered=true|false, text}`.

Só `button` recebe ponteiro: painéis não bloqueiam cliques da câmera. O Studio
não entrega cliques enquanto o botão direito captura a câmera; o Player não
entrega cliques com o cursor escondido.

## Exemplos publicados

- `assets/examples/lua/ui_municao.lua` — contador de munição com barra, tecla de
  disparo e botão de recarga.
- `assets/examples/lua/ui_notificacao.lua` — notificação com aceitar/dispensar,
  hover e recriação.
- `assets/examples/lua/ui_hud_vida.lua` — HUD de vida ligado ao personagem por
  `health_changed`, com estados e morte.

Os três rodam no `linblux_ui_lua` (headless) e foram desenhados no
`linblux_lua_ui_render` (framebuffer 960×600 e 1280×720).

## Limites conhecidos

- Sem imagens animadas, fontes por elemento (família fixa), máscaras, gradientes
  ou layout automático por fluxo de texto.
- Sem texto editável, listas, scroll ou drag; novos tipos exigem C++.
- A UI não é salva no lugar: o script é a fonte da verdade e recria os elementos
  ao iniciar o Play.
- Não substitui o HUD protegido (vida, hotbar, pausa) nem o RmlUi do Studio.
- Um callback corre como tarefa Lua normal, com as mesmas cotas de instrução.
