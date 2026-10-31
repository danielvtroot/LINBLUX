# Histórico — Objetos, personagens, itens e recursos

Registros de 25–27/10/2026, reunidos por assunto. São evidências das revisões
originais; não representam validação da versão atual. Consulte o
[índice atual](../README.md) para os manuais e comandos de uso.

Manuais atuais: [Modelos](../MODELOS.md), [Attachments](../ATTACHMENTS.md), [Inventário](../INVENTARIO.md), [Áudio](../AUDIO_MUSICA.md) e [Imagens](../RECURSOS_DE_IMAGEM.md).

<a id="registro-20"></a>

## Registro 20

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/20-npcs-e-navegacao.md).

### 20 — NPCs programáveis e navegação 3D

Data: 25/10/2026. Base: `48dba40`. Escopo: backend C++ (cena, física, runtime
Lua, persistência), exemplos Lua e testes. Nenhum arquivo de frontend foi
alterado: RmlUi, ImGui, HUD, menu do Player, tema, locales e layout permanecem
intactos.

#### Incrementos

| Incremento | Estado | Entrega |
| --- | --- | --- |
| 1 — Controle de NPC por Lua | IMPLEMENTADO, TESTADO | `NpcProperties` na Part (dados serializados), `game::NpcSystem`, namespace `linblux.npc` (criar, anexar, desanexar, velocidade, destino, parar, consultas). |
| 2 — Navegação e caminhos | IMPLEMENTADO, TESTADO | `game::NavGrid`: grade XZ amostrada da física real, A* 8-vizinhos sem cortar cantos, suavização por linha de visão, destino inacessível explícito, degrau limitado a 1,2. |
| 3 — Patrulha e perseguição | IMPLEMENTADO, TESTADO | `npc_patrulha.lua` e `npc_perseguidor.lua`, ambos executados pelo Studio e pelo Player. |
| 4 — Vida, dano e monstro | IMPLEMENTADO, TESTADO | Vida/max/vida/respawn por NPC, morte e `npc_monstro.lua` com ataque intervalado usando a vida real do jogador. |
| 5 — Persistência, exemplos e testes | IMPLEMENTADO, TESTADO | Campo `npc` no JSON do projeto, três exemplos reais, testes headless e validação no Player. |

Publicação: os incrementos 1–4 compartilham os mesmos arquivos de implementação
(`Npc`, `Navigation`, `Runtime`, `PhysicsWorld`, `LuaRuntime`). Foram validados e
publicados como uma unidade funcional, com os artefatos de exemplo, teste e
documentação na sequência.

Commits (push normal em `origin/main`):

- `5f29424d29f20ddecec53fe6557db9ce9f2b324e` — implementação (cena, física,
  navegação, runtime Lua, persistência, CMake).
- `1fad7e30786040412d5c56909e606f03c01df901` — exemplos, testes, docs e este
  relatório; hash confirmado em `origin/main`, acima de `48dba40`.

Após o envio, uma linha redundante (`error.clear()` em `RuntimeSession::start`)
foi removida; `linblux_studio`, `linblux_game` e `linblux_lua_player_validation`
foram reexecutados e passaram 3/3 com o código final.

#### API Lua adicionada (namespace `linblux.npc`)

- `create{name, position, size, color, material, shape, parent, script,
  script_name, speed, max_health, respawn_delay, turn_speed, enabled}` → Part.
- `attach(peca, opcoes?)`, `detach(peca)`.
- `move_to(peca, destino)` → `true` quando existe rota; `false` para destino
  inacessível (estado `unreachable`), sem mover o NPC.
- `stop(peca)`, `state(peca)` (`idle|moving|arrived|unreachable|dead`),
  `target(peca)`, `path(peca)`.
- `speed(peca)`, `set_speed(peca, n)`, `health`, `max_health`, `set_health`,
  `damage`, `heal`.
- `find_path(de, para)` → pontos, alcançável.
- `navigation()` → `{ready, cells, walkable, cell_size, columns, rows}`.

Nenhum personagem fixo foi criado no C++: o NPC é uma Part comum com componente
e o comportamento é sempre script.

#### Implementação

- `src/objects/Part.{h,cpp}`: `NpcProperties` (já preparado localmente em
  trabalho anterior) com validação e marcação de corpo sujo ao anexar/remover.
- `src/game/Npc.{h,cpp}`: controlador por `InstanceId`, movimento por velocidade
  no passo fixo, giro limitado por `turn_speed`, morte, respawn configurável,
  detecção de travamento com replanejamento (máx. 2) e descarte de
  controladores de Parts destruídas.
- `src/game/Navigation.{h,cpp}`: grade derivada apenas de Parts estáticas
  sólidas (piso com normal ≥ 0,7, altura livre 4,5, folga lateral 1,0), no
  máximo 4096 células, construída sob demanda; A* com custo diagonal e
  orçamento de expansão.
- `src/physics/PhysicsWorld.{h,cpp}`: travamento de rotação para Parts com NPC,
  `setBodyYaw` sem tocar na velocidade, `isSpaceClear` para validar o degrau e
  filtros `solidOnly`/`staticOnly` no raycast.
