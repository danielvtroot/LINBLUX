# Attachments

Attachment é um componente real da hierarquia, com ID estável e transformação
local validada. Pode ser filho de Part ou Modelo. Crie pelo botão **Criar
Attachment** nas Propriedades ou pelo menu de inserção; selecione no Explorador
para editar posição/rotação locais e consultar a posição global.

A direção frontal é -Z. A pose global compõe a posição, orientação e escala do
pai. Parts armazenam posição/rotação mundiais; para seus Attachments, a escala
vem do Modelo ancestral mais próximo. Alterar Part.size altera a geometria,
sem redefinir a unidade local do Attachment. Escalar o Modelo escala os offsets.
Reparentar conserva a pose global das âncoras, ajustando os offsets locais.
Desagrupar um Modelo que tenha Attachments diretamente nele é recusado quando
o destino não aceita Attachments; remova ou reparentie a âncora antes.

```lua
local v = linblux.vec3
local boca = linblux.scene.create("attachment", {
    name="BocaDoCano", parent=cano, position=v(0,0,-2), rotation=v(0,0,0)
})
boca.position = v(0,0,-3) -- local, em unidades da cena
boca.rotation = v(0,20,0) -- graus, Euler ZYX
print(boca.world_position.x, boca.direction.z)
```

`position`, `rotation`, `transform` são locais e editáveis. `world_position`,
`world_rotation`, `world_transform` e `direction` são calculados e somente
leitura. Transformação local aceita apenas position/rotation; escala local é1.
`name`, `id`, `parent`, clonagem, destruição e enumeração/remoção de componentes
usam os mecanismos existentes. Referência destruída não vira ponteiro livre.
Exemplo executável completo: assets/examples/lua/attachments.lua.

Projetos com Attachment usam JSON v5: attachment.position [x,y,z] e
attachment.rotation quaternion [x,y,z,w]. Parts usam orientation em quaternion.
IDs, vínculos e quadros sobrevivem a salvar, copiar, exportar e importar pelo
codec de projetos existente. v1–v4 permanecem legíveis; leitores antigos recusam
v5. Carregamento inválido não substitui a cena atual.

Attachments não são corpos rígidos nem juntas físicas. Vínculos/juntas e
inventário continuam pendentes até implementação e validação próprias.
