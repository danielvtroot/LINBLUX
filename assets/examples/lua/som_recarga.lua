-- Som de recarga
-- R executa a recarga com pausa de segurança. O som vem do atributo estável
-- SoundReload do Modelo; nenhum caminho absoluto é guardado no script.
local raiz = linblux.scene.root
local som = nil
for _, entrada in ipairs(linblux.audio.list()) do
    if entrada.kind == "sound" then
        som = entrada
        break
    end
end

local arma = linblux.scene.create("model", { name = "ArmaRecarregavel" })
local corpo = linblux.scene.create("part", {
    name = "CorpoDaArma", parent = arma,
    position = linblux.vec3(0, 3, -14), size = linblux.vec3(1, 1, 4),
    color = linblux.vec3(80, 70, 60),
})
corpo.anchored = true
arma:set_attribute("SoundReload", som and som.reference or "")
raiz:set_attribute("recarga_pronta", true)

if not som then
    warn("Importe um som (tipo Som) para ouvir a recarga.")
    return
end

local ultima = -100
local function recarregar()
    local agora = linblux.time.now()
    if agora - ultima < 1.2 then return end
    ultima = agora
    local handle = linblux.audio.play(arma:get_attribute("SoundReload"), { volume = 0.75 })
    raiz:set_attribute("recargas", (raiz:get_attribute("recargas") or 0) + 1)
    raiz:set_attribute("recarga_handle", handle)
    print("Recarga iniciada")
end

linblux.events.on("update", function()
    if linblux.input.key_pressed("r") then recarregar() end
end)
