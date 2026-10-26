# Inventário da sessão

No Studio, selecione um Modelo e marque **Protótipo de item** nas Propriedades.
Defina o tipo (identificador estável) e escolha o ícone pelo seletor de imagens.
Esse catálogo é carregado antes dos scripts, evitando executar comportamento de
protótipos. O nome amigável é o nome do Modelo. O marcador, tipo e ícone são
atributos persistidos e acompanham salvar, Mundo Padrão e pacotes .lo.

A API `linblux.items` usa Modelos e IDs reais. Registrar um tipo transforma o
Modelo em protótipo da sessão: suas peças ficam ocultas/sem contato e seus
scripts ficam desativados. Instâncias clonadas retomam os scripts originais,
com novos IDs; não compartilham peças com o protótipo.

```lua
linblux.items.register("chave", modelo, {
    name="Chave vermelha", icon="assets/minha-chave.png", equipable=true,
    grip_position=linblux.vec3(1,0,-2), grip_rotation=linblux.vec3(0,0,0)
})
local item = linblux.items.spawn("chave", {position=linblux.vec3(4,3,0)})
linblux.items.collect(item) -- concessão por script; interação E é outra camada
linblux.items.equip(item)
linblux.items.unequip()
linblux.items.drop(item) -- procura chão e espaço livre próximo ao jogador
```

O ícone é opcional; quando definido deve ser imagem gerenciada em assets/.
As opções de register são name, icon, equipable, grip_position/grip_rotation.
spawn aceita position, rotation (Euler ZYX, graus) e scale uniforme opcionais.
`collect` e seu alias `add` aceitam apenas instância world, sem duplicar o item.
`list()` retorna os Modelos guardados/equipados em ordem de coleta.
`equipped()` retorna Modelo ou nil. `info(item)` retorna type/name/icon/state/
equipable. `state(item)` retorna world, stored ou equipped. `remove(item)`
destrói explicitamente a instância da partida, sem excluir arquivos de assets.

Equipar mantém a escala do item, torna as peças sem contato e acompanha a
posição/direção do jogador com o grip configurado. Guardar oculta também
Surfaces e decals legados; consultas e colisões ignoram essas representações.
Soltar preserva ID, scripts, atributos e vínculos; devolve cor/contato e ativa
física. Se nenhuma das posições testadas estiver livre, falha sem retirar o
item do inventário. Equipar outro item guarda o anterior, sem destruir nada.

```lua
linblux.events.on("inventory_changed", function(evento)
    print(evento.action, evento.id) -- ID textual estável, ação e handle item
end)
```

Ações: spawn, collect, equip, unequip, drop e remove. item pode ser nil em remove.
Eventos pertencem à engine; scripts não podem forjá-los por events.emit.
Listeners seguem o ciclo de vida e limites já usados pelos scripts.

Limites: 64 tipos, 128 instâncias, 64 itens guardados, 256 componentes/128 Parts
por protótipo; cota Lua nativa continua aplicável. Protótipo não pode conter
controlador NPC nem juntas externas; tipos não podem se sobrepor. Mudança da
estrutura do protótipo exige registrar outro tipo; referência removida gera
erro seguro. A posição de descarte é conservadora e não serve a itens gigantes.

Definições/modelos/scripts/recursos autorais usam a persistência normal da
experiência. Catálogo, equipamento e bolsa são estado temporário de Play;
nenhuma dessas mutações altera a cena autoral. Estado entre partidas deve ser
salvo explicitamente pelo criador com storage, usando tipo e dados próprios,
não supondo que um ID de instância gerado sobreviva a outra sessão.

Exemplo executável: assets/examples/lua/inventario.lua. Tipos podem ser configurados nas Propriedades ou criados dinamicamente por
Lua (use apenas uma forma para cada tipo). Não há transferência multiplayer. Segurar E, tecla Delete e hotbar ainda
precisam da integração de entrada/apresentação; não são anunciados como prontos.

Clonar um protótipo na autoria deriva um novo identificador de tipo, com o ID
da cópia como sufixo. Instâncias criadas por items.spawn têm o marcador de
protótipo desligado, para não serem registradas como tipos em outra carga.
