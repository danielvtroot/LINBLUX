-- Adicione este Script como filho de uma peça. Atua apenas na cópia de execução.
local objeto = linblux.self
local origem = objeto.position
local tempo = 0
objeto.anchored = true
linblux.events.on("update", function(dt)
    tempo = tempo + dt
    objeto.position = linblux.vec3(origem.x + math.sin(tempo) * 6, origem.y, origem.z)
end)
