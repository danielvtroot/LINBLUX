# Lua do LINBLUX — API v1, implementação progressiva

A API nova é `linblux`, com `api_version == 1`. Os nomes antigos continuam
funcionando em projetos existentes. Novos exemplos usam os namespaces próprios.
Não se declara uma API completa de física, NPCs, áudio ou UI nesta fase.

## Inserir e executar

Selecione uma peça no Explorador e adicione um Script como filho dela. Edite o
código, salve e execute a experiência. Scripts globais ficam no serviço de scripts
do mundo. Script e LocalScript habilitados sob o mundo ou esse serviço são
inicializados ao entrar em execução. Novos clones habilitados são descobertos
na atualização seguinte; reabilitar um Script reinicia seu contexto, inclusive
quando desativado e reativado no mesmo quadro. Fora da
árvore executável, seus callbacks/tarefas são cancelados. ModuleScript só executa por `require` com
handle de módulo do projeto. Não existe require por caminho ou biblioteca nativa.

`linblux.self` é o objeto pai do script; `linblux.script` é o próprio script.
Cada script tem seu contexto. No script global, self é o serviço, não uma peça.
Os scripts rodam na cópia de runtime: executar/parar não altera a cena autoral.
O editor existente oferece cores de sintaxe, linhas, busca, saída e diagnóstico
com arquivo/linha/traceback. Autocompletar e depurador interativo não foram
adicionados nesta fase.

## Cena e objetos

```lua
local bloco = linblux.scene.create("part", {
    name = "Bloco", position = linblux.vec3(10, 5, 0),
    size = linblux.vec3(4, 2, 4), material = "metal"
})
bloco.position = linblux.vec3(12, 5, 0)
bloco.rotation = linblux.vec3(0, 45, 0)
bloco:set_attribute("pontos", 10)
```

- `scene.root`: raiz do mundo; `scene.find(nome_ou_id)`: primeiro objeto com esse
  nome na ordem da cena, ou ID exato. Retorna nil se não existir.
- `scene.create(tipo, opções?)`: tipos `part`, `group`, `model`. Grupos/modelos
  possuem nome, pai e filhos; não ganham transformação hierárquica nesta fase.
- Opções de peça: `name`, `parent`, `position`, `rotation`, `size`, `color`,
  `material`, `shape`, `anchored`, `collidable`, `transparency`. Opções inválidas
  são recusadas antes da criação. Pai inválido desfaz o objeto criado.
- Padrão de peças novas: caixa, 4×4×4, posição zero, ancorada e colidível.
  `shape`: `box` ou `sphere`. A esfera usa o menor eixo como diâmetro físico.
- Propriedades: `name`, `parent`, `position`, `rotation` em graus, `size` em
  unidades de cena, `color` com canais x/y/z de 0 a 255, `transparency` de 0 a 1,
  `material`, `anchored` e `collidable`. `id`, `type` e `alive` são de leitura.
  Para ocultar uma peça, use transparência 1. Tamanho mínimo: 0,2 por eixo.
- Materiais: `smooth`, `plastic`, `wood`, `metal`, `concrete`, `emissive`.
  São os materiais efetivos do renderizador atual, não uma promessa de PBR.
- Métodos: `children()`, `descendants()`, `find_child(nome)`, `clone()`,
  `destroy()`, `get_attribute(nome)`, `set_attribute(nome, valor)`.
  Valor de atributo: booleano, número finito ou texto; nil remove o atributo.
- Scripts/LocalScripts também expõem `enabled`, permitindo suspender e reiniciar
  seus componentes durante a execução. ModuleScript continua sendo biblioteca.
- Referências usam IDs e geração da sessão. Referências destruídas recusam
  acesso; `alive` permite consultá-las sem desreferenciar memória inválida.

Os handles novos usam propriedades em minúsculas e vetores x/y/z. Handles dos
bindings legados mantêm suas propriedades e vetores originais; inclusive filhos
com nomes que poderiam coincidir com uma propriedade nova.

## Física — incremento 2A

```lua
local bola = linblux.scene.create("part", {
    name="Bola", shape="sphere", position=linblux.vec3(10, 12, 0), anchored=false
})
bola.velocity = linblux.vec3(0, 2, 0)
bola:apply_impulse(linblux.vec3(15, 10, 0))
```

- `peça.velocity`: velocidade real do corpo Bullet; pode ser atribuída em peças
  dinâmicas. `apply_impulse(vetor)` aplica impulso central; `apply_force(vetor)`
  acumula força central para o próximo passo físico efetivo. Chamadas de força
  se somam; não são uma força persistente nem devem ser repetidas por FPS para
  simular um controlador de passo fixo. Peças ancoradas recusam essas escritas.
