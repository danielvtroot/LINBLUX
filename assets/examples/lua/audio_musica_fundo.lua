-- Música de fundo
-- Reproduz uma faixa em loop a partir da biblioteca de sons do projeto.
-- Importe um MP3/WAV/OGG em Studio > Imagens/Sons e a entrada aparece aqui com
-- kind = "music". Sem faixa importada o script apenas registra um aviso: nada
-- de erro fatal em uma cena sem áudio.
local faixa = nil
for _, som in ipairs(linblux.audio.list()) do
    if som.kind == "music" then
        faixa = som
        break
    end
end

local raiz = linblux.scene.root
if not faixa then
    raiz:set_attribute("musica_ativa", false)
    warn("Importe um áudio e marque-o como música para ouvir o fundo.")
    return
end

local handle = linblux.audio.play(faixa.id, { volume = 0.35, loop = true })
raiz:set_attribute("musica_ativa", true)
raiz:set_attribute("musica_handle", handle)

-- Ajuste de volume em tempo real: 1 e 2 reduzem/aumentam a faixa. O estado é
-- de sessão; a cena salva não guarda o handle.
linblux.events.on("update", function()
    if linblux.input.key_pressed("1") then
        linblux.audio.set_volume(handle, 0.15)
    elseif linblux.input.key_pressed("2") then
        linblux.audio.set_volume(handle, 0.6)
    elseif linblux.input.key_pressed("0") then
        linblux.audio.pause(handle)
    elseif linblux.input.key_pressed("9") then
        linblux.audio.resume(handle)
    end
end)

-- Stop da experiência: encerrar o stream evita som órfão no Player.
linblux.events.on("stop", function()
    linblux.audio.stop(handle)
    raiz:set_attribute("musica_ativa", false)
end)
