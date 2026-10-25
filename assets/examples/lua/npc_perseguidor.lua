-- Monstro perseguidor
-- Detecta o jogador dentro de um raio, calcula o caminho e persegue. O destino
-- e atualizado quando o alvo se move, e o NPC para quando perde o alvo. O
-- caminho nunca atravessa paredes: a colisao continua sendo do corpo Bullet.
local RAIO = 30
local VELOCIDADE = 13
local INTERVALO = 0.4
local TOLERANCIA = 2

local raiz = linblux.scene.root
local monstro = linblux.npc.create{
    name = "Perseguidor",
    position = linblux.vec3(14, 2.6, 14),
    size = linblux.vec3(2.4, 5, 2.4),
    color = linblux.vec3(190, 60, 60),
    speed = VELOCIDADE,
    max_health = 80,
}

local function distancia(a, b)
    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local buscando = false
local replanejamentos = 0
local proxima_busca = 0

linblux.events.on("update", function(dt)
    if linblux.npc.state(monstro) == "dead" then return end
    proxima_busca = proxima_busca - dt
    if proxima_busca > 0 then return end
    proxima_busca = INTERVALO

    local jogador = linblux.player.position
    if distancia(jogador, monstro.position) > RAIO then
        if buscando then
            buscando = false
            linblux.npc.stop(monstro)
            raiz:set_attribute("alvo_perdido", true)
        end
        return
    end

    if not buscando then
        buscando = true
        raiz:set_attribute("perseguindo", true)
    end

    local destino = linblux.npc.target(monstro)
    if destino == nil or distancia(destino, jogador) > TOLERANCIA then
        replanejamentos = replanejamentos + 1
        raiz:set_attribute("replanejamentos", replanejamentos)
        if not linblux.npc.move_to(monstro, jogador) then
            raiz:set_attribute("destino_inacessivel", true)
        end
    end
end)
