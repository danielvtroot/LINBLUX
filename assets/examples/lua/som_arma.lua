-- Som de arma
-- Um Modelo de arma guarda o disparo no atributo SoundFire. Clique esquerdo ou
-- F dispara; o intervalo mínimo evita uma voz por quadro.
local raiz = linblux.scene.root
local som = nil
for _, entrada in ipairs(linblux.audio.list()) do
    if entrada.kind == "sound" then
        som = entrada
        break
    end
end

local arma = linblux.scene.create("model", { name = "ArmaExemplo" })
local corpo = linblux.scene.create("part", {
    name = "CorpoDaArma", parent = arma,
    position = linblux.vec3(0, 3, -14), size = linblux.vec3(1, 1, 4),
    color = linblux.vec3(70, 75, 85),
})
corpo.anchored = true
arma:set_attribute("SoundFire", som and som.reference or "")
raiz:set_attribute("arma_pronta", true)

if not som then
    warn("Importe um som (tipo Som) para ouvir o disparo.")
    return
end

local ultimo = -100
local function disparar()
    local agora = linblux.time.now()
    if agora - ultimo < 0.18 then return end
    ultimo = agora
    local handle = linblux.audio.play(arma:get_attribute("SoundFire"), { volume = 0.85 })
    raiz:set_attribute("disparos", (raiz:get_attribute("disparos") or 0) + 1)
    raiz:set_attribute("disparo_handle", handle)
end

linblux.events.on("update", function()
    if linblux.input.mouse_pressed("left") or linblux.input.key_pressed("f") then
        disparar()
    end
end)

linblux.events.on("stop", function()
    local handle = raiz:get_attribute("disparo_handle")
    if type(handle) == "number" then linblux.audio.stop(handle) end
end)