- `linblux.physics.gravity()` retorna a magnitude vertical. `set_gravity(valor)`
  aceita de 1 a 500 e atualiza tanto o Bullet quanto os cálculos do jogador.
- `physics.raycast(origem, direção, distância, peça_ignorada?)` retorna nil ou
  `{object, point, normal, distance}` com dados reais do Bullet. Ignora a cápsula
  do jogador; inclui peças mesmo sem resposta de contato. Direção não nula;
  distância positiva até 1 milhão de unidades.
- `physics.nearby(centro, raio)` retorna peças cujos **centros** estão no raio.
  É uma consulta de proximidade, não teste exato de interseção de volumes; não
  inclui o jogador. Raio de 0 a 1 milhão.

Essas operações são exclusivas da cópia de runtime. Consultas exatas de volumes
e navegação ainda dependem dos próximos incrementos.

### Passo fixo e contatos — incremento 2D

`linblux.events.on("physics_update", função)` recebe dt de 1/60 s imediatamente
antes de cada passo Bullet, independente dos FPS de exibição. Use esse evento
para forças contínuas. A cota de instruções do quadro é compartilhada pelos
subpassos; adicionar callbacks físicos não multiplica o orçamento. Temporizadores
usam o relógio da atualização, e uma espera zero cede até o próximo passe.

`collision_enter` e `collision_exit` recebem `{first, second, first_id, second_id}`.
As peças aparecem em ordem crescente de ID; o jogador usa ID 0 e o handle
persistente `linblux.player`. Um objeto removido pode aparecer como nil na saída,
mas seu ID é preservado. Escopo opcional em `on` filtra contatos do objeto ou de
seus descendentes. Contato é a presença de um par no Bullet, com tolerância de
0,02 unidade, não um sinal por ponto da superfície ou por quadro.

A engine coleta depois do passo e entrega num passe Lua seguinte, fora das
iterações Bullet. Remover/mover objetos no callback é permitido. Sensores com
`collidable=false` também notificam contatos quando Bullet gera o par; pares
entre dois corpos estáticos não são produzidos. Não é um detector contínuo de
regiões: objetos rápidos podem atravessar sensores entre passos.

Até 1024 pares simultâneos são acompanhados; as notificações compartilham a cota
de tarefas. Sob saturação, excedentes são descartados e a fila cheia gera um
diagnóstico uma vez por sessão. Mantenha callbacks curtos, filtre por escopo e
não dependa da entrega como protocolo de rede. A ausência de listeners dispensa
a coleta. Respawn encerra os contatos do avatar anterior.

```lua
linblux.events.on("collision_enter", function(hit)
    if hit.first_id == 0 then linblux.player:damage(10) end
end, linblux.self) -- Script filho de uma peça/sensor
```

## Jogador e câmera — incremento 2B

`linblux.player` referencia o jogador local real, sem um ponteiro de personagem
que fique inválido ao renascer. Propriedades editáveis: `health`, `max_health`,
`walk_speed`, `jump_height`, `auto_rotate`. Consultas: `position`, `velocity`,
`grounded`, `name`, `generation` e `state` (`idle`, `walking`, `jumping`, `falling`,
`landing`, `dead`). A posição corresponde ao centro da cápsula física.

Métodos: `player:damage(valor)`, `heal(valor)`, `respawn()`, `teleport(posição)`,
`set_checkpoint(posição?)` e `set_animation(estado, id)`. Animações usam estados
`idle`, `walk`, `jump`, `fall`; `linblux.animations.list()` retorna metadados dos
recursos presentes (id, nome em `name`, duração e loop). Nenhuma animação externa
é carregada automaticamente por esses métodos.

Teleporte exige cápsula livre e destino válido; não move o jogador para dentro
de um obstáculo. Checkpoint sem posição usa a posição atual. Na respawn, tenta o
checkpoint e depois os spawns normais se estiver obstruído. É estado da sessão,
liberado no Stop; o script salvo pode reconstruí-lo. Respawn recupera a vida máxima,
preservando o comportamento nativo anterior.

Eventos nativos adicionais: `spawn` recebe o jogador, `health_changed` recebe a
vida e `death` sinaliza morte. Para um HUD, leia a vida em start e spawn e acompanhe
as mudanças; respawn é anunciado pelo evento spawn, não por uma mudança de vida
obrigatória. Os eventos nativos não podem ser emitidos manualmente por Lua.

