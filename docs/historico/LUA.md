# Histórico — Lua, física e ciclo de execução

Registros de 25–27/10/2026, reunidos por assunto. São evidências das revisões
originais; não representam validação da versão atual. Consulte o
[índice atual](../README.md) para os manuais e comandos de uso.

<a id="registro-07"></a>

## Registro 07

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/07-scripting-lua.md).

### Fase 1 — API Lua LINBLUX v1 (25/10/2026)

Implementação real: namespace linblux, objetos part/group/model, criação validada,
consulta por nome/ID, transformações, material, cor, transparência, atributos,
hierarquia, clone/exclusão; eventos próprios com conexão/desconexão/escopo e
ciclo start/update/stop; tarefas/espera/timers; cotas configuráveis pelo host.
Scripts vinculados recebem contexto próprio e os bindings antigos permanecem.

ATLAS localizou LuaRuntime/Runtime/Scene. Não mapeou diretamente os testes de
LuaRuntime.cpp: busca direcionada identificou linblux_studio. Essa limitação foi
registrada no contrato. Não houve varredura global nem alteração do formato salvo.

Testes: FAST inicial aprovou 11/12 e encontrou uma regressão de entrega de morte
antes de respawn. Corrigida a distinção entre sinais legados já emitidos e novas
conexões canceláveis; regressão específica passou pelo supervisor TURBO em 0,82 s.
IMPACT aprovou 13/13 selecionados em 24,7 s (17,3 s de testes), sem falhas/timeouts,
com limite global de 120 s e individual de 45 s. Cobertura foi marcada PARCIAL:
apenas uma integração gráfica foi selecionada; outras onze ficaram excluídas.
Não é aprovação FULL. A suíte Studio registrou 1555 verificações e zero falhas,
incluindo os exemplos Lua originais executados e recarregados do disco.

As verificações incluem criação, alterações, eventos, timers, contexto por peça,
remoção de listeners e tarefas órfãs, cleanup, erro com linha/traceback, recusa de
bibliotecas/caminhos do sistema, loops infinitos, falta de memória, limites do
host, preservação da autoria e scripts legados. Studio e Player compilaram.

Backup: status do AutoBackup foi consultado: monitor parado, nenhum checkpoint
próprio/push registrado. systemctl não está disponível. O estado prévio foi
preservado em .turbo/phase1-scripting-baseline: bundle verificado, patch binário,
estado Git e tar dos arquivos não rastreados. Esse backup local não protege
contra perda do computador. Código rastreado desta fase é protegido por commits
e push normal; alterações preexistentes de outras tarefas não foram incluídas.

Limites: não há inicialização automática de scripts clonados durante Play nem
callback público de destruição, colisão ou interação na API nova. Física avançada,
personagens, NPCs, áudio, assets e UI continuam nas fases seguintes. As cotas da
VM não representam isolamento de processos nem toda a memória/trabalho nativo.
API e exemplos documentados em docs/SCRIPTING_LUA.md.

Commit de implementação: 3fd2f03. Push normal para origin/main e hash remoto
confirmados. Compilação incremental final de Studio, Player e teste Studio:
8,87 s, aprovada. Documentação registrada em commit separado.

<a id="registro-08"></a>

## Registro 08

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/08-fisica-lua-2a.md).

### Fase 2 — incremento 2A: física real por Lua

ATLAS localizou PhysicsWorld e RuntimeSession. Foram acrescentadas operações
validadas do runtime para velocidade, força, impulso, gravidade e raycast de
peças; nenhum ponteiro Bullet é entregue ao Lua. O namespace linblux.physics
oferece esses raios e busca por proximidade de centros. Todas as operações são
aplicadas fora do passo de Bullet, na cópia de execução.

Arquivos: Runtime.{h,cpp}, LuaRuntime.cpp, tests/studio_main.cpp e o guia Lua.
Nenhum formato salvo foi alterado. A consulta de raios ignora a cápsula do
jogador; os bindings legados permanecem intactos.

