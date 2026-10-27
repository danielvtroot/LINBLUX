-- Som ao tocar em uma peça
-- O som é escolhido da biblioteca do projeto e gravado no atributo estável
-- Sound (referência assets/<arquivo>). O contato toca uma vez e respeita um
-- intervalo mínimo para não empilhar vozes.
local raiz = linblux.scene.root
local som = nil
for _, entrada in ipairs(linblux.audio.list()) do
    if entrada.kind == "sound" then
        som = entrada
        break
    end
end

local botao = linblux.scene.create("part", {
    name = "BotaoSonoro",
    position = linblux.vec3(0, 2.6, -12),
    size = linblux.vec3(4, 1, 4),
    color = linblux.vec3(90, 170, 130),
})
botao.anchored = true
botao:set_attribute("Sound", som and som.reference or "")
raiz:set_attribute("toque_pronto", true)

if not som then
    warn("Importe um som (tipo Som) para ouvir o toque.")
    return
end

local ultimo = -100
local function tocar()
    local agora = linblux.time.now()
    if agora - ultimo < 0.3 then return end
    ultimo = agora
    local handle = linblux.audio.play(botao:get_attribute("Sound"), { volume = 0.8 })
    raiz:set_attribute("toque_total", (raiz:get_attribute("toque_total") or 0) + 1)
    raiz:set_attribute("toque_handle", handle)
end

linblux.events.on("collision_enter", function(contato)
    if contato.first == botao or contato.second == botao then tocar() end
end)
