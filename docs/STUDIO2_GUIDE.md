# Studio 2 — recursos disponíveis e limites

## Explorer e Properties

Home → Insert Folder/Model ou botões + do Explorer criam agrupadores reais.
Selecione um grupo e insira Part pela ribbon/Toolbox/menu contextual. Arraste
linhas da árvore sobre Folder/Model para mudar Parent. IDs, ponteiros e
transforms mundiais são preservados; grupos não têm transform próprio.
Parts aninhadas continuam renderizadas e registradas no Bullet.

Busca encontra nome/classe; ancestrais da seleção são revelados. F2 renomeia;
Delete apaga subárvores não protegidas; Ctrl+D duplica a subárvore. Ctrl+C/V
ainda referencia a origem viva: se ela for excluída, copie novamente. Não há
seleção múltipla nem undo/redo de cena nesta versão. Campos do Script Editor
têm o undo/redo de texto do ImGui.

Properties oferece busca, categorias e colunas ajustáveis. Part: Name, ID,
classe, Parent, Position/Rotation/Size, Color, Transparency, Material, Anchored,
CanCollide. Folder/Model: dados e Parent. Script: Enabled, contexto e Source.
Enter confirma Name/transforms; Esc cancela o campo; inválidos são rejeitados.
Lighting e Workspace conservam seus controles anteriores.

View mostra/oculta painéis e Reset Layout reconstrói o dock padrão. O layout
salvo continua tendo preferência; use Reset Layout para experimentar o novo.

## Lua executável, diferente de importar uma cena .lua

Instale `liblua5.4-dev`, reconfigure CMake e compile. Script → New Script cria
um asset (sem seleção, em ServerScriptService). Selecionar um objeto válido
permite criar Script filho dele. ModuleScript vai para ReplicatedStorage por
padrão. Double-click em Script abre o editor; F2 continua renomeando.

Script Editor tem abas, fonte de código editável, linhas, busca indicando a
primeira linha encontrada, Tab, undo/redo de texto, Revert, Save All + Project
e Play. Ctrl+S dentro do editor salva os drafts e o projeto. Digitar/F5 no
source não aciona gameplay. Play aplica drafts em memória, sem forçar escrita
no disco. Source com erro não impede o controller nativo de funcionar.
Syntax highlighting, busca com navegação e autocomplete ainda não existem.

Script e LocalScript executam uma vez ao iniciar Play quando Enabled e
descendentes de Workspace/ServerScriptService. LocalScript é contexto lógico
local do único jogador, não uma VM cliente isolada ou rede. ModuleScript só
executa em `require(handle)`, cacheado por ID durante o Play. ServerStorage e
ReplicatedStorage armazenam módulos/scripts sem autoexecução. Não aceitam
Parts físicas; o editor informa erro ao tentar mover uma subárvore com Parts
para fora de Workspace. Não existem serviços vazios de multiplayer/StarterGui.

```lua
local world = game:GetService("Workspace")
local box = world:FindFirstChild("Part")
if box then
    box.Anchored = true
    box.Position = Vector3.new(0, 5, 0)
    box.Color = Color3.fromRGB(80, 160, 240)
    box:SetAttribute("Score", 10)
end

game:GetService("Players").CharacterAdded:Connect(function(character)
    character.Humanoid.WalkSpeed = 20
    character.Humanoid.JumpHeight = 7
end)

task.wait(1)
print("Uma tarefa do mapa terminou")
```

API atual:

- `game:GetService`: Workspace, Lighting, ServerScriptService, ServerStorage,
  ReplicatedStorage, Players, RunService. Serviços desconhecidos geram erro.
- Instance: Name, ClassName (readonly), Parent, filhos por nome,
  FindFirstChild, GetChildren, GetDescendants, Destroy, Clone, GetAttribute,
  SetAttribute. Clone cria irmão com ID novo, não uma instância órfã.
- Instance.new: Part, Folder, Model; Parent opcional usa Workspace.
- Part: Position/Size/Rotation (graus), Color (RGB 0–255), Anchored, CanCollide,
  Transparency. Vector3.new e Color3.fromRGB retornam tabelas X/Y/Z validadas.
  Não há operadores aritméticos de Vector3 nem CFrame.
- Players.LocalPlayer.Character; Players/LocalPlayer.CharacterAdded;
  Character.Humanoid e Character.Animator. Humanoid: WalkSpeed, JumpHeight,
  AutoRotate, State (readonly). Animator:SetAnimation("Idle"/"Walk"/"Jump", ID)
  valida rig/asset e muda o clip realmente usado pelo controller.
- Humanoid: Health/MaxHealth, TakeDamage, Heal, HealthChanged, Died,
  HealthDisplayType, HealthDisplayDistance, NameDisplayDistance.
  Players.LocalPlayer:Respawn() usa o reset controlado.
- AnimationService:GetAnimation("anim://id") retorna Id, Name, RigType,
  Duration, Loop e Version do asset local, sem nuvem ou IDs Roblox.
- RunService.Heartbeat:Connect(callback(dt)), connection:Disconnect().
- task.spawn(function, ...), task.wait(segundos), print/warn,
  require(ModuleScript). Módulos não podem yieldar durante require.
- Attributes: bool, número finito ou string; nil remove. Até 64 por Instance,
  chave 64 bytes, string 4096 bytes.

Output mostra prints/erros com Script, arquivo/linha e traceback. JSONL também
registra script.output/script.error. Mensagens ficam limitadas a 256 × 2048
bytes. Stop cancela tarefas/conexões, fecha a VM e conserva mensagens no Output.
Novo Play usa uma VM limpa e a cópia runtime; nunca executa no editor.

