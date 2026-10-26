--[[
Comportamento de um item equipável do RifleDemo.

Este script não é chamado pelo C++ como "arma": ele usa a mesma API pública de
qualquer item. A engine fornece o estado genérico (linblux.items), o Attachment
de origem e a física; aqui ficam som, efeito, dano e HUD.
]]
local v = linblux.vec3
local arma = linblux.self

local PENTE, RESERVA, RECARGA, INTERVALO = 12, 24, 1.6, 0.16
local SOM_TIRO = "tiro"
local SOM_IMPACTO = "impacto"
local SOM_RECARGA = "recarga"
local SOM_EQUIPAR = "equipar"
local SOM_VAZIO = "vazio"

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
    linblux.audio.play(referencia, {volume=volume, owner=arma})
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
    linblux.events.emit("rifle.efeito",{kind="flash",x=posicao.x,y=posicao.y,z=posicao.z})
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

-- Efeitos são Parts consultáveis como qualquer objeto. Pule somente nossos
-- efeitos visuais, com trabalho limitado; uma parede continua bloqueando o tiro.
local function consultarDisparo(origem, direcao)
    local restante = 200
    for _ = 1, 8 do
        local hit = linblux.physics.raycast(origem, direcao, restante, arma)
        if not hit or not hit.object:get_attribute("efeito_rifle") then return hit end
        restante = restante - hit.distance - 0.5
        if restante <= 0 then return nil end
        origem = v(hit.point.x + direcao.x*0.5, hit.point.y + direcao.y*0.5, hit.point.z + direcao.z*0.5)
    end
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
    -- Consulte antes de criar efeitos: o traçante também é uma Part visível.
    local acerto = consultarDisparo(origem, direcao)
    faisca(v(origem.x + direcao.x * 0.4, origem.y + direcao.y * 0.4, origem.z + direcao.z * 0.4))
    -- Traçante é uma Part física real: a física existente a move e o tempo a limpa.
    linblux.events.emit("rifle.efeito",{kind="tracer",
        x=origem.x+direcao.x*2,y=origem.y+direcao.y*2,z=origem.z+direcao.z*2,
        dx=direcao.x,dy=direcao.y,dz=direcao.z})
    -- Dano: consulta física real a partir do Attachment, ignorando o próprio item.
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
