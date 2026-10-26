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

### Câmera de experiência e transições

`camera.configure` também aceita `fov` vertical entre 1 e 170 graus. O campo
cameraFov dos padrões do jogador é persistido; cenas antigas sem ele usam 60.
No Studio, o padrão pode ser editado em “Campo de visão vertical”.

`linblux.camera.set_pose({position=vetor, target=vetor, up=vetor?, fov=número?,
duration=segundos?})` assume a câmera da experiência. Alternativamente use
`rotation=vec3(pitch,yaw,roll)` em graus no lugar de target/up. Duração de 0 a
60 segundos, padrão 0. Posição e alvo não podem coincidir; up não pode ser zero
nem paralelo à direção. Opções inválidas são rejeitadas antes de alterar a câmera.

Durante uma pose, mouse/zoom não movem a câmera e não forçam captura do cursor.
ESC e o menu da aplicação continuam funcionando. A transição usa interpolação
suave de posição/FOV e quaternion para orientação, evitando colapso na rotação
de 180 graus. Pausa congela a transição. Teleporte preserva a pose; respawn
reinicia a câmera nativa. Poses não fazem desvio automático de obstáculos.

`camera.release()` devolve o controle ao jogador, restaurando yaw/pitch/distância
anteriores e os padrões atuais (incluindo FOV). `configure` altera os padrões,
sem encerrar uma pose: use release explicitamente. O controle de movimento do
jogador mantém a referência de órbita anterior durante a pose.

`camera.state()` inclui `position`, `target`, `up`, `rotation` em graus, `fov`,
`scripted` e `transitioning`, além das consultas anteriores. A pose é transitória;
salve o Script para reconstruí-la ao executar. [camera.lua](../assets/examples/lua/camera.lua)
é executado após salvar/reabrir e também pelo Player independente.

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

## Transformações e componentes de script

Uma peça expõe `transform` como cópia `{position, rotation, size}`; rotação em
graus. Atribuir essa tabela aplica a transformação mundial de uma vez. Campos
omitidos preservam o valor anterior; valores inválidos não aplicam alterações
parciais. `get_transform(referencial?)` e `set_transform(tabela, referencial?)`
convertem posição/rotação para o espaço local de outra peça. Sem referencial,
usam o mundo. `size` continua em unidades físicas, sem herdar escala do referencial.

`point_to_world`, `point_to_local`, `vector_to_world`, `vector_to_local` recebem
um vetor: pontos incluem translação, direções não. `scale_by(fator)` aceita
número uniforme ou vetor de fatores positivos, respeitando tamanho mínimo 0,2
e máximo 1 milhão por eixo. `linblux.transform.interpolate(a,b,t)` interpola
posição/tamanho e usa quaternion para rotação (t entre 0 e 1); campos omitidos
nesse cálculo usam posição/rotação zero e tamanho unitário.

Não há mudança de semântica na hierarquia: grupos/modelos não são referenciais
espaciais automáticos; mover um referencial não arrasta outras peças. Peças não
são aceitas como filhas de peças pelo formato existente. Use referência explícita
e reaplique a transformação quando desejar acompanhamento por script.

`objeto:add_script(fonte, nome?)` adiciona um Script real, descoberto na próxima
atualização se estiver na árvore executável. Fonte até 256 KiB, nome 1..128 bytes
sem NUL, sujeito às cotas nativas. `components()` lista filhos Script/LocalScript/
ModuleScript/Decal/Texture; `remove_component(componente)` remove apenas um desses
filhos diretos, respeitando proteção. `type` identifica script/decal/texture.
Adicionar/remover não apaga imagens ou arquivos do projeto. Interfaces de imagem
e propriedades de decals são tratadas na fase de assets, não simuladas aqui.

Componentes criados em Play pertencem à cópia de runtime. Salve o Script criador
para reconstruí-los; a autoria não é alterada ao executar. Os scripts adicionados
usam o mesmo ambiente controlado, cotas, eventos e descarregamento. Veja
[transformacoes.lua](../assets/examples/lua/transformacoes.lua), executado após
salvar/reabrir e no Player real.

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