TURBO FAST: 12/12 selecionados aprovados, zero falhas/timeouts. Compilação em
95,6 s, testes em 10,3 s, total 106,3 s. Run: 20261025T070746Z-27267, orçamento
120 s de testes, timeout individual 60 s. O teste novo verifica movimento real,
velocidade após impulso, gravidade, ponto/normal/distância do raio, peça ignorada,
proximidade e rejeição de vetores inválidos e corpos ancorados. Testes de runtime,
spawn, persistência, padrões e compatibilidade também passaram nessa seleção.

Não é a conclusão de toda a Fase 2: contatos, atualização física por evento,
controle do personagem/câmera, áudio e UI seguem pendentes na API nova. Não se
rodou FULL nem se repetiram capturas da Home, que não foi alterada.

<a id="registro-09"></a>

## Registro 09

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/09-jogador-lua-2b.md).

### Fase 2 — incremento 2B: jogador, câmera e encerramento seguro

Implementado em Runtime/LuaRuntime: jogador real por linblux.player, dano, cura,
vida, velocidade de caminhada, salto, rotação automática, respawn, teleporte com
consulta de obstrução e checkpoint de sessão com fallback aos spawns normais.
Animações são listadas do projeto e atribuídas ao controlador nativo. Câmera é
consultável/configurável sem alterar a câmera do editor. Eventos nativos novos:
spawn, health_changed e death. Sinais legados continuam preservados.

Uma falha no teste novo esperava 100 de vida após respawn com máximo 150. A
implementação existente restaura o máximo; a expectativa foi corrigida para 150,
preservando o comportamento nativo. FAST inicial: 11/12; regressão específica
corrigida passou pelo supervisor TURBO em 1,67 s, com timeout de 5 s. Studio,
Player e teste Studio compilaram em 15,09 s na rodada final. Não houve FULL.

A inspeção de segurança identificou finalizadores __gc como caminho sem hook de
instruções no Lua. setmetatable agora recusa __gc inclusive false, preservando
metatabelas normais. O evento stop é a alternativa de cleanup com cota. Chamadas
nativas durante a liberação final são bloqueadas; testes verificam recusa,
metatabelas comuns, liberação e encerramento sem tarefas pendentes. Referência
primária e limites estão em SCRIPTING_LUA.md. Isso não constitui sandbox de SO.

Câmera real em primeira pessoa, alteração de vida, eventos, checkpoint/respawn,
recusa de teleporte obstruído e preservação da autoria foram testados. Alterações
locais de outras tarefas permanecem fora deste commit. A Fase 2 continua em
incrementos: contatos, passo fixo, entrada, áudio e UI ainda precisam de integração.

<a id="registro-10"></a>

## Registro 10

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/10-ciclo-de-vida-lua-2c.md).

### Fase 2 — incremento 2C: ciclo de vida dos componentes Lua

Scripts clonados habilitados agora entram em execução. Desativar cancela tarefas
e listeners; reativar reinicia o contexto, inclusive no mesmo quadro, sem duplicar
callbacks. O evento start é direcionado ao script novo. Deslocar um script para
fora da árvore executável interrompe sua execução. ModuleScript preserva require.

O evento destroy requer escopo e entrega o ID removido a um observador ainda
existente. A conexão é removida depois da entrega; desconectar antes cancela a
notificação enfileirada. Nenhuma dessas operações executa Lua dentro de Bullet.

ATLAS localizou LuaRuntime e RuntimeSession. O impacto de LuaRuntime.cpp não
mapeou diretamente o teste; a inspeção dirigida confirmou linblux_studio. Foram
alterados LuaRuntime.cpp, studio_main.cpp, o guia Lua e o contrato de scripting.

Validação final: compilação pelo TURBO de Studio, Player e teste Studio em
37,02 s. FAST 20261025T080212Z-5393: 12/12 testes aprovados, zero falhas/timeouts,
11,9 s em testes, 22,5 s totais incluindo build incremental de 5,5 s. A regressão
verifica clonagem, ativação, reinício no mesmo quadro, uma única assinatura ativa,
destruição, cancelamento e preservação da cena de autoria. Não foi executado FULL
nem validação visual, pois este incremento altera o scheduler de runtime.

