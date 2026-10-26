# Interação e controles de itens

Itens world recebem coleta por proximidade: segure E por0,6 s, ou botão oeste
(X) do controle. Soltar a tecla, mudar de alvo, afastar-se ou abrir o menu
cancela o progresso. O teste de alcance usa a geometria, não o pivô do Modelo;
raycast impede interação através de obstáculos. Uma conclusão exige liberar
a tecla antes de outra ativação. Não há prompt sem alvo válido.

A interação também serve a portas, baús e dispositivos por Lua:

```lua
linblux.interactions.register(porta, {
    label="Abrir porta", distance=6, hold_seconds=0.6, key="e"
})
linblux.events.on("interaction_completed", function(evento)
    if evento.object and evento.object.id==porta.id then
        porta.rotation = linblux.vec3(0,90,0)
    end
end)
```

register substitui a configuração do mesmo objeto; unregister remove (e
suprime registro automático de item até novo register). prompt() expõe objeto,
nome e progresso reais para a apresentação personalizada. Objetos precisam ser
Parts ou Modelos com geometria. Limites:256 entradas, distância0,25–20,
duração0,05–10 s; teclas da lista pública, sem Esc/F1–F12 reservadas ao host.
Máximo de quatro candidatos com teste de linha de visão por atualização.

A hotbar é compartilhada entre Studio/Play e Player independente. Fica oculta
sem itens equipáveis; usa nome ou ícone da biblioteca, destaque vermelho e
somente os slots ocupados. Teclas1–9 selecionam a página atual; pressionar a
mesma tecla desequipa. PgUp/PgDn alternam páginas; bumpers do controle percorrem
itens. Delete solta o equipado com física e preserva identidade/dados/scripts.
Ausência de espaço mantém o item e mostra erro real. Recolher usa E novamente.
Objetos sem equipable continuam na bolsa, mas não criam slot.

O menu captura a entrada antes do runtime: controles de jogo não executam
quando pausado. A apresentação de progresso/hotbar usa retângulos, texto e
texturas já gerenciadas, sem framebuffer extra nem alocação de vídeo.
Exemplo público: assets/examples/lua/interacao_porta.lua.

Testes nativos cobrem cancelamento, obstáculo, conclusão única, coleta,
seleção, Delete/recoleta e dados preservados. O teste gráfico separado
linblux_item_controls_ui usa teclado X11, capturas720p/1080p e paginação. A
simulação de bits de controle não comprova teste em hardware de gamepad.
