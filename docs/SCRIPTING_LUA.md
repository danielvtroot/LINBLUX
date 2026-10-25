# Lua do LINBLUX — API v1, implementação progressiva

A API nova é `linblux`, com `api_version == 1`. Os nomes antigos continuam
funcionando em projetos existentes. Novos exemplos usam os namespaces próprios.
Não se declara uma API completa de física, NPCs, áudio ou UI nesta fase.

## Inserir e executar

Selecione uma peça no Explorador e adicione um Script como filho dela. Edite o
código, salve e execute a experiência. Scripts globais ficam no serviço de scripts
do mundo. Script e LocalScript habilitados sob o mundo ou esse serviço são
inicializados ao entrar em execução; ModuleScript só executa por `require` com
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

Essas operações são exclusivas da cópia de runtime. Contatos, ciclo de passo
fixo, consultas de volumes e controle de personagem na API nova ainda dependem
dos próximos incrementos da Fase 2.

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

Eventos reservados: `start`, após o primeiro passe de inicialização dos scripts;
`update`, uma vez por atualização do runtime, com dt em segundos; `stop`, na saída,
antes de liberar a cena. Somente a engine emite esses três eventos. Registre start
antes de aguardar uma tarefa; não é reproduzido para listeners registrados depois.
Stop cancela trabalhos pendentes, faz um passe com cota e libera tudo; seus
callbacks não podem aguardar. Start/stop recebem o valor numérico 0.

Ao remover/desabilitar o script ou destruir seu escopo explícito, callbacks e
tarefas pendentes são cancelados no próximo ponto seguro. Uma função que já está
executando pode terminar ou aguardar; não há preempção nativa no meio da função.
Não existe callback público de destruição, colisão ou interação nesta fase.
Scripts clonados após o início ainda não são inicializados automaticamente.

## Tempo

`linblux.time.now()` mede segundos de simulação; `wait(segundos?)` aguarda numa
tarefa; `spawn(função, ...)` agenda trabalho; `after(segundos, função, ...)`
agenda uma chamada. Intervalos aceitos: 0 a 3600 segundos. Uma espera zero cede
até o próximo passe. Pausa da sessão pausa o relógio. Temporizadores pertencem
ao script que os criou e são liberados ao parar.

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