Alterações locais de outras tarefas foram preservadas. Próximo incremento:
passo fixo e eventos de contato reais; áudio, UI Lua e demais fases seguem pendentes.

<a id="registro-11"></a>

## Registro 11

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/11-contatos-lua-2d.md).

### Fase 2 — incremento 2D: passo fixo e contatos Bullet

Implementado physics_update antes dos passos de 1/60 s e collision_enter/exit
depois da coleta real de manifolds. Lua nunca executa durante a enumeração de
contatos ou a simulação Bullet. Eventos são enfileirados e canceláveis, filtrados
por objeto/grupo. Respawn encerra os contatos do avatar anterior. Forças contínuas
agora podem ser aplicadas por passo físico sem depender do FPS de apresentação.

Arquivos: Runtime.{h,cpp}, LuaRuntime.{h,cpp}, studio_main.cpp, guia Lua e contrato
de scripting. ATLAS localizou runtime/física, confirmou consumidores dos headers;
tests-for Runtime.cpp não mapeou teste diretamente. O impacto do header encontrou
os testes de game, experiências, defaults, spawn e studio, cobertos pelo FAST.
ATLAS também listou cópias em .turbo como referências: são ruído do índice,
não fontes compiladas; não foram alteradas.

TURBO FAST 20261025T080624Z-7885: 12/12, zero falhas/timeouts, build 40,4 s,
testes 9,3 s, total 50 s. Depois foi acrescentada a regressão de novo contato
após respawn: Studio/Player/teste recompilados em 13,03 s; regressão pelo supervisor
TURBO em 0,365 s, 1621 verificações, zero falhas. Timeout individual de 10 s.
Não houve FULL, captura visual ou nova bateria gráfica: este incremento não altera
a apresentação e foi exercitado com cena, scripts, controlador e Bullet reais.

Testado: 60 callbacks físicos por segundo a 30 e 120 FPS com mesma velocidade;
entrada/saída em sensor sem resposta sólida; callback que move ou destrói peça;
contato do jogador antes e depois de respawn; autoria intacta e runtime válido.

Limites explícitos: até 1024 pares, fila compartilhada com cotas Lua; excedentes
podem ser descartados, com diagnóstico de fila cheia. Não é detecção contínua de
regiões, nem inclui pares estático-estático. Navegação, entrada roteada, áudio,
UI Lua e as fases posteriores continuam pendentes. O guia documenta a API real.

<a id="registro-12"></a>

## Registro 12

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/12-entrada-lua-2e.md).

### Fase 2 — incremento 2E: entrada roteada e interação por Lua

Implementado linblux.input com estados e bordas de teclado, mouse e controle,
posição relativa ao viewport, movimento/roda do mouse e eixos do controle 0.
Eventos key/mouse/gamepad_pressed/released usam o scheduler existente, com cotas
e cancelamento por script. Escape e teclas de função ficam reservados à engine.
Lua não faz polling dos dispositivos nem altera a posse decidida pelo frontend.

Studio respeita o roteador existente de gameplay, foco, widgets e menus; mouse
exige viewport sob o ponteiro ou captura. Player exige foco e ausência de pausa.
As amostras são transitórias, não mudam formatos serializados. O exemplo original
interacao.lua usa E ou botão sul para emitir o evento das portas já implementadas.

Arquivos: ScriptInput.h, Runtime.{h,cpp}, LuaRuntime.cpp, duas linhas de integração
em GamePanels.cpp, main.cpp do Player, studio_main.cpp, rml_ui_main.cpp, registro
do teste em CMakeLists, exemplo, guia Lua e contrato de scripting. As traduções
preexistentes em GamePanels foram preservadas fora do índice/commit.

