-- Notificação interativa
-- Um card com dois botões: aceitar ou dispensar. Clicar em qualquer botão
-- resolve a notificação, escreve o resultado no Output e agenda a próxima.

local panel = linblux.ui.create("panel", {
    anchor = "top", x = 0, y = 24, width = 420, height = 132,
    background = "#0F0F13F2",
})

local title = linblux.ui.create("label", {
    parent = panel, x = 18, y = 14, width = 384, height = 24,
    text = "Nova missão disponível", font_size = 18, color = "#F2F2F5",
})

local detail = linblux.ui.create("label", {
    parent = panel, x = 18, y = 40, width = 384, height = 22,
    text = "Colete 3 cristais para liberar a passagem norte.",
    font_size = 14, color = "#B9B9C3",
})

local accept = linblux.ui.create("button", {
    parent = panel, anchor = "bottom-right", x = -132, y = -16,
    width = 112, height = 34, text = "Aceitar",
    font_size = 15, background = "#D7263F", color = "#FFFFFF",
})

local dismiss = linblux.ui.create("button", {
    parent = panel, anchor = "bottom-right", x = -18, y = -16,
    width = 104, height = 34, text = "Dispensar",
    font_size = 15, background = "#26262E", color = "#F2F2F5",
})

local resolved = false

local function hide()
    linblux.ui.set(panel, "visible", false)
end

local function resolve(answer, message)
    if resolved then return end
    resolved = true
    print("Notificação:", answer)
    linblux.ui.set(title, "text", answer == "aceita" and "Missão aceita" or "Missão dispensada")
    linblux.ui.set(detail, "text", message)
    linblux.ui.set(accept, "visible", false)
    linblux.ui.set(dismiss, "visible", true)
    linblux.ui.set(dismiss, "text", "Fechar")
    linblux.task.wait(3)
    hide()
end

linblux.ui.on(accept, "click", function()
    resolve("aceita", "3 cristais restantes · siga para o norte.")
end)

linblux.ui.on(dismiss, "click", function()
    if resolved then
        hide()
        return
    end
    resolve("dispensada", "A notificação foi arquivada.")
end)

linblux.ui.on(accept, "hover", function(info)
    linblux.ui.set(accept, "background", info.hovered and "#E23A55" or "#D7263F")
end)

linblux.ui.on(dismiss, "hover", function(info)
    linblux.ui.set(dismiss, "background", info.hovered and "#33333D" or "#26262E")
end)

-- Mostra que a notificação pode ser recriada dinamicamente pelo jogo.
linblux.events.on("key_pressed", function(key)
    if key == "n" and resolved then
        resolved = false
        linblux.ui.set(panel, "visible", true)
        linblux.ui.set(title, "text", "Nova missão disponível")
        linblux.ui.set(detail, "text", "Colete 3 cristais para liberar a passagem norte.")
        linblux.ui.set(accept, "visible", true)
        linblux.ui.set(dismiss, "visible", true)
        linblux.ui.set(dismiss, "text", "Dispensar")
    end
end)
