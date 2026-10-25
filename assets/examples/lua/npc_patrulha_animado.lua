-- NPC de patrulha com rig e animação
-- A patrulha continua sendo a navegação real da engine; o rig R6 do jogador
-- (rig = "player") dá o visual, e o clipe vem do estado do controlador:
-- idle ao esperar, walk na volta e run quando a velocidade passa de 12.
local ROTA = {
    linblux.vec3(-30, 2.6, -30),
    linblux.vec3(  0, 2.6, -30),
    linblux.vec3(  0, 2.6, -45),
    linblux.vec3(-30, 2.6, -45),
}
local ESPERA = 0.8
local VELOCIDADE = 14 -- >= 12 seleciona o clipe run em movimento

local raiz = linblux.scene.root
local usaRig = true
local guarda = linblux.npc.create{
    name = "GuardaAnimado",
    position = ROTA[1],
    size = linblux.vec3(2, 5, 2),
    color = linblux.vec3(70, 130, 220),
    speed = VELOCIDADE,
    turn_speed = 10,
    rig = "player",
}
raiz:set_attribute("patrulha_animada_pronta", true)
raiz:set_attribute("patrulha_rig", linblux.npc.rig(guarda))

local indice = 2
local espera = 0
local pontos = 0
local vistos = { idle = false, walk = false, run = false }

linblux.npc.move_to(guarda, ROTA[indice])

local function proximo_ponto()
    indice = indice % #ROTA + 1
    if not linblux.npc.move_to(guarda, ROTA[indice]) then
        raiz:set_attribute("patrulha_animada_inacessivel", indice)
        indice = indice % #ROTA + 1
        linblux.npc.move_to(guarda, ROTA[indice])
    end
end

linblux.events.on("update", function(dt)
    local clipe = linblux.npc.animation(guarda)
    vistos[clipe] = true
    raiz:set_attribute("patrulha_clipe", clipe)
    if clipe == "walk" then raiz:set_attribute("patrulha_walk", true) end
    if clipe == "run" then raiz:set_attribute("patrulha_run", true) end
    local estado = linblux.npc.state(guarda)
    if estado == "moving" or estado == "dead" then return end
    espera = espera - dt
    if espera > 0 then return end
    espera = ESPERA
    pontos = pontos + 1
    raiz:set_attribute("patrulha_animada_pontos", pontos)
    proximo_ponto()
end)
