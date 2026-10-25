-- Monstro com rig: persegue, ataca com animação e morre com clipe death
-- O ataque é apresentado por linblux.npc.attack e o dano é aplicado na vida
-- real do jogador; o monstro também recebe dano pelo evento "atacar_monstro",
-- entra no clipe hurt e, ao zerar a vida, cai no clipe death. O respawn é
-- configurado no componente (respawn_delay) e restaura vida/posição.
local RAIO = 34
local ALCANCE = 4.5
local INTERVALO = 1.6
local VELOCIDADE = 16
local TOLERANCIA = 2

local raiz = linblux.scene.root
local monstro = linblux.npc.create{
    name = "MonstroAnimado",
    position = linblux.vec3(-18, 2.6, 6),
    size = linblux.vec3(3, 5.5, 3),
    color = linblux.vec3(120, 70, 150),
    speed = VELOCIDADE,
    turn_speed = 12,
    max_health = 60,
    respawn_delay = 3,
    rig = "player",
}
raiz:set_attribute("monstro_animado_pronto", true)
raiz:set_attribute("monstro_rig", linblux.npc.rig(monstro))

local function distancia(a, b)
    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local relogio = 0
local ultimoAtaque = -INTERVALO
local vidaAnterior = linblux.npc.health(monstro)

linblux.events.on("update", function(dt)
    relogio = relogio + dt
    local clipe = linblux.npc.animation(monstro)
    raiz:set_attribute("monstro_clipe", clipe)
    if clipe == "hurt" then raiz:set_attribute("monstro_ferido", true) end
    if clipe == "death" then raiz:set_attribute("monstro_morte_animada", true) end

    local vida = linblux.npc.health(monstro)
    if vida > vidaAnterior then
        raiz:set_attribute("monstro_respawn", (raiz:get_attribute("monstro_respawn") or 0) + 1)
    end
    vidaAnterior = vida
    if linblux.npc.state(monstro) == "dead" then return end

    local jogador = linblux.player.position
    local d = distancia(jogador, monstro.position)
    if d > RAIO then
        if linblux.npc.state(monstro) == "moving" then linblux.npc.stop(monstro) end
        return
    end
    if d > ALCANCE then
        local destino = linblux.npc.target(monstro)
        if destino == nil or distancia(destino, jogador) > TOLERANCIA then
            linblux.npc.move_to(monstro, jogador)
        end
        return
    end
    if linblux.npc.state(monstro) == "moving" then linblux.npc.stop(monstro) end
    if relogio - ultimoAtaque < INTERVALO then return end
    ultimoAtaque = relogio
    linblux.npc.attack(monstro)
    linblux.player:damage(12) -- vida real do jogador, com morte e respawn
    raiz:set_attribute("monstro_ataques", (raiz:get_attribute("monstro_ataques") or 0) + 1)
end)

-- Dano recebido: o clipe hurt é automático; a morte também.
linblux.events.on("atacar_monstro", function(payload)
    local dano = 10
    if payload and payload.dano then dano = payload.dano end
    linblux.npc.damage(monstro, dano)
    if linblux.npc.state(monstro) == "dead" then
        raiz:set_attribute("monstro_morto", true)
    end
end)
