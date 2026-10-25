-- NPC de patrulha
-- Cria uma entidade comum de cena com o componente de NPC e percorre uma rota
-- em ciclo. O caminho vem da navegacao da engine: paredes estaticas e limites
-- de piso sao contornados, e o corpo Bullet continua sendo quem colide.
local ROTA = {
    linblux.vec3(-30, 2.6, -30),
    linblux.vec3(  0, 2.6, -30),
    linblux.vec3(  0, 2.6, -45),
    linblux.vec3(-30, 2.6, -45),
}
local ESPERA = 0.8
local VELOCIDADE = 10

local raiz = linblux.scene.root
local guarda = linblux.npc.create{
    name = "GuardaPatrulha",
    position = ROTA[1],
    size = linblux.vec3(2, 5, 2),
    color = linblux.vec3(70, 130, 220),
    speed = VELOCIDADE,
    turn_speed = 10,
}
assert(linblux.npc.state(guarda) == "idle")

local indice = 2
local espera = 0
local pontos = 0

raiz:set_attribute("patrulha_pronta", true)
linblux.npc.move_to(guarda, ROTA[indice])

local function proximo_ponto()
    indice = indice % #ROTA + 1
    if not linblux.npc.move_to(guarda, ROTA[indice]) then
        -- Destino inacessivel: registra e tenta o ponto seguinte da rota.
        raiz:set_attribute("patrulha_inacessivel", indice)
        indice = indice % #ROTA + 1
        linblux.npc.move_to(guarda, ROTA[indice])
    end
end

linblux.events.on("update", function(dt)
    local estado = linblux.npc.state(guarda)
    if estado == "moving" or estado == "dead" then return end
    -- Chegou (ou nao ha rota): espera antes de seguir para o proximo ponto.
    espera = espera - dt
    if espera > 0 then return end
    espera = ESPERA
    pontos = pontos + 1
    raiz:set_attribute("patrulha_pontos", pontos)
    proximo_ponto()
end)
