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
Lua (use apenas uma forma para cada tipo). Não há transferência multiplayer. Coleta por E, Delete e hotbar são descritos abaixo.

Clonar um protótipo na autoria deriva um novo identificador de tipo, com o ID
da cópia como sufixo. Instâncias criadas por items.spawn têm o marcador de
protótipo desligado, para não serem registradas como tipos em outra carga.

Escopo Alpha desta unidade: estrutura de componentes capturada ao instanciar.
Modele a hierarquia antes de spawn; adicionar/remover Parts de uma instância
guardada/equipada não é suportado nesta etapa. Cor/material, transformações e
atributos de comportamento permanecem programáveis. Estado automático entre
partidas e transferência multiplayer também ficam fora deste incremento.

## Ações programáveis de itens (Alpha)

`linblux.items.configure(item,{magazine=12,reserve=24,consume=1,interval=.16,
reload_time=1.6})` configura um estado de partida independente por ID.
`activate(item)` e `reload(item)` retornam `sucesso,motivo`; exigem o item
atualmente equipado. `deactivate`, `cancel_reload`, `set_ammo(item,pente,reserva)`
e `combat(item)` completam a API. `combat` retorna ammo/reserve/magazine/consume,
interval/reload_time/reload_progress/cooldown/reloading/active, ou nil.

Pente0..100000, reserva0..1000000, consumo1..100000, intervalo/recarga0..60 s.
Inteiros Lua são verificados antes da conversão C++; configurações inválidas
não alteram o estado. `magazine=0` representa ferramenta sem munição.
O evento `item_action` entrega item, action, ammo, reserve, magazine, reloading e
active após cada transição. Ações: activate,deactivate,ammo,
reload_start,reload_end,reload_cancel. Troca/soltura cancelam recarga;
destruição remove estado, callbacks e UI. Stop limpa tudo. O autor define os
efeitos em Lua — a engine não contém uma arma fixa.

RifleDemo demonstra Attachment, raycast ignorando a subárvore do Modelo,
munição, recarga, recoil, som por ID estável, UI, E/Delete/recoleta. Consulte
[as instruções](../assets/examples/rifledemo/README.md).

## Coleta, interação e hotbar

Itens world recebem coleta por proximidade: segure E por0,6 s, ou botão oeste
(X) do controle. Soltar a tecla, mudar de alvo, afastar-se ou abrir o menu
cancela o progresso. O teste de alcance usa a geometria, não o pivô do Modelo;
raycast impede interação através de obstáculos. Uma conclusão exige liberar
a tecla antes de outra ativação. Não há prompt sem alvo válido.

A interação também serve a portas, baús e dispositivos por Lua:

```lua
linblux.interactions.register(porta, {
    label="Abrir porta", distance=6, hold_seconds=0.6, key="e"
})
linblux.events.on("interaction_completed", function(evento)
    if evento.object and evento.object.id==porta.id then
        porta.rotation = linblux.vec3(0,90,0)
    end
end)
```

register substitui a configuração do mesmo objeto; unregister remove (e
suprime registro automático de item até novo register). prompt() expõe objeto,
nome e progresso reais para a apresentação personalizada. Objetos precisam ser
Parts ou Modelos com geometria. Limites:256 entradas, distância0,25–20,
duração0,05–10 s; teclas da lista pública, sem Esc/F1–F12 reservadas ao host.
Máximo de quatro candidatos com teste de linha de visão por atualização.

A hotbar é compartilhada entre Studio/Play e Player independente. Fica oculta
sem itens equipáveis; usa nome ou ícone da biblioteca, destaque vermelho e
somente os slots ocupados. Teclas1–9 selecionam a página atual; pressionar a
mesma tecla desequipa. PgUp/PgDn alternam páginas; bumpers do controle percorrem
itens. Delete solta o equipado com física e preserva identidade/dados/scripts.
Ausência de espaço mantém o item e mostra erro real. Recolher usa E novamente.
Objetos sem equipable continuam na bolsa, mas não criam slot.

O menu captura a entrada antes do runtime: controles de jogo não executam
com o menu aberto ou a sessão pausada. A apresentação de progresso/hotbar usa retângulos, texto e
texturas já gerenciadas, sem framebuffer extra nem alocação de vídeo.
Exemplo público: assets/examples/lua/interacao_porta.lua.

Testes nativos cobrem cancelamento, obstáculo, conclusão única, coleta,
seleção, Delete/recoleta e dados preservados. O teste gráfico separado
linblux_item_controls_ui usa teclado X11, capturas720p/1080p e paginação. A
simulação de bits de controle não comprova teste em hardware de gamepad.

## Organizar protótipos em ServerStorage

No menu **+** do Explorador, escolha **Protótipos de itens**. O Studio localiza
ou cria `ServerStorage/Itens` sem duplicar a pasta e sem alterar pastas homônimas
em outros pais. `Itens` é uma Folder comum, não um serviço ou classe nova.

1. Crie um Modelo com suas Parts e scripts, ou mova um Modelo existente para a pasta.
2. Marque **Protótipo de item**, defina o tipo e escolha um ícone opcional nas Propriedades.
3. Salve. O conteúdo guardado não aparece no Mundo nem possui colisão ativa.
4. Em um Script de ServerScriptService, use `linblux.items.spawn("espada")`,
   substituindo pelo tipo configurado.
5. A instância aparece no Mundo com IDs novos e scripts do clone restaurados.
   O protótipo original continua guardado; coleta, equipar, guardar e soltar
   usam o inventário existente.

O catálogo é registrado antes dos scripts de Play. Scripts do protótipo não
executam enquanto guardados. Hierarquia, tipo, ícone e recursos acompanham
salvar/reabrir e exportação/importação `.lo`. Stop preserva a autoria.
Não há StarterItems automático nesta Alpha; concessão inicial pode ser programada
pelo criador com a API existente, sem prometer uma interface de StarterPack.

O histórico atual cobre transformações. Migração em lote de nomes e undo geral
de nomes/hierarquia não foram acrescentados; mantenha cópia da experiência antes
de reorganizações grandes ou revisão de scripts que buscam objetos por nome.