ATLAS encontrou PlayerInput, drawGameplayViewport e o contrato game. A inspeção
dirigida confirmou a posse do teclado e as regras de captura. As strings de eventos
são alocadas dentro de chamada Lua protegida, evitando panic nativo por cota de
memória ao receber uma tecla.

TURBO FAST 20261025T081458Z-12225: 12/12 aprovados, zero falhas/timeouts,
build 72,9 s, testes 9,9 s, total 84 s. Resultado global PASSED_PARTIAL/cobertura
desconhecida por CMake e mudanças locais fora deste incremento; isso não é FULL.
Compilação final de Studio, Player e ambos os testes em 4,72 s. Validação final
pelo supervisor TURBO: studio + script_input_ui, 2/2 em 9,625 s, timeout 45 s.
Studio: 1643 verificações; integração RmlUi/X11: 33; zero falhas.

Verificado com janela real em Xvfb/software: tecla mantida só dispara uma vez,
clique no viewport chega ao Lua, coordenadas relativas válidas, pausa não dispara
a ação, continuar restaura a entrada e Shift+F5 encerra Play preservando a autoria.
Teste headless inclui controle/eixos por amostra injetada, desconexão, bordas,
liberação ao parar e execução do arquivo interacao.lua. Não houve teste com
controle físico nem medição de GPU integrada; não houve FULL ou redesign visual.

AutoBackup foi consultado: monitor parado, sem checkpoint/push próprio. Commits
destes incrementos são enviados manualmente a origin/main; não se afirma que
arquivos locais ignorados, privados ou fora dos commits estejam protegidos.

Fase 2 continua: áudio e UI por Lua ainda faltam. NPCs/navegação, gerenciador de
assets, personagem sem rosto obrigatório e nova interface do Player permanecem
nas próximas fases. Home, vídeo e editor 3D não foram redesenhados neste incremento.

<a id="registro-14"></a>

## Registro 14

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/14-tempo-lua-e-validacao-player.md).

### Missão de conclusão — Fase 1, incremento de tempo e diagnóstico

A Fase 1 inteira NÃO está concluída. Esta unidade fecha tarefas canceláveis,
intervalos e validação de erros no Player, sem implementar APIs fictícias de
NPCs, áudio ou UI. A Fase 0 foi publicada em cd4fb406cfccc6b030e56eadbede0c3965ef66f3.
Código deste incremento publicado em 26066dff73dc128c50987de2f5f25dfcf71ff89b,
hash remoto main confirmado após push normal. Este registro documental é posterior
ao commit de código para não presumir sucesso de um envio ainda pendente.

Implementado em LuaRuntime: spawn/after modernos retornam tarefa com active,
cancel e token próprio; cancelar uma referência antiga não atinge tarefas novas
mesmo com reutilização do registro Lua. every usa um coroutine com espera entre
pulsos, cotas compartilhadas e liberação no Stop. task.spawn legado preservado.
O exemplo temporizadores.lua é carregado do arquivo real, salvo/reaberto pelo
ProjectIO e executado no runtime. Erro no intervalo encerra só aquela tarefa.

Player --validate agora retorna falha por erro Lua, inclusive no Stop. Um contador
da sessão evita falso sucesso quando prints removem o erro do buffer limitado de
mensagens. O contador continua disponível após Stop e é zerado no próximo Play.
Arquivo/linha/traceback são mantidos quando presentes no buffer; erros evictos são
indicados com referência aos logs. A janela de validação é de 2 s de simulação.

Arquivos: LuaRuntime.{h,cpp}, Player/main.cpp, studio_main.cpp, novo harness
lua_player_validation.py, CMakeLists, temporizadores.lua, guia Lua e contrato.
Nenhum formato de cena/experiência mudou; não altera a autoria durante Play.

ATLAS localizou o agendador, Runtime/PlayerCamera e ProjectIO; o código real
confirmou a ausência de cancelamento/intervalos e o retorno falso do Player.
Compilação final Studio/Player/teste pelo TURBO em 73,21 s (header afetado).
FAST 20261025T154144Z-30094: 13/13, zero falhas/timeouts, build 32,3 s e testes
9 s, total 41,5 s. Studio: 1655 verificações, zero falhas. Teste standalone:
exemplo válido retorna 0; erro inicial, erro no Stop e erro evicto retornam 1;
todos preservam os bytes da cena salva. Não são mocks do Player.