- `src/game/Runtime.{h,cpp}`: `NpcSystem` por sessão, recriado no Play, liberado
  no Stop, executado dentro do passo fixo após `syncScene` e antes do passo
  Bullet.
- `src/scripting/LuaRuntime.cpp`: namespace `linblux.npc` com validação de
  opções antes de criar qualquer objeto.
- `src/serialization/ProjectIO.cpp`: campo `npc` no objeto JSON, leitura
  estrita e validação; ausência do campo mantém cenas antigas idênticas.

#### Exemplos

- `assets/examples/lua/npc_patrulha.lua` — rota em ciclo, espera em cada ponto,
  registro de ponto inacessível.
- `assets/examples/lua/npc_perseguidor.lua` — raio de detecção, replanejamento
  quando o alvo se move, parada ao perder o alvo.
- `assets/examples/lua/npc_monstro.lua` — perseguição, ataque com intervalo,
  dano na vida real do jogador e dano recebido pelo evento `atacar_monstro`.

#### Testes e resultados

- `linblux_studio` (`tests/studio_main.cpp`, `LinbluxNpc` + `NpcExamples`):
  1824 verificações, 0 falhas (1,016 s). Cobre criação, múltiplos NPCs, muro
  contornado (nenhum quadro com o corpo dentro do muro), destino inacessível,
  chegada, patrulha, perseguição, reposicionamento de alvo, perda de alvo,
  dano/cura/morte/respawn, destruição, `detach`, serialização JSON e liberação
  no Stop.
- `linblux_lua_player_validation`: 23 casos, incluindo quatro novos
  (`NpcPatrulha.lua`, `NpcDano.lua`, `NpcInvalido.lua`, `NpcSemComponente.lua`)
  executados no binário `linblux_player --validate`. Cena salva preservada.
- TURBO FAST (`20261025T191055Z-9045`): compilação aprovada, 13/13 testes,
  15,3 s de 300 s de orçamento.
- TURBO IMPACT (`20261025T191148Z-10201`): compilação aprovada, 22/22 testes,
  84,6 s de 400 s, cobertura global parcial (declarada pelo TURBO). Inclui
  `linblux_studio`, `linblux_game`, `linblux_tests`, `linblux_experiences`,
  `linblux_lua_player_validation`, `linblux_script_input_ui` e
  `linblux_progress_player_ui`.
- Nenhum FULL. Nenhum teste gráfico novo de UI foi executado por não haver
  alteração de interface.

#### Limitações reais da navegação

- Grade XZ, não navmesh triangulada; o mundo plano é amostrado em células
  (2,5 unidades por padrão, ajustadas para caber em 4096).
- Só superfícies e obstáculos **estáticos** entram na grade. Corpos dinâmicos
  (NPCs, caixas móveis) colidem mas não são desviados; mover geometria estática
  exige reconstruir a grade (`linblux.npc.navigation()`).
- Sem saltos, sem saltos entre plataformas, sem ligação por rampa acima do
  limite de normal (0,7) e sem desvio dinâmico de outros agentes.
- Degraus de até 1,2 unidade são vencidos com verificação de espaço livre;
  desníveis maiores viram `unreachable`.
- O agente padrão da grade é uma caixa de raio 1,0 e altura 4,5; NPCs maiores
  que os corredores medidos podem ficar presos e terminam `unreachable`.
- Destinos fora dos limites da grade ou em ilhas isoladas retornam `false` sem
  mover o NPC.
- Vida, destino e waypoints são estado de execução: não são gravados. A posição
  inicial (dados da Part) e as propriedades de NPC são.
- Reposicionamento só existe no respawn configurado; não é usado como
  deslocamento normal.

#### Próximo incremento pendente

Fase 2 da continuidade segue EM DESENVOLVIMENTO: rigs/animações para NPCs,
combate com armas e projéteis, e as demais demonstrações exigidas pela fase
(oito demonstrações reais). Áudio (fase 3), imagens/decals (4), rosto de fábrica
(5), UI Lua (6), vida circular (7), menu lateral (8) e integração (9) continuam
pendentes. O destino externo privado do AutoBackup continua não configurado.

<a id="registro-22"></a>

## Registro 22

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/22-modelos.md).

### Modelos — pivô, transformação de conjunto e Lua

IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER: ModelFrame tipado; seleção múltipla
com Ctrl; agrupar/desagrupar preservando poses; pivô sem mover geometria; mover,
girar e escalar uniformemente nas Propriedades; Modelos aninhados, clonagem com
IDs novos, scripts por Modelo e API Lua com conversão local/mundial. Modelo não
cria corpo ou solda automática. Gizmos de conjunto e undo composto seguem pendentes.

Arquivos principais: Instance, Scene, LuaRuntime, ProjectIO, EditorActions,
StudioFrontend, EditorUI/StudioPanels, ribbon, CMake e testes studio/player/
experiences/rml; exemplo modelos.lua e guia MODELOS.md. ATLAS consultado antes
de implementar; hierarquia/clonagem/runtime existentes foram ampliados.

