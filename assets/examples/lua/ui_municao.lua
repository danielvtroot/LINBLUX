-- Contador de munição (HUD)
-- Somente API pública: linblux.ui, linblux.events e linblux.audio.
-- F: dispara · R: recarrega · Tiros sem munição ficam bloqueados.

local MAX_AMMO = 30
local ammo = MAX_AMMO

local panel = linblux.ui.create("panel", {
    anchor = "bottom-right", x = -24, y = -24,
    width = 240, height = 104, background = "#0F0F13E6",
})

linblux.ui.create("label", {
    parent = panel, x = 16, y = 12, width = 200, height = 20,
    text = "MUNIÇÃO", font_size = 14, color = "#B9B9C3",
})

local counter = linblux.ui.create("label", {
    parent = panel, x = 16, y = 34, width = 200, height = 40,
    text = "", font_size = 30,
})

local meter = linblux.ui.create("bar", {
    parent = panel, anchor = "bottom-left", x = 16, y = -18,
    width = 208, height = 10, color = "#D7263F", background = "#3F3F49FF",
    max = MAX_AMMO,
})

local reload = linblux.ui.create("button", {
    parent = panel, anchor = "bottom-right", x = -16, y = -52,
    width = 104, height = 30, text = "Recarregar (R)",
    font_size = 14, background = "#26262EFF", color = "#F2F2F5",
})

local function paint()
    linblux.ui.set(counter, "text", string.format("%02d / %02d", ammo, MAX_AMMO))
    linblux.ui.set(counter, "color", ammo > 0 and "#F2F2F5" or "#D7263F")
    linblux.ui.set(meter, "value", ammo)
end

paint()

-- O exemplo funciona sem biblioteca de som: só toca se "tiro" existir.
local shot_sound = false
for _, sound in ipairs(linblux.audio.list()) do
    if sound.id == "tiro" then shot_sound = true end
end

linblux.events.on("key_pressed", function(key)
    if key == "f" then
        if ammo > 0 then
            ammo = ammo - 1
            if shot_sound then linblux.audio.play("tiro") end
            paint()
        end
    elseif key == "r" then
        ammo = MAX_AMMO
        paint()
    end
end)

linblux.ui.on(reload, "click", function()
    ammo = MAX_AMMO
    paint()
end)

linblux.ui.on(reload, "hover", function(info)
    linblux.ui.set(reload, "background", info.hovered and "#33333DFF" or "#26262EFF")
end)