IMPACT 20261025T154331Z-31455: 18/18 selecionados, zero falhas/timeouts em
46,5 s/46,8 s totais. Sem build adicional após a compilação incremental/FAST.
Gráficos excluídos nesta alteração de scheduler/CLI: nenhuma apresentação mudou.
Cobertura global parcial por CMake/diffs locais; não houve FULL e isso não é
aprovação integral de todos os sistemas da engine.

Limites: cancelamento não interrompe código C++ ou uma função já em execução;
o próximo retorno/yield encerra a tarefa. Intervalo não recupera pulsos atrasados.
Não há persistência arbitrária de closures. Scripts/configurações de autoria
continuam no formato existente; armazenamento de progresso de jogo está pendente.

Próximo passo: seguir a matriz da Fase 1 em CONCLUSAO_ENGINE.md. Antes de NPCs,
completar o contrato de transformações/componentes, câmera e persistência controlada,
resolvendo explicitamente as dependências de áudio/UI das fases 3/6. Não chamar
esta unidade de "Lua completo" nem pular os requisitos restantes.

<a id="registro-15"></a>

## Registro 15

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/15-camera-lua.md).

### Missão integral — câmera Lua

Implementado: FOV vertical persistido em PlayerSettings (1..170 graus, 60 quando
ausente no JSON antigo), edição nos padrões de câmera do Studio, configure Lua,
pose por posição/alvo/up ou Euler em graus, transição suave com quaternion,
consultas reais e release para a órbita anterior. Mouse/zoom não alteram poses;
cursor fica livre mesmo com câmera travada nos padrões. ESC continua protegido.
Pausa congela transição; teleporte mantém pose; respawn reinicia a câmera nativa.

Arquivos: Runtime.{h,cpp}, Rig.{h,cpp}, GameAssetsIO.cpp, LuaRuntime.cpp,
GamePanels.cpp (somente dois trechos), Player/main.cpp, chave de localização,
studio_main.cpp, rml_ui_main.cpp, lua_player_validation.py, camera.lua e contratos.
Outros diffs locais do editor/locales/TURBO preservados fora do commit.

ATLAS localizou Runtime, PlayerSettings, GameAssetsIO e bindings; confirmou os
consumidores do header. FAST 20261025T162427Z-8590: 13/13, sem falhas/timeouts,
build 122,5 s (consumidores dos headers), testes 13,8 s, total 137,7 s. Compilação
final Studio/Player/studio_tests/rml_ui_tests: 5,03 s após ajuste direcionado.
Integração supervisionada camera-lua-integration: studio + lua_player_validation
+ script_input_ui, 3/3 em 13,693 s, timeout global 60 s e timeout gráfico CTest 45 s.

IMPACT 20261025T163049Z-12921: 13/13 selecionados em 10,7 s (11,5 s totais),
sem rebuild redundante. Cobertura global parcial: gráficos gerais omitidos e o
teste gráfico pertinente foi executado separadamente pelo supervisor TURBO.
Não houve FULL; não se declara validação integral de todos os sistemas.

Verificado: salvar/reabrir FOV; aceitar JSON antigo sem campo e recusar FOV inválido
sem mutar cena; transição/rotação oposta sem vetor degenerado; pausa; release;
cena autoral intacta. Exemplo real camera.lua executado pelo Player; pose inválida
recusada. No Studio gráfico, FOV real, cursor/RMB, ESC/retomada e release testados.

Limites explícitos: pose é estado de execução reconstruído por Script salvo;
configure altera padrões sem cancelar pose; release é explícito. Poses não fazem
desvio automático de obstáculos e movimento usa a referência de órbita anterior.
Não há câmera do editor exposta ao Lua. A Fase A/Lua continua em desenvolvimento;
próxima unidade: transformações relativas e componentes programáveis existentes.