Duas regressões encontradas: duplicateTree invalidava a lista de filhos ao
clonar Parts (GDB em .turbo/logs/models-gdb.log); corrigido com snapshot da lista.
Rotação de 90° perdia precisão no ciclo Euler. Cenas com pivô explícito adotam
v4: pivô e orientação das Parts persistem quaternions. Leitura v1/v2/v3 mantém
as regras anteriores; projetos sem pivô novo mantêm sua versão. Testes conferem
a orientação composta dos filhos após reabrir, em vez de reduzir a exigência.

Cota nativa de 32 operações custosas de Modelos por quadro compartilhada pelos
scripts; exemplo e teste de excesso executados no Player real. Validação de
tamanho/posição acontece antes de modificar filhos; escala não uniforme não é
suportada. Grupos recusam objetos protegidos/repetidos e seleção pai+filho.

FAST final 20261026T004226Z-7953: 17/17, build286,1 s, testes12,4 s, total299 s.
Cobertura global desconhecida por CMake/teste novo. Build final focado Studio,
Player e rml_ui_tests: 2,37 s. IMPACT 20261026T020406Z-15199: 20/20 em53,8 s
(54,3 s totais), cobertura global parcial, sem FULL. Falhas anteriores de Studio
(crash e precisão) foram corrigidas; não são apresentadas como aprovações.

linblux_models_ui: 1/1 em29,644 s; supervisor65 s e CTest60 s. X11 seleciona
duas Parts, agrupa, altera posição/rotação/escala/pivô, salva/reabre, executa
Lua em Play, para e desagrupa sem alterar as posições. Captura inspecionada:
/tmp/loblox-rml-ui-pHAiUX/model-properties.png. Log .turbo/logs/models-ui.log.
Headless também verifica raycast Bullet após mover, clonagem, seleção após
remoção, JSON inválido sem alteração, exportação/importação .lo e scripts.

Próximo: gizmos/undo de conjunto e Attachments/vínculos físicos; depois inventário,
E/Delete/hotbar e demais requisitos da missão. Não há inventário, RifleDemo ou
multiplayer implícito nesta entrega. Diffs alheios preservados.

<a id="registro-23"></a>

## Registro 23

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/23-gizmos-modelos.md).

### 23 — Gizmos e undo de Modelos / continuidade

Código: `271a5cbf684bc367fc8da4dd807f5a1f846dcddf`. Esta entrega fecha o incremento de edição de Modelos;
a missão integral continua EM DESENVOLVIMENTO. Encerramento solicitado pelo
usuário devido ao orçamento de créditos, sem iniciar novas funcionalidades.

#### Implementado e testado

- Mesmo gizmo das Parts para mover, girar e escalar Modelos; escala uniforme.
- Uma operação undo/redo por gesto, incluindo Parts e pivôs aninhados.
- Edição numérica de transformação/pivô participa do histórico.
- Cancelamento restaura poses e pivôs implícitos antigos. Proxy não cria corpo
  Bullet; corpos filhos são suspensos/liberados durante edição.
- Restauração valida referências antes de alterar peças; histórico limitado.
- Corrigido Ctrl+Z/Y sobre painéis passivos RmlUi, preservando captura em texto,
  diálogos, menus e arrastes. A hipótese inicial de simples foco de texto foi
  refutada pelo segundo teste gráfico; a captura de ponteiro bloqueava atalhos.

Arquivos: ModelTransformSession, EditHistory, TransformSession, EditorUI,
StudioPanels, Scene, EditorActions, StudioFrontend, CMake e testes studio/rml_ui.
Guia MODELOS.md e contrato ATLAS do editor atualizados.

#### Evidências reais

- ATLAS usado para localizar fluxo de input/histórico e consultar impacto.
  Novo arquivo não teve teste direto mapeado; selecionados studio/models_ui
  pelo contrato e dependências reais. Índice atualizado após código.
- FAST 20261026T022329Z-25116: build227,2 s; 17/17 em21,0 s.
- IMPACT 20261026T022915Z-30160: 20/20 em57,8 s; total58,4 s.
- Build final Studio/Player/rml_ui_tests via TURBO:25,84 s.
- FAST final 20261026T023325Z-2137:17/17 em11,9 s, total14,4 s.
- models_ui supervisionado: passou em23,394 s após duas falhas reais de undo
  sobre painel, corrigidas. Log: .turbo/logs/model-gizmos-ui-passive-panels.log.
- Captura inspecionada: /tmp/loblox-rml-ui-9FmKtY/model-properties.png.
  Teste X11 inclui arraste, undo/redo, propriedades, salvar, Play/Stop e desagrupar.
- Cobertura global PARCIAL/DESCONHECIDA no TURBO; nenhum FULL realizado.
  Esses testes não comprovam todos os requisitos pendentes da missão.

#### Já publicado nesta recuperação

4bb803c: referências/nomes de imagens, remoção segura e persistência.
bf90835: frontend de recursos, Surface no renderizador e remoção do rosto
obrigatório, preservando rostos explícitos antigos. Relatório21.
9fe720b: Modelos, pivôs, hierarquia, clonagem, Lua e JSONv4 compatível com
leitura v1/v2/v3; relatório22. Este incremento adiciona gizmos/undo.