Projetos novos recebem R6/Idle/Walk/Jump e os scripts Game.server.lua e
CharacterAnimations.server.lua. Mapas existentes não são sobrescritos ao
abrir: Script → Install default game scripts é opt-in e preserva scripts com
esses nomes. A engine nativa cuida de spawn/movimento/respawn; o script padrão
apenas configura animações por ID. Exemplo:

```lua
game:GetService("Players").CharacterAdded:Connect(function(character)
    character.Animator:SetAnimation("Idle", "anim://r6-idle")
    character.Animator:SetAnimation("Walk", "anim://r6-walk")
    character.Animator:SetAnimation("Jump", "anim://r6-jump")
end)
```

Animation Editor → Copy ID copia o URI estável; hover mostra ID, rig e duração.
Renomear clip não muda o ID. Alterações salvas em keys são usadas no próximo
Play. ID inválido gera erro sem substituir a associação válida anterior.
Jump padrão conserva a pose aérea anterior, agora editável; mapas antigos sem
Jump asset usam a pose nativa. Não há blending de prioridades Action em camadas.
Scripts de animação têm precedência sobre Game Settings quando chamam
SetAnimation em CharacterAdded. Edite seus IDs ou desative o script para usar
somente as atribuições de Game Settings.

## Vida e void

Health/MaxHealth padrão 100, sem regeneração automática. Dano/cura devem ser
finitos e não negativos; Health é limitada a 0..MaxHealth. Morrer bloqueia
locomoção/pulo, para a pose de locomoção, emite Died uma vez e respawna após
Respawn delay de Game Settings. Initial Health vale no primeiro spawn;
respawn restaura MaxHealth. Cura não revive uma personagem Dead. Reset do menu
usa a mesma transição de morte, com respawn imediato. Conexões de vida da
personagem antiga são liberadas ao respawn, sem duplicar eventos a cada reset.

HUD fica no canto inferior da viewport, com número e barra suavizada. Overhead
health é opt-in: AlwaysOn/DisplayWhenDamaged/AlwaysOff, distâncias em Game
Settings. Usa cabeça articulada, evita atrás da câmera e consulta obstáculos;
não é desenhada em primeira pessoa. Painéis flutuantes de edição são ocultos
durante Play para não cobrir HUD/gameplay, sem perder seu layout autoral.

Part → Properties → Damage configura o attribute numérico Damage. Contato
Bullet sólido aplica o maior dano entre as Parts tocadas, com cooldown global
de 0,75 s para evitar múltiplas aplicações nos subpassos. CanCollide=false
não causa dano. Não existe um evento Touched genérico anunciado como pronto.

Workspace.FallenPartsDestroyHeight (default -500) é editável/persistido e
acessível em Lua. Abaixo dele, jogador morre e respawna; Parts dinâmicas
deletáveis são removidas só do runtime, liberando Bullet e invalidando handles.
Não há chão infinito. Um spawn abaixo do limite é rejeitado com erro.

## Limites da VM

Lua 5.4 in-process: 8 MiB de alocações Lua, 128 tarefas, 128 conexões,
64 módulos, profundidade require 16, source 256 KiB/Script, 100.000 instruções
por resume e 250.000 por tick. Exceder quota termina a tarefa e produz erro;
não é um orçamento rígido de CPU dentro de funções nativas da biblioteca Lua.
O limite Lua não inclui todos os objetos nativos/renderer/Bullet. Bindings de
criação/Clone/attributes também impõem 10.000 nós e 8 MiB de conteúdo nativo
(nomes/source/attributes) para limitar expansão; não é medida exata de RSS.
Clone calcula o custo da subárvore antes de duplicar, incluindo source.

Sem os/io/debug/package/rede, arquivos, load/loadfile/dofile, bytecode,
coroutine, pcall/xpcall ou string.dump. pcall/xpcall não são expostos para que
scripts não capturem repetidamente erros de quota. Há math/string/table/utf8
e base limitada. require só aceita ModuleScript do projeto. Handles verificam
ID e geração, incluindo personagens antigos após respawn.

Isso reduz erros acidentais, mas NÃO é uma fronteira de segurança contra código
hostil. Bibliotecas nativas e a VM compartilham processo; projetos maliciosos
exigiriam isolamento de processo/OS. Não execute código não confiável como se
essa política oferecesse proteção absoluta.

Referência técnica: [manual oficial Lua 5.4](https://www.lua.org/manual/5.4/).

## Persistência e teste

JSON v3 estende o formato com grupos, parent/order e source/Enabled/attributes.
Mapas simples continuam v1; rigs sem extensões Studio continuam v2. v1/v2
abrem com defaults; scripts não são injetados/sobrescritos ao abrir projetos.
Escrita atômica e staging de carga continuam iguais. O exportador declarativo
Lua v1 recusa conteúdo v3 em vez de perder hierarquia/scripts silenciosamente.

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j2
./build/loblox
```

Teste manual: criar Folder aninhada, arrastar Part, editar Parent e transform,
criar Script, digitar exemplo, salvar, Play, conferir efeito/Output, Stop,
comparar mapa autoral, fechar/reabrir. Capturas/testes automatizados ficam em
`/tmp/loblox-studio-ui-*`; não são projetos pessoais.

Ainda pendentes nesta missão: Shift Lock, importer/Decal/Texture, nova cabeça/rosto,
logo integrada, skybox e novos shaders/perfis. Os PNGs originais foram apenas
inspecionados e preservados; não são apresentados como recursos integrados.