<a id="registro-16"></a>

## Registro 16

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/16-transformacoes-componentes-lua.md).

### Missão integral — transformações e componentes Lua

Implementado: transformação mundial atômica, conversão explícita para referencial
de outra peça, conversão de pontos/direções, escala positiva e interpolação com
quaternion. Criação de Script filho, listagem/remoção de componentes reais, cotas
e descarregamento no scheduler existente. Sem formato novo ou alteração da autoria.

Arquivos: LuaRuntime.cpp, studio_main.cpp, lua_player_validation.py,
transformacoes.lua, guia Lua, contrato de scripting e acompanhamento.

ATLAS localizou Part::transform, RigidTransform/Compose/Inverse, Scene e scripts.
Primeiro FAST: 11/13; exemplo tentava colocar peça como filha de peça, operação
recusada pelo contrato real da cena. Corrigido para grupo e referência espacial
explícita, preservando o formato e os projetos antigos; sem ampliar parenting
às cegas. A API não promete vínculo espacial automático de filhos.

FAST final 20261025T164433Z-20435: 13/13, zero falhas/timeouts, build 19,6 s,
testes 9,8 s, total 29,8 s. IMPACT 20261025T164543Z-21632: 13/13, zero falhas,
build 24,6 s/testes 15,4 s/total 42,2 s; cobertura global parcial sem gráficos.
Validação adicional pertinente do exemplo com raycast Bullet após transformar:
studio + Player, 2/2 em 0,819 s pelo supervisor TURBO, timeout 20 s.

Testado: transformações ida/volta e rotação de 170 a -170 graus; escala efetiva
no corpo Bullet (raycast real encontra a superfície transformada); Script gerado
executa em seu objeto e é removido; salvar/reabrir e Player independente usam
o arquivo de exemplo real. Tamanho inválido não move parcialmente a peça;
referência não espacial, escala negativa, remoção de componente alheio, NUL no
nome e fonte acima de 256 KiB são recusados. Componente alheio continua executando.

Limites: referenciais rígidos usam posição/rotação, size é dimensão física, não
herda escala; modelos/grupos continuam organizacionais. Componentes gerados em
Play são transitórios, reconstruídos pelo Script salvo. Decals são listáveis/
removíveis, mas sua edição/importação Lua pertence à fase de imagens. A missão
e a fase de Lua permanecem incompletas; persistência controlada é a próxima unidade.

<a id="registro-17"></a>

## Registro 17

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/17-persistencia-privada-lua.md).

### Missão integral — persistência privada Lua

Implementado ProgressStore com get/set/remove/status, JSON limitado, escopo
fornecido pelo Studio/Player, salvamento atômico e trava entre sessões. Dados
privados separados de assets/exportação/Git. Cenas sem documento existente,
padrão da engine e --validate usam memória. Início cancelado descarta alterações;
pausa/Stop tentam salvar. Falhas preservam arquivo existente e geram diagnóstico.

Limites/política/formato em PERSISTENCIA_PRIVADA.md e SCRIPTING_LUA.md. Sem API
de caminho, arquivo arbitrário, rede ou comandos do Linux. Estado de jogo não
é cena autoral nem segue no pacote público. Destino de backup privado externo
continua pendente e não bloqueia os demais recursos.

Arquivos: ProgressStore.{h,cpp}, Runtime.{h,cpp}, LuaRuntime.{h,cpp}, Player/main,
ExperienceUI, trecho de GamePanels, CMake, testes studio/rml/player, persistencia.lua,
.gitignore e exclusão na política do AutoBackup. Diffs alheios do TURBO, editor
e locales preservados fora do commit.

ATLAS localizou requestPlay, Runtime::start e identificadores de instalação.
Captura do escopo ocorre no frontend antes do trabalho assíncrono; Lua não recebe
permissão para escolher namespace/pasta. O getter protege a alocação Lua com pcall
interno sem perder a liberação dos objetos C++ em caso de cota de memória.

