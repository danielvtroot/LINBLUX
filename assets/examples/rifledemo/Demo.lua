--[[
RifleDemo: uma experiência real usando apenas APIs públicas.

O Modelo "RifleDemo" é um protótipo autoral (marcador _linblux_item_prototype),
registrado antes dos scripts. Aqui ele é instanciado no chão e usado pelo
jogador: E coleta, 1 equipa, clique atira, R recarrega e Delete solta.
]]
local v = linblux.vec3
local raiz = linblux.scene.root

-- O sistema do mundo possui os efeitos e sua limpeza. Destruir/desabilitar
-- o Script do item não cancela o descarte das Parts que já foram emitidas.
linblux.events.on("rifle.efeito",function(dados)
    local tracer=dados.kind=="tracer"
    local efeito=linblux.scene.create("part",{name=tracer and "Tracante" or "Faisca",
        position=v(dados.x,dados.y,dados.z),size=tracer and v(.22,.22,.22) or v(.32,.32,.32),
        color=v(255,210,110),anchored=not tracer,collidable=false,material="emissive"})
    efeito:set_attribute("efeito_rifle",true)
    if tracer then efeito.velocity=v(dados.dx*220,dados.dy*220,dados.dz*220) end
    local id=tonumber(efeito.id)
    task.spawn(function()
        task.wait(tracer and .9 or .08)
        local restante=linblux.scene.find(id)
        if restante then restante:destroy() end
    end)
end)

local dica, dicaRotulo

local function mostrarDica(texto)
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
        speed=0, respawn_delay=4, rig="player"}
    peca:set_attribute("alvo", true)
    return peca
end

criarAlvo(-8, -30)
criarAlvo(0, -30)
criarAlvo(8, -30)

local item = linblux.items.spawn("fuzil", {position=v(0, 1.6, -5)})
raiz:set_attribute("item_demo", item.id)

mostrarDica("Segure E perto do rifle para coletar")

linblux.events.on("inventory_changed", function(evento)
    if not evento.item or evento.item.id ~= item.id then return end
    if evento.action == "collect" then
        mostrarDica("1 equipa   ·   clique atira   ·   R recarrega   ·   Delete solta")
    elseif evento.action == "drop" then
        mostrarDica("Segure E para recolher o rifle")
    end
end)

-- Guia animado: a patrulha e os clipes usam os sistemas públicos de NPC.
local guia = linblux.npc.create{name="Guia LINBLUX", position=v(-14,4,-14),
    size=v(2,5,2), color=v(110,115,125), speed=6, rig="player"}
local pontos={v(-14,4,-14),v(-22,4,-22)}
local proximo=2
linblux.npc.move_to(guia,pontos[proximo])
linblux.events.on("update",function()
    local estado=linblux.npc.state(guia)
    if estado=="idle" or estado=="arrived" then
        proximo=proximo%2+1
        linblux.npc.move_to(guia,pontos[proximo])
    end
end)