AutoBackup ativo, checkpoint remoto4716e90d35bd confirmado ao verificar,
362 arquivos cobertos e nenhuma pendência. Escopo allowlist; dados ignorados
não estão protegidos no GitHub. Destino externo privado continua não informado.
Alterações preexistentes de ferramentas TURBO, locales e demais tarefas foram
preservadas, sem inclusão nos commits deste incremento.

#### Próxima ação exata

1. Consultar ATLAS para Attachments e vínculos/juntas usando hierarquia e
   animação existentes; implementar transformação local/global e persistência.
2. Inventário real, interação segurando E, soltar Delete, hotbar dinâmica.
3. Completar gerenciamento/streaming/prévia de áudio sobre base2D/Lua existente;
   UI pública Lua; integração NPCs/combate e RifleDemo pelas APIs reais.
4. HUD circular com saúde real; menu lateral animado preservando configurações.
5. Integração abrangente Studio/Player, projetos antigos, Mundo Padrão,
   salvar/reabrir/exportar/importar e testes finais apropriados.

Limites: agrupar/desagrupar ainda não tem undo estrutural; Modelos não criam
soldas físicas. Não há inventário/hotbar/RifleDemo/UI Lua declarados prontos.
Não reconstruir imagens, runtime Lua, NPCs/A* ou áudio já implementados.

<a id="registro-24"></a>

## Registro 24

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/24-attachments-alpha.md).

### 24 — Attachments / preparação Alpha 0.1.0

Código: e9d257e4c2e5deb69cbc94145c927c858b831514. IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER.
Ainda não existe Release Alpha validada: não foi criado tag nem pacote novo.

- Componente real filho de Part/Modelo, IDs, nomes, posição/rotação locais,
  pose global e direção, criação/acesso Lua, clone, remoção e Propriedades.
- Modelos girados/escalados mantêm BocaDoCano correta. Reparentar preserva
  âncoras; agrupamento/desagrupamento falho restaura nós/poses/ordem.
- JSONv5 com quaternions; v1–v4 continuam legíveis. Pacote .lo testado em outro
  HOME e Mundo Padrão preservam Attachment e vínculo; salvamento inválido não
  substitui documento. Exportação Lua declarativa legada continua só v1 e
  recusa hierarquia explicitamente; utilizar JSON/.lo para este recurso.
- Exemplo executável assets/examples/lua/attachments.lua e guia ATTACHMENTS.md.

TURBO: build inicial552,9 s, seguido de incremental focado24,51 s.
FAST inicial16/17: exemplo usava pcall, indisponível no sandbox; casos inválidos
passaram para validação separada no Player, sem liberar pcall.
FAST final20261026T142143Z-19162:17/17,22,1 s (22,7 s total).
IMPACT20261026T142246Z-20080:17/17,10,1 s, cobertura global parcial.
models_ui supervisionado47,226 s, timeout60 s/65 s supervisor, aprovado.
Captura /tmp/loblox-rml-ui-AQdAfK/attachment-properties.png inspecionada.
Nenhum FULL; não extrapolar estes resultados para os recursos pendentes.

AutoBackup ativo, último checkpoint remoto e14ff905d7d6 confirmado no início.
Diffs preexistentes preservados; snapshot inicial em .turbo/alpha-recovery.

Próxima unidade: vínculos físicos/juntas entre Parts/Attachments existentes,
com criação que preserve poses, clone com remapeamento de IDs e liberação
segura no Bullet. Depois inventário/E/Delete/hotbar; áudio/UI Lua; combate e
RifleDemo; HUD/menu; Demo integrada; distribuição, segurança/licenças e Release.
Vídeo MP4 não pode ser distribuído sem comprovar autorização; fallback deve ser
usado no pacote público se a licença continuar desconhecida. Não houve auditoria
final de licenças nem instalação limpa nesta unidade.

<a id="registro-25"></a>

## Registro 25

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/25-juntas-alpha.md).

### 25 — Juntas fixas / Alpha em desenvolvimento

Código: e6f65914de0664343d004fe6c4e89e2853bfaae1. IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER.
Pares de Attachments de Parts distintas alimentam btFixedConstraint. Conectar
preserva poses; alterar quadro local anima a peça dinâmica; remover qualquer
ponta libera a constraint antes do corpo. Peças conectadas não colidem entre si.
Clone remapeia IDs internos, desconecta referências externas na cópia; JSONv6
persiste/valida os vínculos sem alterar leitura v1–v5. Sem corpos duplicados.
Propriedades RmlUi/ImGui permitem escolher destino por nome. API pública Lua:
connect_to, disconnect e joint; exemplo juntas.lua executado no Player real.
Arquivos: Instance/Scene, PhysicsWorld, ProjectIO, LuaRuntime, StudioPanels,
EditorActions/StudioFrontend, testes studio/experiences/lua_player/rml_ui.

