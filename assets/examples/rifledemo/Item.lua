--[[
Comportamento de um item equipável do RifleDemo.

Este script não é chamado pelo C++ como "arma": ele usa a mesma API pública de
qualquer item. A engine fornece o estado genérico (linblux.items), o Attachment
de origem e a física; aqui ficam som, efeito, dano e HUD.
]]
local v = linblux.vec3
local arma = linblux.self

local PENTE, RESERVA, RECARGA, INTERVALO = 12, 24, 1.6, 0.16
local SOM_TIRO = "assets/tiro.wav"
local SOM_IMPACTO = "assets/impacto.wav"
local SOM_RECARGA = "assets/recarga.wav"
local SOM_EQUIPAR = "assets/equipar.wav"
local SOM_VAZIO = "assets/vazio.wav"

local painel, rotulo, ferrolho, boca

for _, filho in ipairs(arma:descendants()) do
    if string.sub(filho.name, 1, 8) == "Ferrolho" then ferrolho = filho end
end
-- A boca pertence à Part do cano, não diretamente ao Modelo. Percorra a
-- hierarquia real; cópias recebem sufixos e são localizadas pelo prefixo.
for _, filho in ipairs(arma:descendants()) do
    if string.sub(filho.name, 1, 10) == "BocaDoCano" then boca = filho end
end

local function equipada()
    return linblux.items.equipped() == arma
end

local function tocar(referencia, volume)
    if not linblux.audio.device_ready() then return end
    linblux.audio.play(referencia, {volume=volume})
end

local function hud()
    local estado = linblux.items.combat(arma)
    if not estado then return end
    if not equipada() then
        if painel then
            linblux.ui.destroy(painel)
            painel, rotulo = nil, nil
        end
        return
    end
    if not painel then
        painel = linblux.ui.create("panel", {anchor="bottom-right", x=-24, y=-24,
            width=232, height=64, background="#101014E6"})
        rotulo = linblux.ui.create("label", {parent=painel, anchor="center",
            width=210, height=28, text="", color="#FFFFFF", font_size=18})
    end
    local texto
    if estado.reloading then
        texto = "RECARREGANDO "..math.floor(estado.reload_progress * 100).."%"
    else
        texto = estado.ammo.." / "..estado.magazine.."   reserva "..estado.reserve
    end
    linblux.ui.set(rotulo, "text", texto)
end

local function faisca(posicao)
    local brilho = linblux.scene.create("part", {name="Faisca", position=posicao,
        size=v(0.32, 0.32, 0.32), color=v(255, 196, 92), anchored=true,
        collidable=false, material="emissive"})
    task.spawn(function()
        task.wait(0.2)
        if brilho then brilho:destroy() end
    end)
end

local function recuar()
    if not ferrolho or not boca then return end
    local direcao = boca.direction
    local antes = ferrolho.position
    ferrolho.transform = {position=v(antes.x - direcao.x * 0.22, antes.y - direcao.y * 0.22,
        antes.z - direcao.z * 0.22), rotation=ferrolho.rotation}
    task.spawn(function()
        task.wait(0.07)
        if not ferrolho then return end
        local agora = ferrolho.position
        ferrolho.transform = {position=v(agora.x + direcao.x * 0.22, agora.y + direcao.y * 0.22,
            agora.z + direcao.z * 0.22), rotation=ferrolho.rotation}
    end)
end

local function atirar()
    if not equipada() or not boca then return end
    local ok, motivo = linblux.items.activate(arma)
    if not ok then
        if motivo == "Sem munição." then tocar(SOM_VAZIO, 0.35) end
        return
    end
    recuar()
    tocar(SOM_TIRO, 0.7)
    local origem = boca.world_position
    local direcao = boca.direction
    faisca(v(origem.x + direcao.x * 0.4, origem.y + direcao.y * 0.4, origem.z + direcao.z * 0.4))
    -- Traçante é uma Part física real: a física existente a move e o tempo a limpa.
    local tracer = linblux.scene.create("part", {name="Tracante",
        position=v(origem.x + direcao.x * 2, origem.y + direcao.y * 2, origem.z + direcao.z * 2),
        size=v(0.18, 0.18, 0.18), color=v(255, 224, 130), anchored=false,
        collidable=false, material="emissive"})
    tracer.velocity = v(direcao.x * 220, direcao.y * 220, direcao.z * 220)
    task.spawn(function()
        task.wait(0.9)
        if tracer then tracer:destroy() end
    end)
    -- Dano: consulta física real a partir do Attachment, ignorando o próprio item.
    local acerto = linblux.physics.raycast(origem, direcao, 200, arma)
    if acerto then
        faisca(acerto.point)
        tocar(SOM_IMPACTO, 0.5)
        local atingido = acerto.object
        if atingido and atingido:get_attribute("alvo") then
            linblux.npc.damage(atingido, 34)
        end
    end
end

linblux.items.configure(arma, {magazine=PENTE, reserve=RESERVA, consume=1,
    interval=INTERVALO, reload_time=RECARGA})

linblux.events.on("item_action", function(evento)
    if not evento.item or evento.item.id ~= arma.id then return end
    if evento.action == "reload_start" then tocar(SOM_RECARGA, 0.6) end
    if evento.action == "reload_end" then tocar(SOM_RECARGA, 0.45) end
    hud()
end)

linblux.events.on("inventory_changed", function(evento)
    if evento.item and evento.item.id == arma.id and evento.action == "equip" then
        tocar(SOM_EQUIPAR, 0.5)
    end
    hud()
end)

linblux.events.on("update", function(dt)
    hud()
    if not equipada() then return end
    if linblux.input.key_pressed("r") then
        local ok, motivo = linblux.items.reload(arma)
        if not ok and motivo == "Pente já está cheio." then tocar(SOM_VAZIO, 0.3) end
    end
    if linblux.input.mouse_down("left") then atirar() end
end)

linblux.events.on("destroy", function()
    if painel then linblux.ui.destroy(painel); painel = nil end
end, arma)
