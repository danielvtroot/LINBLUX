-- HUD personalizado com a vida atual
-- A barra acompanha o personagem pelo evento health_changed; o retrato usa
-- uma imagem opcional dos recursos da experiência (sem quebrar o HUD se ela
-- não existir, porque o caminho só é aplicado quando o arquivo está presente).

local MAX_HEALTH = 100

local hud = linblux.ui.create("panel", {
    anchor = "top-left", x = 20, y = 20, width = 300, height = 92,
    background = "#0F0F13E6",
})

local name = linblux.ui.create("label", {
    parent = hud, x = 16, y = 12, width = 200, height = 24,
    text = "Jogador", font_size = 16, color = "#F2F2F5",
})

local health_text = linblux.ui.create("label", {
    parent = hud, anchor = "top-right", x = -16, y = 12, width = 90, height = 24,
    text = "100 / 100", font_size = 14, color = "#B9B9C3",
})

local health_bar = linblux.ui.create("bar", {
    parent = hud, anchor = "bottom-left", x = 16, y = -18,
    width = 268, height = 16, max = MAX_HEALTH,
    color = "#4CD964", background = "#26262EFF",
})

local status = linblux.ui.create("label", {
    parent = hud, anchor = "bottom-left", x = 16, y = -36,
    width = 268, height = 16, text = "Saudável",
    font_size = 12, color = "#8A8A96",
})

local function paint(health)
    health = math.max(0, math.min(MAX_HEALTH, health))
    linblux.ui.set(health_bar, "value", health)
    linblux.ui.set(health_text, "text", string.format("%d / %d", math.floor(health + 0.5), MAX_HEALTH))
    if health <= 25 then
        linblux.ui.set(health_bar, "color", "#D7263F")
        linblux.ui.set(status, "text", "Estado crítico")
        linblux.ui.set(status, "color", "#D7263F")
    elseif health <= 60 then
        linblux.ui.set(health_bar, "color", "#E2A33A")
        linblux.ui.set(status, "text", "Ferido")
        linblux.ui.set(status, "color", "#E2A33A")
    else
        linblux.ui.set(health_bar, "color", "#4CD964")
        linblux.ui.set(status, "text", "Saudável")
        linblux.ui.set(status, "color", "#8A8A96")
    end
end

paint(linblux.player.health)

linblux.events.on("health_changed", function(health)
    paint(health)
end)

linblux.events.on("death", function()
    linblux.ui.set(status, "text", "Caído · respawn em instantes")
    linblux.ui.set(status, "color", "#D7263F")
end)

linblux.events.on("spawn", function()
    paint(linblux.player.health)
end)