Validação ATLAS (impacto/contratos) e TURBO:
- Build focado inicial falhou por cast em trecho de adoção; guardas movidas
  para canParent. Build corrigido58,91 s; Studio/Player/UI/pacotes80,74 s.
- Native inicialmente detectou desvio físico0,14764 sob gravidade: 16 iterações
  eram insuficientes para a alavanca. Override local64 corrigiu, sem relaxar
  tolerância e sem elevar solver global. Regressão focada1,719 s aprovada.
- FAST20261026T143742Z-27681: build151,4 s,17/17 em12,6 s (total164,7 s).
- IMPACT20261026T144210Z-31357:17/17 em10,4 s; global parcial.
- models_ui supervisionado45,444 s aprovado: seletor de junta, âncoras,
  salvar/reabrir/Play/Stop/desagrupar. Captura inspecionada em
  /tmp/loblox-rml-ui-Asba8a/attachment-properties.png.
- Pacote .lo/defaults com vínculo e Player em outro HOME validados no teste
  experiences; erros de ID preservam o documento anterior. Nenhum FULL.

Limites explícitos: juntas fixas apenas; sem motor livre/dobradiça com limites,
sem novo rig arbitrário. Destino dinâmico necessário para animação física.
Benchmark de grandes montagens pendente. Inventário/interação/hotbar não foram
implementados por este incremento. AutoBackup ativo, push95d1977da501 confirmado.

Retomada: inventário real sobre Modelos e IDs, estados world/stored/equipped,
controle Lua e preservação de scripts/propriedades; depois E/Delete/hotbar.
ATLAS não localizou inventário de jogo: hits inventory.py são inventário de
TESTES do TURBO, não reutilizar como sistema de itens. Runtime/input e UI
compartilhada Studio/Player precisam integrar os novos estados. Continuam
pendentes áudio completo, UI Lua, combate/RifleDemo, HUD/menu, Demo, pacote,
instalação limpa, auditoria de segurança/licenças, tag e Release.

<a id="registro-26"></a>

## Registro 26

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/26-inventario-alpha.md).

### 26 — Inventário de Modelos / preparação Alpha

Código e4863f372e8225aedd49b1f211964bba5e748039: IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER.
Protótipos nas Propriedades (tipo e ícone), pré-carregamento antes da VM, clone
com scripts ativos, instâncias world/stored/equipped, bolsa, equipar/desequipar,
descarte com busca de chão/espaço Bullet, remoção e consultas. API pública
linblux.items e inventory_changed; limites e exemplo real em INVENTARIO.md e
assets/examples/lua/inventario.lua. Identidade/atributos/scripts permanecem no
mesmo Modelo ao coletar/soltar/recolher. Cena autoral intacta após Play/Stop.

InventoryHidden centraliza ocultação de peças/decals, queries e contatos.
item_icon participa da consulta central de referências de imagens. Configuração
visual acompanha JSON/.lo/Mundo Padrão; bolsa/equipamento são estado da sessão.
Clone autoral deriva novo ID de tipo; instância não vira outro protótipo.

Arquivos: Inventory, Runtime, LuaRuntime, ImageAssets, PhysicsWorld,
SceneRenderer, Scene, StudioPanels/StudioFrontend/EditorActions, CMake e testes
studio/experiences/lua_player/decal_render/rml_ui. Contrato ATLAS game atualizado.

Evidências ATLAS/TURBO:
- build focado135,45 s, depois23,16 s; integração da autoria174,65 s;
  correção final89,61 s. FAST final20261026T153147Z-24075: build48 s,
  15/15 em15,5 s (total64,5 s), cobertura desconhecida pela seleção/CMake novo.
- IMPACT20261026T153342Z-25703:20/20 em64,5 s (total65,2 s), global parcial.
- Integração supervisionada experiences/models_ui/decal_render:3/3 em57,581 s,
  .turbo/logs/inventory-final-integration.log. Captura inspecionada em
  /tmp/loblox-rml-ui-WP1HT0/model-properties.png.
- Native InventoryLifecycle: duplicação de coleta rejeitada, escala preservada,
  descarte usando bounds reais (inclui esfera não uniforme), referências
  destruídas, scripts clonados e autoria intacta. Player executa inventario.lua.
- GUI cria protótipo, muda tipo, salva e usa spawn/collect/equip durante Play.
  Pacote .lo/defaults preserva tipo/ícone; pixels verificam protótipo e item
  guardado ocultos inclusive com decals, e instância world visível.
- Primeira GUI falhou: checkbox retinha foco e bloqueava atualização das
  Propriedades. Corrigido sem afrouxar o teste; novos campos aparecem ao marcar.
  Nenhum FULL nem pacote de distribuição/instalação limpa desta versão.

Limites Alpha explícitos: estrutura capturada ao instanciar; não adicionar ou
remover Parts enquanto guardado/equipado. Sem transferência multiplayer ou
salvamento automático da bolsa entre partidas; criador usa storage para dados
persistentes. Protótipos não aceitam NPC nem juntas externas. Fluxo do seletor
de ícone compartilha ImagePicker existente; o teste GUI desta unidade usa nome
sem escolher ícone. Exportação/importação inclui ícone configurado no teste.

