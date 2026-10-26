# UI por Lua — modelo declarativo no Studio e no Player

Base remota: `2f7cd5dc30c712966e5972ad705a8bcaae96f8e2`. Áudio e as regressões de
reinício já estavam publicados e não foram refeitos.

## O que foi implementado

- `game::UiModel` (`src/game/Ui.h/.cpp`): árvore limitada de `panel`, `label`,
  `button`, `image` e `bar`, com posição, tamanho, âncoras, cor, fundo,
  transparência, visibilidade, `z`, texto, imagem, `value`/`max` e `font_size`.
  Layout em tela virtual 1280×720 escalada por `min(w/1280, h/720)`, filhos
  resolvidos dentro do retângulo do pai.
- Dono por script, ids monotônicos e validação de todas as escritas. Limites:
  128 elementos, profundidade 4, 64 mutações por quadro, 64 eventos, texto de
  512 bytes, imagem precisa existir nos `assets/` da experiência.
- `linblux.ui` em `LuaRuntime`: `create`, `set`, `get`, `destroy`, `on`
  (`click`/`hover`), `off`, `count`. Callbacks rodam como tarefas Lua, com as
  mesmas cotas; `uiClick`/`uiHover` chegam pela fila do runtime, nunca durante
  o render.
- Limpeza: destruir o pai destrói filhos; `cancelScript` desabilita o script,
  remove elementos e handlers; `RuntimeSession::stop` limpa o modelo.
- `RuntimeSession` drena cliques/hover no `update`; o host só informa o ponto
  no viewport.
- Renderização: `loblox::DrawLuaUi` (`src/rendering/LuaUiRenderer.h/.cpp`) desenha
  com raylib na textura do Play do Studio e na tela do Player — um layout, dois
  hosts. No Studio, cliques são ignorados enquanto o botão direito captura a
  câmera; no Player, com o cursor escondido.

## Exemplos publicados (somente API pública)

- `assets/examples/lua/ui_municao.lua`: contador de munição com barra, tecla F,
  recarga por R e botão.
- `assets/examples/lua/ui_notificacao.lua`: notificação com aceitar/dispensar,
  hover, recriação e `task.wait`.
- `assets/examples/lua/ui_hud_vida.lua`: HUD de vida ligado a `health_changed`,
  estados de dano e morte.

## Evidência

- `linblux_lua_interface` (headless): modelo, `get/set`, âncoras (centro e
  bottom-right em 1280×720), escala em 640×360, clique entregue ao callback,
  hover enter/leave, limpeza ao desabilitar o script e ao parar, erros de handle
  inválido, tipo, texto >512, imagem ausente, profundidade, orçamento de
  mutações, evento inválido e escrita entre scripts; e os três exemplos
  (munição decrementa com F e recarrega pelo botão; notificação aceita; HUD
  acompanha dano até 20/100 e “Estado crítico”). FAST 15/15.
- `linblux_lua_ui_render` (gráfico): painel ancorado desenhado e clicado em
  960×600 e 1280×720, verificado por pixels do framebuffer.
- `linblux_lua_ui_player` (gráfico, executável real): `ui_hud_vida.lua` valida
  com `--validate` e é pintado na janela do Player (4276 pixels da barra).
- `linblux_visual_studio` (gráfico, Studio real): durante o Play, o painel Lua é
  encontrado no framebuffer; 213 verificações, 0 falhas.

## Limitações

- Sem imagens animadas, fontes por elemento, máscaras, gradientes, listas,
  scroll, drag ou campos de texto. Novos tipos exigem C++.
- A UI não é serializada no lugar: o script é a fonte da verdade e recria os
  elementos em cada Play.
- Não substitui o HUD protegido nem o RmlUi do Studio; é uma camada adicional
  dentro do viewport.
- O mouse do Studio só entrega cliques à UI quando a câmera não está capturada.
- Próximos passos Alpha: combate genérico/RifleDemo, HUD circular, menu lateral,
  Demo integrada, pacote Linux e revisão final.