### Estado privado de execução

`linblux.storage.get(chave, padrão?)`, `set(chave, valor)`, `remove(chave)` e
`status()` operam somente o escopo definido pelo host para a experiência aberta.
Não aceitam caminho ou nome de arquivo. Valores: nil/null, booleanos, números
finitos, textos, tabelas de chaves textuais ou arrays contínuos desde 1. Funções,
userdata, metatabelas, tabelas mistas/cíclicas e dados fora dos limites são recusados.
`get` retorna uma cópia; alterá-la não grava até chamar set. remove retorna se
uma chave existia. Nil armazenado é null, distinto de chave ausente com padrão.

Limites: 128 chaves de 1..128 bytes, 64 KiB por valor JSON, 256 KiB no conjunto,
profundidade 8 e 1024 nós por valor, 128 leituras e 64 alterações por quadro.
Sem acesso livre a io/os/rede. O escopo, a pasta e o modo persistente são escolhas
C++ do Studio/Player, não configurações confiadas ao mapa.

`status()` informa persistent, pending e error (quando houve falha de gravação).
Mudanças ficam imediatamente disponíveis em memória; gravação automática limitada
a uma por 2 s e tentativa final ao pausar/parar. Um encerramento abrupto pode
perder alterações ainda pendentes. Escrita usa temporário, fsync, rename e fsync
do diretório. Pastas/arquivos recusam links; arquivo criado com modo 0600 e pasta
privada 0700. Outra sessão simultânea do mesmo escopo é recusada por trava.
Arquivo corrompido/versão incompatível não é sobrescrito: o erro é informado.

Experiências gerenciadas usam installationId, preservado quando a instalação é
movida; importar um pacote cria instalação nova e estado privado novo. Cenas
JSON avulsas usam caminho canônico. Estado fica em
`$XDG_STATE_HOME/linblux/progress` ou `~/.local/state/linblux/progress`, separado
dos assets e da exportação `.lo`. Copie essa pasta junto à estratégia privada de
backup; GitHub não protege esse progresso. Não copie dados privados para assets.

Cenas não salvas, edição do padrão da engine e `--validate` usam somente memória,
com persistent=false, sem ler/gravar o progresso real. Cancelar o carregamento
descarta as alterações dessa tentativa, inclusive callbacks de Stop. O formato
do mundo continua o mesmo: configurações autorais/objetos são persistidos pela
experiência; storage é progresso/configuração privada do jogador, não exportado.

O exemplo [persistencia.lua](../assets/examples/lua/persistencia.lua) é executado
no runtime e no Player. A integração gráfica verifica dois processos independentes,
reinício do Studio e validação sem modificar progresso. `--check-scripts` permite
ao Player gráfico retornar saída 1 após encerrar se houve erros de script.

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
NPCs, navegação, imagens/decals e áudio estão implementados (ver as últimas
seções). UI por Lua continua dependência em desenvolvimento.
Entrada e dano já estão disponíveis conforme as seções acima.

## Consultas de volumes e controle do jogador

`linblux.physics.overlap_box(centro, tamanho, opcoes?)` e
`overlap_sphere(centro, raio, opcoes?)` retornam `objetos, truncado`. O Bullet
testa a geometria real, inclusive sensores (`collidable=false`), não só centros
ou caixas envolventes. Opções: `rotation` Euler em graus (apenas caixa),
`ignore` (peça), `include_player` (padrão true), `solid_only` (false) e
`max_results` (1–256, padrão 256). Resultado ordenado por ID; jogador tem ID 0
e type="player". `truncado=true` significa que candidatos de broadphase não
foram examinados após atingir o limite; não garante outra interseção exata.