PENDENTE imediato: interação universal por proximidade/segurar E, Delete e
hotbar dinâmica (a API de inventário não simula esses controles). Depois áudio
completo, UI Lua, combate/RifleDemo, HUD circular, menu lateral, Demo integrada,
regressões finais, pacote Linux, licenças/segurança, instalação limpa e Release.
Não criar tag/Release enquanto os critérios essenciais estiverem pendentes.

<a id="registro-27"></a>

## Registro 27

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/27-controles-itens-alpha.md).

### 27 — Interação, E, Delete e hotbar

Código 1258b0a5943d6e39e94faf98718d984f2a6f11e3: IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER.
InteractionSystem genérico (Parts/Modelos), alcance pela geometria, linha de
visão, tecla configurável, hold/cancelamento/latch e evento Lua
interaction_completed. Itens world usam o mesmo componente, sem lógica de
arma fixa. Delete solta o equipado;1–9 selecionam, PgUp/PgDn paginam, bumpers
percorrem slots. Hotbar dinâmica/progresso/erro real no Studio e Player pelo
ExperienceOverlay compartilhado. Sem itens equipáveis, nenhum slot é desenhado.

ATLAS localizou runtime/input/render e contratos; TURBO:
- build focado59,85 s; native studio/Player2/2 em3,468 s;
- FAST20261026T160530Z-8930: build128,9 s;15/15 em12,6 s; total142,1 s;
- IMPACT20261026T161250Z-12778:20/20 em54,2 s, total56,9 s;
- item_controls_ui supervisionado52,749 s aprovado. Capturas reais mostraram
  texto pequeno/serrilhado; um ajuste para fonte vetorial Linux, tamanho mínimo
  e liberação de fonte foi feito. Build28,55 s e incremental3,92 s.
- GUI final31,350 s aprovada em .turbo/logs/item-controls-ui-font.log;
  captura /tmp/loblox-rml-ui-sREPjY/hotbar-720.png inspecionada, além da captura
  1080p anterior. Fluxo X11:hold parcial/release/afastamento/E/coleta/1/Delete/
  recoleta com carga37 preservada/página2/remoção/Stop. Sem FULL.
- Cobertura global parcial/desconhecida; gamepad validado por snapshots,
  não por dispositivo físico. Saúde/menu antigos ainda existem e são pendentes.

Arquivos: Interaction, Inventory.worldItems, Runtime, LuaRuntime,
ExperienceOverlay/SceneRenderer, Player, dois hunks de GamePanels, CMake e
studio/lua_player/rml_ui. Diffs preexistentes de GamePanels foram preservados.
Guia INVENTARIO.md#coleta-interação-e-hotbar e exemplo interacao_porta.lua usam a API pública real.

Próxima fase: áudio. Auditoria encontrou ProbeAudio usando LoadWave para a
música inteira só para duração, e Player abrindo dispositivo depois dos scripts.
Corrigir duração com metadados limitados/streaming e ordem de inicialização;
validar memória/reprodução, completar prévia/gerenciamento sem reescrever backend.
Depois UI Lua, RifleDemo/combate, HUD circular/menu lateral, Demo e distribuição.
Não existe pacote/tag/Release Alpha validado nesta entrega.

<a id="registro-28"></a>

## Registro 28

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/28-audio-completo.md).

### Áudio MP3/WAV/OGG — metadados, vozes com dono e exclusão segura

Base remota: `e719270bb2c3017da2085d21b7138b11ce6c1023`. Diffs locais alheios
(áudio, inventário, Player, TURBO) foram preservados; este incremento publica
apenas a unidade de áudio já implementada e agora validada.

#### Problemas reais corrigidos

- `ProbeAudio` decodificava o arquivo inteiro com `LoadWave` só para descobrir a
  duração. Uma música longa custava RAM/tempo na importação e no primeiro
  `play`. Agora a leitura usa metadados do container (FFmpeg/libavformat) com
  orçamento de 2 s, `protocol_whitelist=file`, `format_whitelist=wav,ogg,mp3`,
  `probesize`/`analyzeduration` limitados e cache por tamanho+mtime. Sem
  FFmpeg, a raylib só inspeciona com dispositivo aberto — e isso é declarado no
  erro, não mascarado.
- O `Player` chamava `InitAudioDevice` **depois** de iniciar os scripts; um
  `linblux.audio.play` no `EngineStart` caía no modo sem dispositivo. O
  dispositivo agora é aberto antes da sessão, e fechado por guarda RAII também
  nos caminhos de erro.
- Vozes não tinham dono: remover a peça deixava o som tocando. `play` aceita
  `owner`; `AudioSystem::update(dt, scene)` libera a voz quando o objeto some.
  O `RuntimeSession` também atualiza vozes durante a pausa, então pausar não
  congela nem vaza áudio.
- O contrato Lua não expunha duração da biblioteca, `device_ready` nem se a voz
  é streaming. Foram adicionados `list()[i].duration`, `audio.device_ready()` e
  `audio.is_streaming(handle)`, além do limite de 16 operações de áudio por
  quadro.