Compilação inicial Studio/Player/studio_tests: 118,67 s (headers). Compilação
final desses alvos e rml_ui_tests: 24,19 s. FAST final 20261025T172332Z-11941:
14/14, zero falhas/timeouts, build 4,4 s + testes 12,7 s = 17,5 s totais.
Integração supervisionada progress-studio-player-integration: 4/4 em 14,43 s,
timeout global 75 s, cada gráfico CTest limitado a 45 s. Studio salva/para/reabre
e recupera valor; dois processos gráficos do Player avançam contador 1→2;
--validate mantém os bytes de progresso e da cena intactos; arquivo modo 0600.

IMPACT 20261025T172935Z-17013: 18/18 em 49,4 s (50 s totais), sem falhas/timeouts.
Sem rebuild redundante; gráficos gerais excluídos, pertinentes executados acima
com supervisor. Cobertura global parcial, sem FULL ou alegação de missão completa.

Regressões: cancelar início não grava; outra sessão não toma a trava; corrupção
preservada; link para arquivo válido recusado sem alterar destino; limites de
valor/leitura/alteração, funções/ciclos/metatabelas/tabelas mistas recusados.
Progresso normal recupera após fechar, JSON autoral permanece idêntico.

Limites: gravação pequena síncrona a cada 2 s de simulação e na pausa/saída;
hardware de disco com falha não tem garantia de latência. Encerramento abrupto
pode perder valores pendentes. Cenas avulsas movidas mudam escopo; instalações
gerenciadas preservam ID ao mover, novas importações recebem ID novo. Não existe
reset silencioso de dados corrompidos. Próximo: ampliar física/controle de personagem.

<a id="registro-18"></a>

## Registro 18

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/18-consultas-controle-lua.md).

### Missão integral — volumes Bullet e controle Lua

Implementado overlap_box/overlap_sphere com contactTest real, caixas orientadas,
filtros de sensores/peça/jogador, resultados estáveis e cota host-only de consultas
por quadro (padrão 8). Coleta nativa termina antes de construir os handles Lua.
Truncamento é conservador: pode haver candidato broadphase não examinado, sem
garantia de outra interseção exata. Limites documentados em SCRIPTING_LUA.md.

Jogador recebe intenção horizontal world/camera, salto pelo controlador existente,
velocidade, força e impulso na cápsula real. Intenção dura um quadro e retorna à
entrada normal se não for renovada. FixedUpdate pode definir intenção antes do
passo corrente. Não há navegação/NPCs ou eventos contínuos de região anunciados.

Arquivos: Runtime.{h,cpp}, LuaRuntime.{h,cpp}, consultas_controle.lua, testes
studio/player, guia e contrato physics. ATLAS localizou Runtime, Bullet e
consumidores; impacto cobre Studio/Player e testes de runtime.

Build focado Studio/Player/studio_tests: 114 s, aprovado. FAST
20261025T174655Z-26450: compilação adicional dos consumidores em 62,6 s;
13/13 testes em 13,6 s, total 79 s, sem falhas/timeouts. O exemplo real é salvo,
reaberto e executado; verifica sensores, exclusão de jogador, peça ignorada,
limite de resultados, caixa orientada sem falso positivo por AABB, impulso,
movimento e salto. Player também recusa nove consultas no quadro e movimento
vertical inválido, preservando os bytes das cenas.

IMPACT 20261025T174825Z-27825: 13/13 em 11,6 s (12,2 s totais); cobertura global
parcial, gráficos gerais omitidos. Integração gráfica pertinente script_input_ui
aprovada, 1/1 em 20,792 s, supervisor com timeout 45 s; log
.turbo/logs/spatial-control-input.log. Não houve FULL nem alegação de conclusão
integral. AutoBackup confirmado ativo, checkpoint/push 17e2422d0925, sem arquivos
cobertos pendentes. Dados privados continuam fora do backup público de código.

