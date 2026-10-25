-- Som ao interagir com uma peça
-- Toca um som 2D quando o jogador entra em contato com a peça e outro quando
-- sai do volume de interação. O som vem da biblioteca do projeto (kind
-- "sound"); o script não depende de caminho absoluto nem de arquivo externo.
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
botao.Anchored = true
raiz:set_attribute("interacao_pronta", true)

local ultimo = 0
local function tocar(motivo)
    if not som then
        warn("Importe um som (kind sound) para ouvir a interação.")
        return
    end
    -- Intervalo mínimo para não empilhar dezenas de vozes no contato contínuo.
    local agora = linblux.time.now() + ultimo
    if agora - ultimo < 0.25 then return end
    ultimo = agora
    local handle = linblux.audio.play(som.id, { volume = 0.8 })
    if handle then
        raiz:set_attribute("interacao_som", motivo)
        raiz:set_attribute("interacao_total", (raiz:get_attribute("interacao_total") or 0) + 1)
    end
end

linblux.events.on("collision_enter", function(outra)
    if outra == botao then tocar("entrou") end
end)
linblux.events.on("collision_exit", function(outra)
    if outra == botao then tocar("saiu") end
end)
