-- Monstro perseguidor com rig e animação
-- Detecta o jogador no raio, calcula caminho pela navegação existente e
-- persegue com o clipe run. Aproximando-se, dispara o clipe attack; ao perder
-- o alvo, para e volta ao idle. Nada aqui atravessa paredes.
local RAIO = 30
local ALCANCE_ATAQUE = 5
local VELOCIDADE = 16
local TOLERANCIA = 2

local raiz = linblux.scene.root
local cacador = linblux.npc.create{
    name = "PerseguidorAnimado",
    position = linblux.vec3(-20, 2.6, -20),
    size = linblux.vec3(2.4, 5, 2.4),
    color = linblux.vec3(190, 80, 90),
    speed = VELOCIDADE,
    turn_speed = 12,
    rig = "player",
}
raiz:set_attribute("perseguidor_animado_pronto", true)
raiz:set_attribute("perseguidor_rig", linblux.npc.rig(cacador))

local function distancia(a, b)
    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local perseguindo = false
local relogio = 0
local ultimoAtaque = -10

linblux.events.on("update", function(dt)
    relogio = relogio + dt
    if linblux.npc.state(cacador) == "dead" then return end
    local jogador = linblux.player.position
    local d = distancia(jogador, cacador.position)

    if d <= RAIO then
        if not perseguindo then
            perseguindo = true
            raiz:set_attribute("perseguidor_perseguindo", true)
        end
        if d > ALCANCE_ATAQUE then
            local destino = linblux.npc.target(cacador)
            if destino == nil or distancia(destino, jogador) > TOLERANCIA then
                linblux.npc.move_to(cacador, jogador)
            end
        else
            linblux.npc.stop(cacador)
            if relogio - ultimoAtaque > 1.2 then
                ultimoAtaque = relogio
                linblux.npc.attack(cacador)
                raiz:set_attribute("perseguidor_ataques",
                    (raiz:get_attribute("perseguidor_ataques") or 0) + 1)
            end
        end
    elseif perseguindo then
        perseguindo = false
        linblux.npc.stop(cacador)
        raiz:set_attribute("perseguidor_perdeu_alvo", true)
    end

    raiz:set_attribute("perseguidor_clipe", linblux.npc.animation(cacador))
    raiz:set_attribute("perseguidor_estado", linblux.npc.state(cacador))
end)