`linblux.camera.state()` retorna posição, alvo, distância, primeira pessoa e trava.
`camera.configure(tabela)` aceita `mode` (`orbit`, `follow`, `first_person`),
`distance`, `min_distance`, `max_distance`, `sensitivity`, `allow_first_person`,
`locked` e `allow_shift_lock`. A configuração é validada antes de ser aplicada;
o estado renderizado reflete a atualização seguinte. Preferências são da sessão,
sem regravar a experiência. Não se altera a câmera do editor.

## Eventos e ciclo de vida

```lua
local conexao
conexao = linblux.events.on("porta.abrir", function(dados)
    print(dados.mensagem)
    conexao:disconnect()
end)
linblux.events.emit("porta.abrir", {mensagem = "Olá"})
```

`events.on(nome, callback, objeto_de_escopo?)` retorna uma conexão com
`disconnect()` e `connected`. `emit(nome, dados?)` agenda callbacks, sem executá-los
recursivamente durante a emissão. O payload Lua é compartilhado entre os callbacks
daquela emissão: não é uma cópia profunda nem uma mensagem de rede.

Eventos de ciclo de vida: `start`, após o primeiro passe de inicialização dos scripts;
`update`, uma vez por atualização do runtime, com dt em segundos; `stop`, na saída,
antes de liberar a cena. Somente a engine emite eventos nativos. Para scripts novos, start é entregue somente aos listeners daquele script após
a inicialização. Lotes além da cota de tarefas são adiados, não descartados.
Registre start antes de aguardar uma tarefa; não é reproduzido para listeners registrados depois.
Stop cancela trabalhos pendentes, faz um passe com cota e libera tudo; seus
callbacks não podem aguardar. Start/stop recebem o valor numérico 0.

Ao remover/desabilitar o script ou destruir seu escopo explícito, callbacks e
tarefas pendentes são cancelados no próximo ponto seguro. Uma função que já está
executando pode terminar ou aguardar; não há preempção nativa no meio da função.
O evento `destroy` requer um objeto de escopo e entrega seu ID após a remoção,
num ponto seguro do scheduler. O observador precisa continuar existindo. A
conexão é removida após o callback e `disconnect()` cancela notificações pendentes.
Interações específicas são compostas pelo criador usando entrada, consultas e
eventos próprios; não existe um sistema de interação obrigatório.

## Entrada roteada — incremento 2E

`linblux.input` consulta uma amostra fornecida pelo frontend, nunca lê dispositivos
diretamente. No Studio, exige a posse de gameplay pelo roteador existente; texto,
widgets e menus preservam sua entrada. Mouse exige viewport sob o ponteiro ou
captura de gameplay. O Player só amostra com janela focada e jogo sem pausa.
Escape e teclas de função permanecem sob controle da engine e não são expostos.

- `key_down(nome)`, `key_pressed(nome)`, `key_released(nome)`; letras `a` a `z`,
  dígitos, `space`, `enter`, `tab`, `backspace`, `left/right/up/down`,
  `left_shift/right_shift`, `left_ctrl/right_ctrl`, `left_alt/right_alt`,
  `home/end`, `page_up/page_down`, `insert/delete`.
- `mouse_down/pressed/released(nome)`; nomes `left`, `right`, `middle`, `side`,
  `extra`. `pointer()` retorna `{x,y,dx,dy,wheel}` em pixels, com posição relativa
  ao viewport (à janela no Player).
- `gamepad_connected()` consulta o controle de índice 0; `gamepad_down/pressed/released`
  aceitam `up/right/down/left`, `north/east/south/west`, `left_bumper/right_bumper`,
  `left_trigger/right_trigger`, `select/start`, `left_stick/right_stick`.
  `gamepad_axis(nome)` aceita `left_x/left_y/right_x/right_y/left_trigger/right_trigger`;
  retorna o eixo bruto [-1,1], sem deadzone aplicada, ou 0 sem controle.

Eventos `key_pressed/key_released`, `mouse_pressed/mouse_released` e
`gamepad_pressed/gamepad_released` recebem o nome do botão e só são emitidos pela
engine. As bordas se referem a mudanças na amostra de gameplay, sem repetição por
tecla mantida; perder a posse gera liberações na atualização seguinte. Ao ganhar
posse, botões já mantidos podem gerar uma nova ativação. A pausa suspende Lua;
após continuar, o estado é comparado com a última amostra.

Consultas pressed/released são válidas durante o quadro de apresentação; dois
subpassos físicos podem consultar a mesma borda. Prefira eventos para ações únicas.
Não existe texto digitado, remapeamento persistente, múltiplos controles ou
vibração nesta API inicial. Controle físico não é simulado como teclado.

[interacao.lua](../assets/examples/lua/interacao.lua) alterna o sinal próprio das
portas com E ou o botão sul. Os testes executam o arquivo real com a API pública.