Limites: oito consultas por quadro compartilhadas entre scripts e subpassos;
o host pode configurar `Limits.spatialQueries` (1–128) com a sessão parada.
Raio até 1000; dimensões de caixa entre 0,001 e 2000; coordenadas finitas até
1 milhão. Consultas sincronizam mudanças de peças antes de ler o mundo físico,
sem chamar Lua dentro da enumeração Bullet. Não há assinatura contínua de
regiões nesta API: o script compara os conjuntos de IDs de cada consulta.

`linblux.player:move(direcao, referencia?)` define intenção horizontal XZ até o
próximo quadro. Referência "world" (padrão) ou "camera"; -Z representa frente.
Magnitude acima de 1 é normalizada. Sem chamada no próximo quadro, o controle
normal do usuário retorna. `:jump()` solicita salto ao controlador existente,
respeitando chão, coyote time e buffer. Não é teletransporte.
`player.velocity = vetor`, `:apply_force(vetor)` e `:apply_impulse(vetor)`
atuam na cápsula Bullet. O controlador pode alterar a velocidade horizontal no
passo seguinte; forças não substituem intenção de movimento. APIs recusam uso
com jogador morto ou sessão fora de execução.

[consultas_controle.lua](../assets/examples/lua/consultas_controle.lua) é salvo,
reaberto e executado pelo teste Studio e pelo Player independente, com
verificações de caixa orientada, exclusão de falsos positivos, salto e movimento.

## Propriedades dos corpos rígidos

Peças expõem `body`, uma cópia de `{density, friction, restitution,
linear_damping, angular_damping}`. `peca.body={density=2}` aplica somente os
campos informados, validando toda a alteração antes de modificar a peça.
`scene.create("part",{body={...}})` aceita os mesmos campos. Limites: density
0,001–1000; friction 0–10; restitution e damping 0–1. Padrões preservam o
comportamento anterior: 1, 0,8, 0, 0,02 e 0,1, respectivamente.

`peca.mass` lê a massa real no Bullet: volume × densidade, mínimo 0,1 para
peças dinâmicas; ancoradas têm massa zero. Mudar densidade preserva a velocidade
atual (portanto não conserva momento linear). Redimensionar recalcula massa e
inércia. Forma esférica usa o menor eixo como diâmetro, como antes.
`angular_velocity` lê/escreve velocidade angular em radianos por segundo;
`:apply_torque(vetor)` e `:apply_angular_impulse(vetor)` usam eixos globais.
Força/torque são consumidos pelo passo físico; para efeito contínuo renove-os
em fixed_update. Não aplicam movimento a peças ancoradas.

Propriedades físicas aparecem no Studio (RmlUi e ImGui), são clonadas e salvas
no objeto JSON em `body` com `linearDamping`/`angularDamping`. Campo ausente em
cenas antigas usa os padrões anteriores. Leitura inválida rejeita o documento
sem substituir a cena atual. Exportação declarativa Lua preserva os campos.
Valores modificados durante Play continuam transitórios, como as demais
propriedades; edite no Studio e salve para persistir na autoria.

[corpos_rigidos.lua](../assets/examples/lua/corpos_rigidos.lua) demonstra massa,
impulsos, rotação, amortecimento e alternância ancorada/dinâmica no Player real.

## NPCs e navegação

Um NPC é uma **Part comum** da cena com o componente `NpcProperties`
(`enabled`, `speed`, `maxHealth`, `respawnDelay`, `turnSpeed`). Não há
personagem fixo no C++: o corpo é criado por `scene.create("part", {...})` (ou
autorado no Studio) e o comportamento vem de um Script. Os dados são
serializados no objeto JSON (`npc`), por isso o NPC sobrevive a salvar e
reabrir. Vida e posição de respawn são estado de execução e não são gravadas.

