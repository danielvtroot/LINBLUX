-- Monstro que persegue e ataca
-- Usa a navegacao para perseguir o jogador, ataca por proximidade com intervalo
-- entre golpes e aplica dano pela API real de vida do jogador. O NPC tambem
-- pode receber dano: qualquer script pode emitir "atacar_monstro".
local RAIO = 30
local ALCANCE = 4
local DANO = 12
local INTERVALO = 1.5

local raiz = linblux.scene.root
local monstro = linblux.npc.create{
    name = "Monstro",
    position = linblux.vec3(-14, 2.6, 14),
    size = linblux.vec3(2.4, 5, 2.4),
    color = linblux.vec3(150, 40, 170),
    speed = 12,
    max_health = 60,
    respawn_delay = 0,
}

local function distancia(a, b)
    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local relogio = 0
local ultimo_ataque = -INTERVALO
local ataques = 0

linblux.events.on("update", function(dt)
    relogio = relogio + dt
    if linblux.npc.state(monstro) == "dead" then return end
    local jogador = linblux.player
    local d = distancia(jogador.position, monstro.position)
    if d <= ALCANCE then
        if linblux.npc.state(monstro) == "moving" then linblux.npc.stop(monstro) end
        if relogio - ultimo_ataque >= INTERVALO then
            ultimo_ataque = relogio
            jogador:damage(DANO) -- vida real do jogador, com morte e respawn
            ataques = ataques + 1
            raiz:set_attribute("ataques", ataques)
        end
    elseif d <= RAIO then
        local destino = linblux.npc.target(monstro)
        if destino == nil or distancia(destino, jogador.position) > 2 then
            linblux.npc.move_to(monstro, jogador.position)
        end
    else
        if linblux.npc.state(monstro) == "moving" then linblux.npc.stop(monstro) end
    end
end)

-- Dano recebido: outro script emite linblux.events.emit("atacar_monstro", {dano = 25})
linblux.events.on("atacar_monstro", function(payload)
    local dano = 10
    if payload and payload.dano then dano = payload.dano end
    linblux.npc.damage(monstro, dano)
    if linblux.npc.state(monstro) == "dead" then
        raiz:set_attribute("monstro_morto", true)
    end
end)