- Exclusão de áudio não era transacional. `DescribeSoundUsage` lista
  objeto/atributo/script que cita arquivo, `id` ou nome; `RemoveSoundAsset`
  recusa uso, valida caminho/symlink, move para backup temporário, salva e
  restaura metadados e arquivo se o salvamento falhar.
- `GameAssets` valida nome (1–128 bytes) e duração finita/não negativa, e a
  duração é persistida em `assets.json`. A UI de Sons locais ganhou prévia
  (ouvir/pausar/parar), volume, loop, renomear e excluir com dependências.

#### Evidência

- Build incremental: `cmake --build build-linblux -j2` sem erros; define
  `LINBLUX_AUDIO_METADATA_FFMPEG` ativa (flags.make).
- `build-linblux/linblux_audio_tests`: `Probe PCM1h: duração 3600.00s, aumento
  de pico RSS 6292 KiB` — arquivo PCM esparso de 1 h não é decodificado na RAM.
- TURBO supervisionado `.turbo/logs/audio-finish`: `linblux_audio`,
  `linblux_studio`, `linblux_lua_player_validation` e `linblux_experiences`
  aprovados (4/4), cobertura parcial.
- `linblux_audio` cobre MP3/WAV/OGG reais, volume/pause/loop, 64 vozes,
  voz com dono removida junto do objeto, biblioteca persistida/reaberta,
  duração salva, recusa de exclusão em uso, reversão em falha de salvamento e
  remoção autorizada com arquivo + metadados apagados.

#### Regressões de integração corrigidas nesta etapa

- `linblux_editor_restart`: a comparação aceita apenas o arredondamento float32
  da rotação persistida (15° → 14,999998) e o bloco `body` padrão que o editor
  grava, sem tolerar eixo trocado ou rotação ausente.
- `linblux_engine_default_restart`: cartão do mundo padrão localizado pela cor
  da marca e Play localizado pelo id `play` no diagnóstico de geometria
  (`LOBLOX_TRACE_UI`), reemitido após cada redimensionamento.
- `linblux_experience_startup`: `faceImage` vazio é válido desde a remoção do
  rosto obrigatório de fábrica.
- `linblux_rml_ui`: a página de recursos usa o escopo `assets`, então o botão de
  importação é `assets-import-dialog-`; o teste também confirma que a seção de
  sons locais é construída mesmo sem dispositivo de áudio.
- Execução integral supervisionada de `linblux_rml_ui` em 1920x1080: 1051
  verificações, 0 falhas.

#### Limitações

- Sem áudio 3D/atenuação por distância e sem mixer por categoria.
- A prévia do Studio e a reprodução do Player continuam 2D globais.
- Sem FFmpeg instalado, importação headless falha explicitamente; a gravação
  nunca ocorre sem metadados válidos.
- Streaming é o `Music` da raylib (arquivo aberto no `play`), não decodificação
  parcial própria.
- Pacote/instalação limpa, HUD, menu lateral, UI Lua, RifleDemo e Demo integrada
  continuam pendentes do Alpha.

<a id="registro-29"></a>

## Registro 29

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/29-ui-lua.md).

### UI por Lua — modelo declarativo no Studio e no Player

Base remota: `2f7cd5dc30c712966e5972ad705a8bcaae96f8e2`. Áudio e as regressões de
reinício já estavam publicados e não foram refeitos.

#### O que foi implementado

- `game::UiModel` (`src/game/Ui.h/.cpp`): árvore limitada de `panel`, `label`,
  `button`, `image` e `bar`, com posição, tamanho, âncoras, cor, fundo,
  transparência, visibilidade, `z`, texto, imagem, `value`/`max` e `font_size`.
  Layout em tela virtual 1280×720 escalada por `min(w/1280, h/720)`, filhos
  resolvidos dentro do retângulo do pai.
- Dono por script, ids monotônicos e validação de todas as escritas. Limites:
  128 elementos, profundidade 4, 64 mutações por quadro, 64 eventos, texto de
  512 bytes, imagem precisa existir nos `assets/` da experiência.
- `linblux.ui` em `LuaRuntime`: `create`, `set`, `get`, `destroy`, `on`
  (`click`/`hover`), `off`, `count`. Callbacks rodam como tarefas Lua, com as
  mesmas cotas; `uiClick`/`uiHover` chegam pela fila do runtime, nunca durante
  o render.
- Limpeza: destruir o pai destrói filhos; `cancelScript` desabilita o script,
  remove elementos e handlers; `RuntimeSession::stop` limpa o modelo.
- `RuntimeSession` drena cliques/hover no `update`; o host só informa o ponto
  no viewport.
- Renderização: `loblox::DrawLuaUi` (`src/rendering/LuaUiRenderer.h/.cpp`) desenha
  com raylib na textura do Play do Studio e na tela do Player — um layout, dois
  hosts. No Studio, cliques são ignorados enquanto o botão direito captura a
  câmera; no Player, com o cursor escondido.

#### Exemplos publicados (somente API pública)

