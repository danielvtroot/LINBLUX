-- Script global: E ou botão sul do controle alterna as portas do exemplo porta.lua.
-- As ações passam pelo roteamento de gameplay; o menu da engine continua acessível.
local aberta = false
local function alternar()
    aberta = not aberta
    linblux.events.emit("porta.alterar", aberta)
end
linblux.events.on("key_pressed", function(tecla)
    if tecla == "e" then alternar() end
end)
linblux.events.on("gamepad_pressed", function(botao)
    if botao == "south" then alternar() end
end)
