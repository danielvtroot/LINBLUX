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
o destino não aceita Attachments; remova ou mova a âncora para outro pai antes.

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
leitura. Transformação local aceita apenas position/rotation; escala local é 1.
`name`, `id`, `parent`, clonagem, destruição e enumeração/remoção de componentes
usam os mecanismos existentes. Referência destruída não vira ponteiro livre.
Exemplo executável completo: assets/examples/lua/attachments.lua.

Projetos com Attachment usam JSON v5: attachment.position [x,y,z] e
attachment.rotation quaternion [x,y,z,w]. Parts usam orientation em quaternion.
IDs, vínculos e quadros sobrevivem a salvar, copiar, exportar e importar pelo
codec de projetos existente. v1–v4 permanecem legíveis; leitores antigos recusam
v5. Carregamento inválido não substitui a cena atual.

Attachments não são corpos rígidos. Um par de Attachments de Parts distintas
pode definir uma **junta fixa**, implementada pelo Bullet, sem criar corpos
adicionais. Nas Propriedades, **Conectar a** escolhe a âncora pelo nome da peça;
**Sem junta** desconecta. A conexão ajusta a âncora de destino para preservar
a pose atual das peças. Não move a geometria ao conectar.

```lua
origem:connect_to(destino) -- ambas pertencem a Parts distintas
origem.rotation = linblux.vec3(0,30,0) -- anima o quadro da junta
print(origem.joint.id) -- Attachment de destino
origem:disconnect() -- também pode ser chamado no destino
-- equivalente: origem.joint = destino; origem.joint = nil
```

Uma âncora participa de um único par; use outras âncoras para encadear peças.
O solver mantém os quadros coincidentes contra gravidade/forças, desabilita
colisão entre as duas peças conectadas e respeita a edição pelo Studio. Para
animar fisicamente, o destino precisa ser dinâmico; duas peças ancoradas não
são movidas pelo solver. Ajustar posição/rotação local anima a conexão usando
os mesmos quaternions/transformações da engine; não cria outro sistema de rigs.
Cada junta fixa usa 64 iterações locais do solver; o orçamento global das
peças sem junta permanece inalterado. Benchmark de grandes montagens segue
pendente. Não há dobradiça de eixo livre ou motor com limites nesta integração.
Exemplo executável: assets/examples/lua/juntas.lua.

A junta vive no Attachment de origem: JSONv6, `jointTarget` com o ID do destino.
Leitura v1–v5 é preservada; projetos só com âncoras continuam v5. Carregamento
valida a referência inteira antes de substituir o documento. Clonar uma
subárvore remapeia vínculos internos; vínculos a âncoras externas são desligados
na cópia. Excluir qualquer ponta desfaz o vínculo e remove a constraint antes
de liberar corpos Bullet. Inventário permanece um sistema separado.