## Tempo

`linblux.time.now()` mede segundos de simulação; `wait(segundos?)` aguarda numa
tarefa; `spawn(função, ...)` agenda trabalho; `after(segundos, função, ...)`
agenda uma chamada. Intervalos aceitos: 0 a 3600 segundos. Uma espera zero cede
até o próximo passe. Pausa da sessão pausa o relógio. Temporizadores pertencem
ao script que os criou e são liberados ao parar.

`spawn` e `after` retornam uma tarefa. `tarefa.active` consulta se ainda existe
trabalho executável; `tarefa:cancel()` ou `linblux.time.cancel(tarefa)` cancela
a execução/espera pendente e retorna true se alterou seu estado. Repetir o
cancelamento ou usar um handle já concluído retorna false; ele não cancela uma
tarefa nova mesmo quando Lua reutiliza registros internos. Uma função que já
executa pode terminar até retornar ou ceder; o scheduler libera a tarefa num
ponto seguro. `task.spawn` legado mantém seu retorno anterior.

`every(segundos, função, ...)` retorna uma tarefa periódica cancelável, com
intervalo maior que zero e no máximo 3600 s. Aguarda antes do primeiro pulso e
antes de cada repetição, sem criar uma tarefa nova por pulso. Não acumula pulsos
atrasados; callback lento ou que aguarda alonga o período. Erro encerra o intervalo
e é reportado com traceback. Compartilha cotas, pausa e descarregamento das outras
tarefas. [temporizadores.lua](../assets/examples/lua/temporizadores.lua) é um exemplo
executado nos testes de salvar/reabrir e no Player independente.

`linblux_player --validate caminho` simula 2 segundos e descarrega a experiência;
falhas de Lua durante esse período, inclusive no stop, retornam saída 1 com
arquivo/linha. Saída 0 valida somente essa janela, não todas as ações futuras.

## Persistência e física

Código, vínculo pai/filho, atributos e transformações de autoria usam a
serialização existente; salve a experiência para persistir. O Mundo Padrão
publicado inclui os scripts e novas experiências recebem cópias independentes.
Objetos criados em Play são transitórios. Alterações Lua são aplicadas antes da
sincronização do Bullet, fora da iteração física. Uma peça dinâmica volta a ser
controlada pela física; use peças ancoradas para mover plataformas por script.
Não há callbacks Lua executados dentro do passo de Bullet nesta fase.

## Limites e ambiente controlado

Não são carregados os, io, package, debug, rede ou carregamento de binários.
load, loadfile, dofile, string.dump e coleta manual de lixo ficam indisponíveis.
pcall/xpcall não são oferecidos para impedir captura repetida da falha de cota.
Metatabelas comuns continuam disponíveis, mas `setmetatable` recusa qualquer
campo `__gc` não nil, inclusive false. O Lua desativa hooks nesses finalizadores,
o que permitiria contornar a cota; use o evento `stop` para cleanup. Referência:
[implementação oficial de finalização do Lua 5.4](https://www.lua.org/source/5.4/lgc.c.html#GCTM).
A API nativa recusa chamadas durante a liberação final da VM.
Módulos são recursos do próprio projeto, com limites de quantidade e profundidade.

Padrões: 8 MiB de Lua, 100 mil instruções por retomada, 250 mil por passe,
128 tarefas e 128 listeners. Conteúdo nativo continua limitado a 10 mil objetos
/ 8 MiB de nomes, fontes e atributos. Erros são registrados com script, linha e
traceback; uma tarefa com erro termina sem encerrar o jogo.

O host C++ pode usar `LuaRuntime::configure(Limits, erro)` com a VM parada.
A configuração nunca é lida automaticamente de um mapa importado nem exposta a
Lua. O allocator Lua não contabiliza toda a memória do processo ou do Bullet;
essa VM no mesmo processo, cotas são controles de confiabilidade e contenção,
não equivalem a isolamento do sistema operacional contra falhas nativas.
A cota de instruções também não mede todo o trabalho interno de funções C.

## Exemplos originais executados pelos testes

- [Plataforma móvel](../assets/examples/lua/plataforma.lua): Script filho da peça.
- [Porta por evento](../assets/examples/lua/porta.lua): Script filho da porta.
- [Sistema global](../assets/examples/lua/mundo.lua): cria objetos e aciona a porta.

Copie cada exemplo para o editor de scripts. O importador de cena declarativa
`.lua` não executa esses scripts e não deve ser usado para importá-los como cenas.
NPCs, navegação, entrada, dano, áudio, decals e UI na API nova são as fases seguintes.
