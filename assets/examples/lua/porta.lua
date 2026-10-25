-- Script filho da porta. Outro script emite "porta.alterar" com true ou false.
local porta = linblux.self
local fechada = porta.position
porta.anchored = true
linblux.events.on("porta.alterar", function(aberta)
    porta.position = linblux.vec3(fechada.x, fechada.y + (aberta and 7 or 0), fechada.z)
    porta:set_attribute("aberta", aberta == true)
end, porta)
