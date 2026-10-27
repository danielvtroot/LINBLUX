-- Playlist de música por Script
-- Toca as faixas marcadas como Música, uma por vez, em ordem. A playlist
-- declarativa em Studio > Áudio é salva com o projeto e tem precedência; este
-- exemplo é para quem precisa montar a ordem em tempo de execução.
local raiz = linblux.scene.root
local faixas = {}
for _, som in ipairs(linblux.audio.list()) do
    if som.kind == "music" then
        faixas[#faixas + 1] = som
    end
end

if #faixas == 0 then
    warn("Importe e marque ao menos uma música para usar a playlist.")
    return
end

local atual = 0
local handle = nil
local pausada = false
local repetir = true
local volume = 0.3

local function tocar(indice)
    atual = indice
    local faixa = faixas[atual]
    handle = linblux.audio.play(faixa.id, { volume = volume, loop = false })
    raiz:set_attribute("playlist_faixa", faixa.name)
    raiz:set_attribute("playlist_indice", atual)
    print("Playlist:", atual, faixa.name)
end

local function proxima()
    if atual < #faixas then
        tocar(atual + 1)
    elseif repetir then
        tocar(1)
    else
        handle = nil
        raiz:set_attribute("playlist_ativa", false)
    end
end

tocar(1)
raiz:set_attribute("playlist_ativa", true)
raiz:set_attribute("playlist_total", #faixas)

linblux.events.on("update", function()
    if handle and not linblux.audio.is_playing(handle) and not linblux.audio.is_paused(handle) then
        proxima()
    end
    -- 0 pausa/retoma; 9 avança; 8 alterna a repetição da lista.
    if linblux.input.key_pressed("0") then
        if pausada then
            linblux.audio.resume(handle)
        else
            linblux.audio.pause(handle)
        end
        pausada = not pausada
    elseif linblux.input.key_pressed("9") then
        linblux.audio.stop(handle)
        proxima()
    elseif linblux.input.key_pressed("8") then
        repetir = not repetir
        raiz:set_attribute("playlist_repetir", repetir)
    end
end)

linblux.events.on("stop", function()
    if handle then linblux.audio.stop(handle) end
    handle = nil
    raiz:set_attribute("playlist_ativa", false)
end)
