-- Música de fundo por Script
-- Prefira configurar Studio > Áudio > Música da experiência para uma faixa
-- declarativa que inicia no Play e é salva com o projeto. Este exemplo mostra
-- o mesmo resultado pela API pública, útil quando a música depende de lógica.
local raiz = linblux.scene.root
local faixa = nil
for _, som in ipairs(linblux.audio.list()) do
    if som.kind == "music" then
        faixa = som
        break
    end
end

if not faixa then
    raiz:set_attribute("musica_ativa", false)
    warn("Importe um MP3/WAV/OGG e marque-o como Música em Studio > Áudio.")
    return
end

local handle = linblux.audio.play(faixa.id, { volume = 0.35, loop = true })
raiz:set_attribute("musica_ativa", true)
raiz:set_attribute("musica_id", faixa.id)
raiz:set_attribute("musica_handle", handle)
print("Música de fundo:", faixa.name, faixa.duration)

-- 0 pausa, 9 retoma, 1 abaixa e 2 aumenta. O estado é de sessão.
linblux.events.on("update", function()
    if linblux.input.key_pressed("0") then
        linblux.audio.pause(handle)
        raiz:set_attribute("musica_ativa", false)
    elseif linblux.input.key_pressed("9") then
        linblux.audio.resume(handle)
        raiz:set_attribute("musica_ativa", true)
    elseif linblux.input.key_pressed("1") then
        linblux.audio.set_volume(handle, 0.15)
    elseif linblux.input.key_pressed("2") then
        linblux.audio.set_volume(handle, 0.6)
    end
    if not linblux.audio.is_playing(handle) and not linblux.audio.is_paused(handle) then
        raiz:set_attribute("musica_ativa", false)
    end
end)

-- Stop da experiência: encerra o stream para não deixar som órfão no Player.
linblux.events.on("stop", function()
    linblux.audio.stop(handle)
    raiz:set_attribute("musica_ativa", false)
end)