```lua
local npc = linblux.npc.create{
    name = "Guarda", position = linblux.vec3(0, 2.6, 0),
    size = linblux.vec3(2, 5, 2), speed = 10, max_health = 100,
    respawn_delay = 0, turn_speed = 10,
    script = "print('componente do NPC')",   -- opcional
}
linblux.npc.attach(peca, {speed = 6})   -- transforma uma peça existente
linblux.npc.detach(peca)
local ok = linblux.npc.move_to(npc, linblux.vec3(20, 2.6, 0)) -- false = inacessível
linblux.npc.stop(npc)
linblux.npc.state(npc)      -- idle | moving | arrived | unreachable | dead
linblux.npc.speed(npc)      linblux.npc.set_speed(npc, 12)
linblux.npc.health(npc)     linblux.npc.max_health(npc)
linblux.npc.set_health(npc, 40)  linblux.npc.damage(npc, 10)  linblux.npc.heal(npc, 5)
linblux.npc.target(npc)     -- destino (vec3) ou nil
linblux.npc.path(npc)       -- waypoints restantes
local pontos, alcancavel = linblux.npc.find_path(de, para)
local nav = linblux.npc.navigation() -- {ready, cells, walkable, cell_size, columns, rows}
```

**Movimento.** O NPC é um corpo dinâmico conduzido no passo fixo: a velocidade
horizontal aponta para o próximo waypoint e a vertical continua sob gravidade.
Assim paredes, escadas e o jogador são resolvidos pelo Bullet existente; o
teletransporte só existe no respawn configurado. A rotação é travada (o NPC
não tomba) e o giro respeita `turn_speed` (rad/s). Degraus de até 1,2 unidade
são vencidos quando há espaço livre; saltos e desníveis maiores não são
suportados.

**Navegação.** Uma grade XZ é amostrada a partir da física real: só entram
superfícies e obstáculos **estáticos** (`anchored`) sólidos. Cada célula exige
piso com normal ≥ 0,7, altura livre de 4,5 e folga lateral de 1,0. O caminho
vem de A* com 8 vizinhos (sem cortar cantos), custo diagonal e suavização por
linha de visão. O agente padrão é uma caixa dinâmica; corpos maiores que os
corredores medidos podem ficar presos, o que vira `unreachable` após duas
tentativas de replanejamento. Destinos em ilhas isoladas, dentro de paredes ou
sobre plataformas altas retornam `false` sem mover o NPC. A grade é construída
sob demanda, uma vez por sessão, limitada a 4096 células; depois de mover
geometria estática chame `linblux.npc.navigation()` ou recrie o NPC para
reconstruí-la. Não há navmesh triangulada, saltos entre plataformas, desvio
dinâmico de outros agentes nem suporte a rampas com inclinação acima do limite:
a movimentação continua restrita às superfícies caminháveis.

**Ciclo de vida.** Ao parar a execução todos os controladores são liberados;
destruir a Part (`part:destroy()`) descarta o controlador no passo seguinte e
`linblux.npc.detach` devolve a rotação livre ao corpo. Os NPCs são apenas
objetos da cena de execução: a cena autoral nunca é modificada por Play.

Exemplos reais, salvos e executados pelos testes Studio e pelo Player:

- [npc_patrulha.lua](../assets/examples/lua/npc_patrulha.lua): ciclo de
  waypoints com espera em cada ponto e tratamento de destino inacessível.
- [npc_perseguidor.lua](../assets/examples/lua/npc_perseguidor.lua): detecta o
  jogador no raio, replaneja quando ele se move e para ao perder o alvo.
- [npc_monstro.lua](../assets/examples/lua/npc_monstro.lua): persegue, ataca por
  proximidade com intervalo, aplica dano na vida real do jogador e recebe dano
  pelo evento `atacar_monstro`.

## Imagens, decals e assets da experiência

A referência canônica de mídia é sempre `assets/<arquivo>` relativa à raiz da
experiência. `game::NormalizeImageReference` é a única regra de normalização:
UI, persistência (`ProjectIO`/`GameAssetsIO`) e `walkImages` a usam, então
projetos antigos que gravaram apenas o nome do arquivo são migrados ao abrir e
regravados canônicos ao salvar. Nenhum caminho absoluto do computador é
gravado.

