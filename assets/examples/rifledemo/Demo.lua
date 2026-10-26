--[[
RifleDemo: uma experiência real usando apenas APIs públicas.

O Modelo "RifleDemo" é um protótipo autoral (marcador _linblux_item_prototype),
registrado antes dos scripts. Aqui ele é instanciado no chão e usado pelo
jogador: E coleta, 1 equipa, clique atira, R recarrega e Delete solta.
]]
local v = linblux.vec3
local raiz = linblux.scene.root

local dica, dicaRotulo

local function dica(texto)
    if not dica then
        dica = linblux.ui.create("panel", {anchor="top", x=0, y=16, width=620,
            height=46, background="#101014CC"})
        dicaRotulo = linblux.ui.create("label", {parent=dica, anchor="center",
            width=600, height=24, text=texto, color="#F2F2F5", font_size=16})
        raiz:set_attribute("dica_painel", dica)
        raiz:set_attribute("dica_rotulo", dicaRotulo)
    else
        linblux.ui.set(dicaRotulo, "text", texto)
    end
end

local function criarAlvo(x, z)
    local peca = linblux.npc.create{name="Alvo", position=v(x, 4, z),
        size=v(2.4, 7, 2.4), color=v(214, 66, 58), max_health=100,
        speed=0, respawn_delay=4}
    peca:set_attribute("alvo", true)
    return peca
end

criarAlvo(-7, -30)
criarAlvo(7, -30)

local item = linblux.items.spawn("fuzil", {position=v(0, 1.6, -9)})
raiz:set_attribute("item_demo", item.id)

dica("Segure E perto do rifle para coletar")

linblux.events.on("inventory_changed", function(evento)
    if not evento.item or evento.item.id ~= item.id then return end
    if evento.action == "collect" then
        dica("1 equipa   ·   clique atira   ·   R recarrega   ·   Delete solta")
    elseif evento.action == "drop" then
        dica("Segure E para recolher o rifle")
    end
end)