- `assets/examples/lua/ui_municao.lua`: contador de munição com barra, tecla F,
  recarga por R e botão.
- `assets/examples/lua/ui_notificacao.lua`: notificação com aceitar/dispensar,
  hover, recriação e `task.wait`.
- `assets/examples/lua/ui_hud_vida.lua`: HUD de vida ligado a `health_changed`,
  estados de dano e morte.

#### Evidência

- `linblux_lua_interface` (headless): modelo, `get/set`, âncoras (centro e
  bottom-right em 1280×720), escala em 640×360, clique entregue ao callback,
  hover enter/leave, limpeza ao desabilitar o script e ao parar, erros de handle
  inválido, tipo, texto >512, imagem ausente, profundidade, orçamento de
  mutações, evento inválido e escrita entre scripts; e os três exemplos
  (munição decrementa com F e recarrega pelo botão; notificação aceita; HUD
  acompanha dano até 20/100 e “Estado crítico”). FAST 15/15.
- `linblux_lua_ui_render` (gráfico): painel ancorado desenhado e clicado em
  960×600 e 1280×720, verificado por pixels do framebuffer.
- `linblux_lua_ui_player` (gráfico, executável real): `ui_hud_vida.lua` valida
  com `--validate` e é pintado na janela do Player (4276 pixels da barra).
- `linblux_visual_studio` (gráfico, Studio real): durante o Play, o painel Lua é
  encontrado no framebuffer; 213 verificações, 0 falhas.

#### Limitações

- Sem imagens animadas, fontes por elemento, máscaras, gradientes, listas,
  scroll, drag ou campos de texto. Novos tipos exigem C++.
- A UI não é serializada no lugar: o script é a fonte da verdade e recria os
  elementos em cada Play.
- Não substitui o HUD protegido nem o RmlUi do Studio; é uma camada adicional
  dentro do viewport.
- O mouse do Studio só entrega cliques à UI quando a câmera não está capturada.
- Próximos passos Alpha: combate genérico/RifleDemo, HUD circular, menu lateral,
  Demo integrada, pacote Linux e revisão final.

<a id="registro-32"></a>

## Registro 32

[Documento original](https://github.com/danielvtroot/LINBLUX/blob/6377b543f9833c72070a5906210bcf9a68a51768/docs/relatorios/32-rifledemo-recuperado.md).

### RifleDemo recuperado — 26/10/2026

Base:4a0be641eefc0103136f46f81f782a5270e5f1ea. WIP preservado e concluído,
sem recriar inventário, áudio, UI ou navegação. ItemActions controla estado
limitado por ID (intervalo, munição, recarga); efeitos/dano/recoil/HUD são Lua.
RifleDemo contém15 objetos autorais,5 sons originais,3 alvos com rig e saúde,
guia em patrulha e decal oficial. Instâncias/E/Delete/slots preservam identidade.

Correções reais: busca em descendants para alcançar BocaDoCano dentro de Cano;
peça do traçante aumentada para respeitar mínimo0,2; raycast anterior aos efeitos
e filtro Lua limitado dos próprios efeitos; som por ID estável, evitando quebra
de paths após relocação/hash na importação; efeitos pertencem ao sistema do
mundo, com descarte mesmo após destruir o item. Inteiros Lua acima do limite
são recusados antes de conversão32bits, com preservação do estado.

Regressão NPC: Runtime anexava NPCs autorados após inicializar a VM e apagava
rotas/vida já configuradas pelos Scripts. attachAuthored agora preserva o
controlador existente. Teste verifica deslocamento real e clipe walk do guia.

Código principal: game/ItemActions, Runtime, Npc, LuaRuntime, exemplos/rifledemo,
tools/examples/build_rifledemo.py, tests/item_actions_main.cpp,
tests/rifledemo_main.cpp e tests/rifledemo_player.py, CMake. API em INVENTARIO.md.

Build Studio/Player e testes aprovado (último lote36,52 s, correçãoNPC15,69 s).
FAST20261026T231845Z-823:17/17 em6,4 s, global desconhecido; IMPACT
20261026T231931Z-1300:23/23 em50,1 s, global parcial. Não são aprovação FULL.
Integração nativa final3/3 em3,22 s. Inventário no Studio39,79 s aprovado.
Player real com E/1/tiro/dano/R/Delete/recoleta e guia animado17,583 s aprovado;
marcador de sucesso só é desenhado após assertivas Lua do fluxo completo.
Reabertura/exportação/importação .lo verificam scripts, sons/protótipo,
saúde/NPCs e imutabilidade da autoria. Captura do Player inspecionada.
Teste final de limpeza de efeitos após descarte foi ampliado ao fechar a unidade;
resultado final registrado no acompanhamento. Sem dispositivo de áudio físico
nesta execução, não se declara reprodução audível aprovada.

Próximo: pacote fora do checkout, migração das expectativas gráficas históricas,
regressão abrangente e revisão de licenças. Não há tag ou Release pública.

Fechamento: rifledemo + rifledemo_player2/2 aprovados em18,310s, incluindo
remoção do item com efeitos ativos, limpeza, guia/walk e roundtrip .lo.