- Importar imagem: `game::ImportMediaTo(dir, origem, MediaKind::Image, ...)`
  valida PNG/JPEG (32 MiB de arquivo, 64 MiB decodificado), copia com ID FNV-1a
  do conteúdo e devolve `assets/<id>.<ext>`. Reimportar o mesmo arquivo reusa o
  destino.
- Aplicar em peça: atributos `decal_image` / `texture_image` (mais
  `decal_face`, `decal_transparency`, `texture_*`). Em rigs: `bodies[i].image`
  ou `rig:add_decal`.
- Remover: `ExperienceManager::removeAsset(ref, usado)` recusa assets citados
  pelo manifesto (ícone/banner/rosto) e o editor recusa quando
  `ImageInUse`/`DescribeSoundUsage` encontram a referência na cena, em atributos,
  na biblioteca de sons ou no código de um Script.
- O rosto da fábrica não é mais obrigatório: uma experiência sem `faceImage`
  é válida; o rig do personagem é a fonte visual e o render só usa o rosto
  embutido quando o projeto não define nenhum.

## Áudio MP3, WAV e OGG

Importe em **Studio → Recursos → Sons locais**; o arquivo vai para `assets/`
da experiência e a biblioteca do projeto (`GameAssets.sounds`) guarda
`id/nome/referência/duração/volume/loop/kind`. A importação lê apenas os
metadados do container (FFmpeg, com orçamento de 2 s e whitelist de
container/`file`) e recusa MP3/WAV/OGG inválidos, Opus e duração não finita
antes de gravar o arquivo.

```lua
local handle = linblux.audio.play("musica_fundo", { volume = 0.4, loop = true })
linblux.audio.play("assets/<id>.ogg", { volume = 1, owner = peca })
linblux.audio.pause(handle)      linblux.audio.resume(handle)
linblux.audio.set_volume(handle, 0.6)   linblux.audio.volume(handle)
linblux.audio.set_loop(handle, false)   linblux.audio.looping(handle)
linblux.audio.is_playing(handle)        linblux.audio.is_paused(handle)
linblux.audio.duration(handle)       -- segundos reais (metadados)
linblux.audio.duration("musica_fundo")
linblux.audio.is_streaming(handle)   -- true quando o backend abriu um Music
linblux.audio.device_ready()         -- false em CI/headless
linblux.audio.stop(handle)       linblux.audio.stop_all()
for _, som in ipairs(linblux.audio.list()) do print(som.id, som.kind, som.duration) end
```

Reprodução 2D global, sem atenuação por distância; cada `play` devolve um
handle independente (até 64 vozes simultâneas). O `owner` opcional amarra a voz
a um objeto: quando ele é removido da cena, a voz é liberada no próximo
`update`. `Studio` e `Player` abrem o dispositivo de áudio **antes** de iniciar
os scripts. Sem dispositivo (CI/headless) o runtime mantém o mesmo contrato de
handles/pause/stop e conduz o término pela duração real dos metadados, sem
fingir um stream. `stop()` da sessão encerra todas as vozes.

Exclusão segura de áudio: `DescribeSoundUsage` identifica objeto/atributo/script
que cita a referência, o `id` ou o nome; `RemoveSoundAsset` recusa enquanto
houver uso, move o arquivo para um backup temporário, salva a cena e restaura
arquivo + metadados se o salvamento falhar. O arquivo externo original do
usuário nunca é apagado.

Limitação real: não há áudio 3D/espacial nem mixer por categoria. `Studio`
pré-escuta com um `AudioSystem` próprio (volume/loop/pausa/parar); o `Player`
usa o dispositivo da janela.

Exemplos reais:

- [audio_musica_fundo.lua](../assets/examples/lua/audio_musica_fundo.lua):
  música em loop com controle de volume e pausa por teclas.
- [audio_som_interacao.lua](../assets/examples/lua/audio_som_interacao.lua):
  som ao entrar/sair do contato com uma peça.