Próximo incremento: propriedades físicas persistentes das peças, depois NPCs
e dependências de áudio/UI; fases globais permanecem abertas.

<a id="registro-19"></a>

## Registro 19

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/19-propriedades-fisicas-lua.md).

### Propriedades físicas persistentes — entrega e continuidade

IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER: BodyProperties tipado com densidade,
atrito, restituição e amortecimento linear/angular; configuração em Propriedades
RmlUi e ImGui; API Lua body, mass, angular_velocity, apply_torque e
apply_angular_impulse. Densidade atualiza massa/inércia no Bullet sem reconstruir
o corpo nem zerar velocidade; ancoragem mantém o comportamento anterior.

Persistência: JSON body opcional (cenas antigas usam os valores anteriores),
clonagem, cena declarativa Lua e pacote .lo. Valores inválidos são recusados sem
alterar parcialmente propriedades ou substituir a cena carregada. Exemplo real:
assets/examples/lua/corpos_rigidos.lua, executado no Studio e Player independente.

Arquivos: Part.{h,cpp}, PhysicsWorld.{h,cpp}, Runtime.{h,cpp}, Scene.cpp,
LuaRuntime.cpp, ProjectIO.cpp, SceneLua.cpp, EditorUI.cpp (somente o hunk próprio),
EditorActions.cpp, StudioFrontend.cpp, cinco traduções próprias no locale,
CMakeLists.txt, testes studio/player/experiences/rml, exemplo e documentação.
Alterações preexistentes de TURBO, editor e traduções foram preservadas.

ATLAS localizou Part, PhysicsWorld, ProjectIO, os dois inspetores e consumidores.
Índice atualizado. TURBO compilou Studio/Player/studio_tests em 129,97 s; FAST
20261025T175451Z-31589 recompilou consumidores em 164,2 s e executou 13/13 testes
em 12,3 s (176,9 s totais). Cobertura global desconhecida devido ao CMake alterado.
IMPACT 20261025T175800Z-1719: 18/18 em 56,4 s (56,9 s totais); cobertura global
parcial, sem FULL. Nenhuma alegação de conclusão integral.

Regressões verificam massa/inércia, impulso, atrito e amortecimento efetivos,
quique real, mudança de densidade, ancoragem, rotação, validação atômica, cena
antiga sem body, clonagem e persistência. O pacote .lo recupera os cinco valores.

Primeiro teste gráfico falhou por xdotool interpretar -1 como opção e atingiu
45 s. Corrigido com -- e menos quadros redundantes no caminho focado. Rebuild
rml_ui_tests/experience_tests em 25,65 s. Validação final supervisionada:
linblux_body_properties_ui + linblux_experiences, 2/2 em 37,405 s, sem falhas;
log .turbo/logs/body-properties-ui-final.log. Os cinco campos foram editados por
X11, densidade negativa recusada, cena salva/reaberta e Play executou Lua com
massa 60 e impulso real. Screenshot geral inspecionado em
/tmp/loblox-rml-ui-70d5VY/body-properties.png; os campos físicos ficam abaixo
do trecho visível quando o filtro é limpo. Não há redesign visual nesta unidade.

A fase Lua inteira continua EM DESENVOLVIMENTO: áudio/UI e dependências restantes
não foram implementados nesta unidade. NPCs e navegação ainda não implementados;
somente localização inicial pelo ATLAS, sem código de pathfinding. Próximo passo:
planejar e implementar navegação física limitada ao terreno efetivamente
suportado, controle/rig/vida de NPCs e exemplos usando APIs públicas, mantendo
as dependências de áudio e UI explícitas. Não recriar runtime ou Bullet.

Interrupção solicitada pelo usuário por limite de créditos: nenhuma fase nova
iniciada. Commit/push desta unidade registrados no relatório central e resposta
final após confirmação remota. Dados privados externos continuam sem destino de
backup configurado; o monitor de código permanece ativo.

PUBLICADO: 4ce62e2b980bbb92ee7d9174ff56ec57a7bcf5f5; push e hash remoto main confirmados.

