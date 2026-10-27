-- Som de interação
-- Aproxime-se do posto e pressione E. A peça guarda a referência no atributo
-- estável SoundInteract; a distância é medida pela posição real do jogador.
local raiz = linblux.scene.root
local som = nil
for _, entrada in ipairs(linblux.audio.list()) do
    if entrada.kind == "sound" then
        som = entrada
        break
    end
end

local posto = linblux.scene.create("part", {
    name = "PostoDeInteracao",
    position = linblux.vec3(6, 2.6, -10),
    size = linblux.vec3(3, 3, 3),
    color = linblux.vec3(70, 120, 210),
})
posto.anchored = true
posto:set_attribute("SoundInteract", som and som.reference or "")
raiz:set_attribute("interacao_pronta", true)

if not som then
    warn("Importe um som (tipo Som) para ouvir a interação.")
    return
end

local ultimo = -100
linblux.events.on("update", function()
    if not linblux.input.key_pressed("e") then return end
    local jogador = linblux.player.position
    local alvo = posto.position
    local dx, dy, dz = jogador.x - alvo.x, jogador.y - alvo.y, jogador.z - alvo.z
    local distancia = math.sqrt(dx * dx + dy * dy + dz * dz)
    if distancia > 7 then
        raiz:set_attribute("interacao_distante", true)
        return
    end
    local agora = linblux.time.now()
    if agora - ultimo < 0.4 then return end
    ultimo = agora
    local handle = linblux.audio.play(posto:get_attribute("SoundInteract"), { volume = 0.9 })
    raiz:set_attribute("interacao_som", true)
    raiz:set_attribute("interacao_handle", handle)
    print("Interação executada a", string.format("%.1f", distancia), "unidades")
end)
