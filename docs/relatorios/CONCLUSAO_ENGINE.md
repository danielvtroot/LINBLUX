# Conclusão da engine LINBLUX — acompanhamento verificável

Missão integral **não concluída**. Estados: PENDENTE, EM DESENVOLVIMENTO,
IMPLEMENTADO, TESTADO, VALIDADO NO PLAYER, PUBLICADO, BLOQUEADO. Uma publicação
de incremento não encerra sua fase quando ainda faltam requisitos obrigatórios.

## Ponto de partida histórico desta missão

HEAD local/remoto main: cf53b096be2c00693b56dbf7775ef93828b53537.
Scripts clonados/reativação, destruição, contatos Bullet, passo fixo e entrada
Lua já estão publicados. Resultados do relatório 12 são históricos e não
substituem os testes futuros. Alterações locais de outras tarefas preservadas.

## Fases

| Fase | Estado | Implementação/evidência atual | Próximo requisito obrigatório |
| --- | --- | --- | --- |
| 0 AutoBackup | PUBLICADO | cd4fb406cfccc6b030e56eadbede0c3965ef66f3, push main confirmado. Monitor labubu ativo via cron, @reboot/watchdog, checkpoints e push automático isolados. Clone remoto recuperou 315 arquivos; índice/main intactos; reinício em 20,005 s. Offline 20/20 e IMPACT 17/17, cobertura global parcial. Relatório 13. | Destino externo privado não informado (cópia externa não configurada). Backend systemd não executável neste host. |
| 1 Lua/scripting | EM DESENVOLVIMENTO | Runtime, eventos, física, entrada, câmera e persistência existentes; 9fe720b amplia Modelos/hierarquia/transformações e scripts. | Componentes restantes, UI programável e integração final de todas as APIs; não declarar Lua completo. |
| 2 NPCs/navegação | EM DESENVOLVIMENTO | 5f29424: A* XZ, patrulha/perseguição, vida/dano/morte/respawn; 93fe7e5: rigs e clipes idle/walk/run/attack/hurt/death integrados ao renderer/Player. | Combate com armas/projéteis e conjunto completo de demonstrações. Não refazer navegação ou animações existentes. |
| 3 Áudio | IMPLEMENTADO/TESTADO | Relatório 28: metadados FFmpeg com timeout (PCM 1 h, RSS +6,2 MiB), voz com `owner`, `device_ready`/`is_streaming`, duração persistida/Lua, prévia e exclusão transacional no Studio, Player abre dispositivo antes dos scripts. | Sem áudio 3D/mixer; validar novamente na integração abrangente. |
| 4 Imagens/decals | EM DESENVOLVIMENTO | 4bb803c/bf90835: consulta central de usos, nomes amigáveis, resolução de vínculos, exclusão segura, seletor, Surface persistida/renderizada e testes gráficos. Relatório 21. | Conferência final de todos os requisitos de imagens/Lua na integração abrangente; corrigir apenas defeitos novos reproduzidos. |
| 5 Rosto de fábrica | PUBLICADO | 00898e2/bf90835: removida associação de fábrica e fallback visual de face/olhos; rig preservado e rosto legado explícito continua renderizado. Pixels e interação gráfica validados, relatório 21. | Não apagar imagens ou associações personalizadas antigas. |
| 6 UI Lua | IMPLEMENTADO/TESTADO | Relatório 29: `linblux.ui` com panel/label/button/image/bar, âncoras 1280×720, eventos click/hover, limites, dono por script e limpeza; desenhado no Play do Studio e no Player pelo mesmo renderizador. `linblux_lua_interface` (FAST), `linblux_lua_ui_render`, `linblux_lua_ui_player` e `linblux_visual_studio` aprovados. | Sem imagens animadas, fontes por elemento, listas/scroll/campos; UI não é serializada (o script recria). |
| 7 Vida circular | PENDENTE | Saúde real já existe no runtime. | Indicador circular vermelho, animação, dano/cura/respawn, personalização Lua. |
| 8 Menu lateral | PENDENTE | Studio e Player têm apresentações distintas existentes. | Painel lateral compartilhado/coerente, funções preservadas, foco/teclado/controle e testes visuais. |
| 9 Integração | PENDENTE | Resultados anteriores não cobrem recursos futuros. | Reabertura/exportação/importação, compatibilidade, Player, gráficos, IMPACT/FULL justificado. |

## Fase 1 — recursos separados (a fase inteira continua em desenvolvimento)

| Recurso | Estado | Código/teste/evidência | Limitações e trabalho restante |
| --- | --- | --- | --- |
| Cena/peças/hierarquia/IDs | PUBLICADO | Base cf53b09, LuaRuntime.cpp + Runtime; LinbluxScripts/studio. | scene.create aceita part/group/model; componentes genéricos e transformações locais/globais completas ainda faltam. Não anunciar APIs inexistentes. |
| Scripts por objeto/globais/clones | PUBLICADO | Base cf53b09; LuaRuntime + ProjectIO; DynamicScriptLifecycle e LinbluxExamples. | Scripts de runtime não gravam automaticamente na autoria; manter essa separação. |
| Forças/impulsos/gravid./raycast/contatos | PUBLICADO | Base cf53b09; Runtime/PhysicsWorld/LuaRuntime; LinbluxPhysics/FixedContacts. | nearby usa centros; consultas de volumes, propriedades completas e regiões contínuas não estão prontas. |
| Jogador/vida/spawn/animações | PUBLICADO | Base cf53b09; LinbluxPlayer + game/spawn. | Um controlador nativo, rig compatível legado; NPCs e rigs arbitrários são pendências próprias. |
| Câmera | PUBLICADO | a4398eed1ef1fbb9fcb8c257166e8327729e6e12, hash remoto confirmado. Relatório 15: FOV persistente, set_pose por alvo/up ou Euler, quaternion, release. FAST 13/13, IMPACT 13/13 + gráfico pertinente/studio/Player 3/3. | Pose transitória reconstruída por Script; sem desvio de obstáculos. |
| Transformações e componentes de peças | PUBLICADO | 2f9632a85fe9baaac953e2c37613158141186c6c, remoto confirmado. Relatório 16: conversões/escala/interpolação, Scripts reais, validação atômica e raycast Bullet; FAST/IMPACT 13/13. | Referência rígida explícita; grupos não herdam transformação. Decals editáveis pendentes na fase de imagens. |
| Entrada/eventos | PUBLICADO | cf53b09; ScriptInput, Runtime, LuaRuntime; script_input_ui real. | Controle físico não testado; amostras de controle injetadas. Não expor Escape/atalhos de saída aos scripts. |
| Tempo/cancelamento/intervalos | PUBLICADO | 26066dff73dc128c50987de2f5f25dfcf71ff89b, push main confirmado; validado no Player. Relatório 14: LuaRuntime; temporizadores.lua, LinbluxTaskCancellation/LinbluxExamples. FAST 13/13, IMPACT 18/18 selecionados. | every não recupera pulsos atrasados; um coroutine por intervalo. |
| Diagnóstico no Player | PUBLICADO | 26066dff73dc128c50987de2f5f25dfcf71ff89b, push main confirmado. Relatório 14: Player --validate + contador LuaRuntime; lua_player_validation.py, quatro cenas reais. | Validação simula 2 s e Stop, não prova todas as ações futuras. Recusa erros evictos do buffer. |
| Segurança/cotas/erros | PUBLICADO | Base cf53b09; limites host-only, finalizadores restritos, VM sem APIs de SO; testes studio. | É limite de confiabilidade em processo, não isolamento de SO; trabalho nativo também exige limites. |
| Persistência de scripts/autoria | PUBLICADO | ProjectIO + LinbluxExamples: salvar/reabrir e executar. | Persistência de estado de gameplay/configurações via API controlada ainda PENDENTE. |
| Estado privado por experiência | PUBLICADO | e44b4e8f07c803fe3bea4198dd234910c42808fe, remoto confirmado. Relatório 17: ProgressStore, storage Lua, Studio/Player; FAST 14/14, IMPACT 18/18 e integração gráfica pertinente 4/4. | Não exportado, limites explícitos, sem backup externo configurado. Publicado; recuperação privada externa continua pendente. |
| Volumes Bullet e controle Lua do jogador | PUBLICADO | 431efefd30b1cb03d0d288da75a62608e9fe35dd, remoto confirmado. Relatório 18: overlap_box/overlap_sphere reais, filtros, cotas, intenção de movimento, salto, velocidade, força/impulso. FAST/IMPACT 13/13. | script_input_ui 1/1 em 20,792 s; publicado. Regiões contínuas e NPCs pendentes. |
| Áudio por Lua | IMPLEMENTADO/TESTADO | Relatório 28: `play` (volume/loop/owner), pause/resume/stop/stop_all, volume/loop, duration, device_ready, is_streaming, list; limite de 16 operações/quadro; `linblux_lua_player_validation` aprovado. | Sem 3D/atenuação nem mixer por categoria. |
| UI por Lua | PENDENTE | Não há namespace documentado como implementado. | Dependência da Fase 6: modelo/render/input e UI protegida. |

Não avançar para NPCs como se a Fase 1 estivesse completa. Resolver dependências
de áudio/UI explicitamente, mantendo os estados parciais visíveis. Todo método
novo precisa de implementação nativa, teste Lua e execução no Player correspondente.

## Regras de continuidade

1. Proteção de código da Fase 0 testada; manter o monitor ativo e completar
   separadamente o destino privado externo quando informado.
2. Usar ATLAS para localizar e TURBO para build/testes; guardar logs em .turbo.
3. Publicar somente arquivos revisados da unidade funcional; preservar diffs alheios.
4. Registrar testes com execução, tempo, resultado, limitações e hashes remotos.
5. Dados privados/ignorados não se tornam protegidos por haver push de código.

## Evidências da Fase 0

- [Relatório 13](13-autobackup-fase0.md): arquivos, tempos, testes e limitações.
- Checkpoint recuperado: 36917d6c4474fb1358924785ff2b23b21881b27f (auto-backup).
- Marcador público: docs/backup/recovery-canary.txt; SHA-256 registrado no relatório.
- Estado operacional: usuário labubu, ~/.local/state/linblux-backup; backend cron.
- Logs: .turbo/logs/phase0-remote-recovery.log, phase0-backup-final.log e rodadas
  FAST 20261025T151827Z-14019 / IMPACT 20261025T152516Z-20625.
- Cobertura global parcial: testes gráficos omitidos nesta alteração de ferramenta.
- PENDENTE: cópia privada externa e prova de recuperação nesse destino; solicitado
  ao proprietário, sem resposta até este checkpoint. Não há proteção física dos
  dados privados pelo simples fato de o monitor de código estar ativo.

## Retomada após este incremento

Checkpoint de código publicado: 26066dff73dc128c50987de2f5f25dfcf71ff89b.
Compilação Studio/Player/teste aprovada; 1655 verificações de Studio, zero falhas.
FAST 13/13 e IMPACT 18/18 selecionados passaram; cobertura global parcial, sem
FULL ou nova validação gráfica neste incremento de scheduler/CLI. Nenhum build
ou teste ficou ativo; manter apenas o serviço de backup solicitado.

Não há trabalho de NPC/áudio/decals/HUD/menu escondido como pronto. A próxima ação
concreta agora é ampliar propriedades de corpos rígidos e preparar NPCs com a física existente.
Câmera, transformações de peças e persistência privada estão implementadas/
validadas conforme relatórios 15–17. Consultar ATLAS para PhysicsWorld,
RuntimeSession::partMotion/PlayerInput e LuaRuntime. Não duplicar o Bullet nem
mudar a hierarquia/serialização sem migração.

Transformações/componentes e persistência controlada já estão publicados.
Armazenamento de progresso deve usar escopo definido pelo host, diretório privado
fora de assets exportados, validação de caminhos e limites; não expor io/os/load.
Áudio/UI continuam dependências reais a implementar, não métodos a documentar antes
de existir backend. As fases 2–9 permanecem pendentes conforme a tabela.

O monitor de backup deve permanecer ativo como labubu. Os diffs em .opencode,
AGENTS.md, locales, EditorUI/GamePanels, engine_default_restart e ferramentas
TURBO preexistentes foram preservados; não adicioná-los em lote à main. Apenas
as duas linhas de mapeamento de backup do TURBO entraram no commit da Fase 0.

## Checkpoint solicitado por limite de créditos — 25/10/2026

Parada solicitada pelo usuário; missão integral NÃO concluída. Último código
publicado antes desta entrega: 431efefd30b1cb03d0d288da75a62608e9fe35dd.
Câmera a4398ee, transformações/componentes 2f9632a, persistência privada e44b4e8
e consultas/movimento 431efef estão publicados e confirmados no remoto.

Propriedades físicas persistentes PUBLICADAS: 4ce62e2b980bbb92ee7d9174ff56ec57a7bcf5f5;
push normal e hash remoto confirmados. IMPLEMENTADAS, TESTADAS e VALIDADAS NO PLAYER. Relatório 19 descreve arquivos,
FAST 13/13, IMPACT 18/18 e integração final gráfica/pacote 2/2. Nenhum FULL.
O erro inicial do teste gráfico foi corrigido; resultado final aprovado.

Próximo passo exato: consultar ATLAS para Runtime/PhysicsWorld/Rig/SceneRenderer,
definir navegação limitada e entidades NPC sem duplicar os sistemas existentes.
A investigação inicial não encontrou pathfinding implementado; nenhum código
de navegação foi escrito. Persistência de estado/transforms e propriedades
físicas não precisam ser reconstruídas. Áudio/UI permanecem dependências reais
da API Lua; fases 2–9 continuam pendentes conforme a tabela.

Pendentes: NPCs/monstros/combate/navegação e oito demos; áudio MP3/WAV/OGG com
Editor/Player/Lua/streaming/persistência; completar imagens/decals e operações
seguras; remover só o rosto obrigatório de fábrica; UI Lua; vida circular
vermelha; menu lateral animado preservando controles; integração abrangente,
projetos antigos, Mundo Padrão, exportação/reabertura e documentação final.
Backup externo de dados privados ainda sem destino. Preservar todos os diffs
preexistentes, especialmente tools/turbo/turbo_lib/runner.py e selection.py.

Checkpoint final de código: 4ce62e2b980bbb92ee7d9174ff56ec57a7bcf5f5. AutoBackup ativo,
push/checkpoint de backup 7d6d02774dd9 confirmado, sem alterações cobertas
pendentes. Nenhuma compilação ou bateria de testes ativa ao encerrar.
Parada por solicitação explícita do usuário, para economizar créditos.

## Checkpoint NPCs e navegação — 25/10/2026

Incrementos 1–4 da missão de NPCs publicados como uma unidade funcional em
`5f29424d29f20ddecec53fe6557db9ce9f2b324e` (código) e
`1fad7e30786040412d5c56909e606f03c01df901` (exemplos, testes e docs), com push
normal e hash remoto confirmado em `origin/main` (base anterior `48dba40`).
Relatório 20 descreve a
entrega completa: API `linblux.npc`, `game::NpcSystem`, `game::NavGrid` (grade XZ
+ A*), trava de rotação/degrau no `PhysicsWorld`, persistência no campo `npc` do
objeto JSON, três exemplos Lua reais e testes headless.

Evidência: `linblux_studio` com 1824 verificações e zero falhas (inclui
`LinbluxNpc` e `NpcExamples`), `linblux_lua_player_validation` com 23 casos no
binário real do Player (quatro novos de NPC), TURBO FAST 13/13 (15,3 s) e TURBO
IMPACT 22/22 (84,6 s), cobertura global parcial declarada pelo TURBO. Nenhum
FULL e nenhum teste gráfico novo: não houve alteração de frontend.

Limitações registradas, não maquiadas: grade XZ em vez de navmesh; apenas
geometria estática entra na navegação; sem saltos, sem ligação entre ilhas e sem
desvio dinâmico de agentes; degraus acima de 1,2 elevam a `unreachable`; o
agente padrão da grade é uma caixa de raio 1,0 e altura 4,5; vida/destino/
waypoints são estado de execução e não são gravados.

Fase 2 continua EM DESENVOLVIMENTO. Próximo incremento pendente: rigs/animações
para NPCs e combate com armas/projéteis, seguido das oito demonstrações
exigidas. Áudio (3), imagens/decals (4), rosto de fábrica (5), UI Lua (6), vida
circular (7), menu lateral (8) e integração (9) permanecem pendentes. O destino
externo privado do AutoBackup continua não configurado. Diffs preexistentes em
`.opencode`, `AGENTS.md`, locales, `EditorUI/GamePanels`, `engine_default_restart`
e ferramentas TURBO foram preservados e não entraram nestes commits.

## Checkpoint mídia e NPCs animados — 25/10/2026

Dois commits locais publicados nesta retomada (base remota anterior `7c305a4`):
`00898e2` corrige a referência canônica de imagens/decals (a UI gravava nome
solto e o salvamento só entendia `assets/<arquivo>`), migra projetos antigos ao
abrir, remove o rosto obrigatório de fábrica e entrega áudio MP3/WAV/OGG com
biblioteca persistida, API Lua 2D e remoção segura. `93fe7e5` entrega rig R6
opcional para NPC (`rig = "player"` ou id/nome estável), clipes procedurais
idle/walk/run/attack/hurt/death derivados do controlador, pose publicada em
`Scene::npcVisuals()` e desenhada pelo renderer existente sem tocar no frontend.
Rig ausente falha `validate`, nunca vira ponteiro órfão; id é serializado como texto.

Fase 2 (NPCs) segue EM DESENVOLVIMENTO: rigs/animações desta entrega estão
IMPLEMENTADOS, TESTADOS e validados no Player, mas combate com armas/projéteis e
as oito demonstrações completas continuam pendentes. Fase 4 (imagens/decals) e
Fase 3 (áudio) avançaram conforme os commits acima; a fase 5 removeu apenas a
imposição do rosto de fábrica. **Inventário/hotbar/itens NÃO foram iniciados** —
nada de `linblux.items` está implementado; permanece o próximo incremento.

Evidência reexecutada nesta retomada: compilação completa OK; `git diff --check`
limpo; `linblux_image_assets`, `linblux_audio`, `linblux_experiences`,
`linblux_studio` (1892 verificações) e `linblux_lua_player_validation` (27 casos,
inclui áudio e exemplos animados) passaram; nenhum FULL nem teste gráfico novo.
Diffs preexistentes em `.opencode`, `AGENTS.md`, locales, `EditorUI/GamePanels`,
`engine_default_restart` e ferramentas TURBO foram preservados e não entraram nos commits.

## Recuperação em andamento — imagens e rosto

Base confirmada local/remota: e4c7c5d4dce1e7b7d3fd58edf08d7a82d3f312de.
93fe7e5 já está publicado; rigs/animações e navegação de NPCs não foram refeitos.
AutoBackup ativo, checkpoint/push d674ad9f7067 confirmado; snapshot dos diffs
iniciais salvo em .turbo/recovery-images (não versionado, sem substituir Git).

IMPLEMENTADO/TESTADO: registro central de usos em Surface, atributos, membros e
adesivos de rig e manifesto; descritores estáveis; limpeza de vínculo legado;
nomes amigáveis persistidos; remoção da face/olhos automáticos do renderizador;
proteção da cena salva antes da exclusão; seletor e lista de usos no editor.
FAST 20261025T235258Z-9735: 17/17, build 183,7 s, testes 18,5 s. IMPACT
20261025T235744Z-13539: 17/17 em 14,6 s, cobertura global parcial. Sem FULL.
Validação gráfica ainda em andamento; não considerar este incremento publicado
ou visualmente aprovado até o registro final. A primeira tentativa gráfica
falhou na sincronização da primeira ação e foi ajustada para preservar os dois
quadros de eventos da fixture existente, com diagnóstico em vez de crash.

A missão atual também exige Modelos com pivô/transformações de conjunto,
multisseleção, Attachments/juntas, inventário, interação segurando E, Drop/Delete,
hotbar e RifleDemo. Esses recursos NÃO estão implementados nesta recuperação.
Áudio 2D/Lua existe na base 00898e2; gerenciamento completo, prévia e integração
adicional continuam a verificar/completar. UI Lua, HUD circular, menu lateral e
integração abrangente seguem pendentes. Não reimplementar NPCs/áudio já existentes.

VALIDAÇÃO FINAL DA RECUPERAÇÃO: três testes gráficos aprovados em74,874 s
(decal_render, rig_image, rig_decals_ui), com pixels de cabeça sem face e rosto
legado explícito, Surface renderizada/transparente e fluxo X11 de recursos.
Corrigida a falta de codec/renderização de Surface descoberta no teste.
IMPACT final 20261026T000837Z-21049: 17/17 em12,9 s, cobertura global parcial.
Após ajuste final de altura, rig_decals_ui passou novamente em67,334 s e a captura
foi inspecionada. Relatório 21 documenta fontes, falhas corrigidas e evidências.
Código pronto para publicação deste incremento. A missão integral segue aberta.
Próxima fase: Modelos reais com pivô/transformações e seleção múltipla, antes de
Attachments/inventário/E/Delete/hotbar. Não reiniciar investigação de imagens
sem um defeito concreto novo.

Backend de recuperação PUBLICADO: 4bb803c16365d79ad62f752fd25991c15772f398,
push normal confirmado. Frontend e rosto VALIDADO NO PLAYER/renderizador compartilhado,
com publicação junto ao relatório 21. Conferir o último commit do relatório para hash.

## Modelos — incremento em validação

Recuperação de imagens PUBLICADA em 4bb803c e bf90835, ambos com push confirmado.
Modelos IMPLEMENTADOS: ModelFrame, pivô, transformações uniformes em lote,
seleção múltipla/Ctrl+G, desagrupamento, hierarquia, clonagem, scripts por Modelo,
API Lua e persistência/exportação. Modelo novo usa JSON v4 e orientação em
quaternion, preservando leitura v1/v2/v3. Escala não uniforme não é suportada.

Regressões encontradas e corrigidas: iterador invalidado em duplicateTree e
perda de precisão na conversão de rotações de 90° ao salvar. Não enfraquecemos
o teste: quaternion passou a ser persistido e a orientação dos filhos é conferida.
FAST 20261026T004226Z-7953: build286,1 s; 17/17 em12,4 s, total299 s; cobertura
global desconhecida devido ao novo teste/CMake. Build final focado2,37 s.
IMPACT 20261026T020406Z-15199: 20/20 em53,8 s (54,3 s totais), cobertura parcial.
linblux_models_ui aprovado em29,644 s, captura inspecionada; PUBLICADO em9fe720b, push confirmado.

PENDENTE dentro da integração de Modelos: gizmos de conjunto e undo composto.
Depois: Attachments e vínculos físicos; inventário, coleta E, Delete, hotbar;
áudio completo/UI Lua, combate/RifleDemo; HUD/menu e integração final. Não há
implementação desses itens escondida neste incremento.

Relatório 22 e MODELOS.md registram APIs, formato v4, testes e limites.
Próximo requisito: integrar gizmos/undo do conjunto e Attachments/vínculos,
sem reimplementar os sistemas de imagem/NPCs já publicados.

## Checkpoint final desta sessão — gizmos e undo

Código IMPLEMENTADO e TESTADO: 271a5cbf684bc367fc8da4dd807f5a1f846dcddf. Modelos agora usam os gizmos
existentes para mover/girar/escalar uniformemente, com undo composto e
cancelamento exato, sem corpo Bullet auxiliar. Propriedades/pivôs entram no
histórico. Corrigido bloqueio de Ctrl+Z/Y por painéis passivos RmlUi.
FAST final17/17; IMPACT20/20; models_ui supervisionado23,394 s aprovado,
captura inspecionada. Cobertura global parcial; sem FULL. Relatório23.

O item histórico “gizmos/undo pendentes” acima está resolvido por este commit.
PUBLICADO: recuperação4bb803c/bf90835 e Modelos9fe720b, push confirmado.
Publicação do novo código e deste relatório: conferir main remoto e relatório
de entrega desta sessão. AutoBackup ativo e checkpoint remoto confirmado.

Próximo requisito PENDENTE: Attachments/juntas, antes de inventário/E/Delete/
hotbar. Depois: gerenciamento completo de áudio, UI Lua, combate/RifleDemo,
HUD circular, menu lateral e integração abrangente. Não confundir a base de
áudio/NPCs existente com conclusão dessas fases. Mundo Padrão/projetos antigos
foram preservados; validação integral futura ainda necessária.

Encerramento solicitado por limite de créditos; a missão NÃO está concluída.
Retomar pelo relatório23 e MODELOS.md, preservando os diffs alheios restantes.

## Alpha 0.1.0 — retomada de 26/10/2026

Base main7d1c470 confirmada, AutoBackup ativo; diffs alheios preservados.
Attachments: IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER em e9d257e4c2e5deb69cbc94145c927c858b831514.
Criação e edição no Explorador/Propriedades, pose local/global, direção Lua,
clone/remoção, JSONv5, reabertura e pacote .lo/Mundo Padrão conferidos.
FAST17/17; IMPACT17/17 (global parcial); models_ui47,226 s aprovado e captura
inspecionada. Relatório24 e ATTACHMENTS.md contêm evidência/limites.

PENDENTE imediato: juntas/vínculos físicos. Depois inventário/E/Delete/hotbar,
áudio completo, UI Lua, combate/RifleDemo, HUD/menu e Demo integrada.
Alpha NÃO pronto para Release: pacote/instalação limpa, regressão final,
segurança e licenças ainda precisam validação. Não foi criado tag/Release.

## Alpha — juntas fixas

Attachments PUBLICADO em e9d257e, relatório90fab49; push confirmado.
Juntas IMPLEMENTADAS/TESTADAS/VALIDADAS NO PLAYER em e6f65914de0664343d004fe6c4e89e2853bfaae1.
Conexão no Bullet preserva poses, quadros locais animam destino dinâmico;
clone remapeia vínculos e JSONv6/.lo/defaults preservam IDs. Lua e seletor
visual funcionais. FAST17/17; IMPACT17/17 (global parcial); GUI45,444 s.
Relatório25 registra correções reais e limites; sem FULL.

PENDENTE imediato agora: inventário/E/Delete/hotbar (não confundir com
inventory.py do TURBO). Depois demais fases Alpha já listadas. Não foi gerado
pacote, tag ou Release; critérios de lançamento ainda não satisfeitos.

## Alpha — inventário conectado ao Editor/Player

Juntas PUBLICADAS em e6f6591, relatório87f3ad7, push confirmado.
Inventário IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER em e4863f372e8225aedd49b1f211964bba5e748039.
Propriedades registram protótipos antes da VM, Lua controla instâncias reais,
coleta/equipamento/descarte mantêm IDs/dados/scripts. Renderização/consultas
ocultam representações guardadas e protótipos. Autoria permanece intacta.
FAST15/15; IMPACT20/20 (global parcial); integração3/3 em57,581 s e captura
inspecionada. Relatório26 e INVENTARIO.md registram APIs e limites Alpha.

PENDENTE imediato: E por proximidade, Delete e hotbar. Ainda não implementados
por este commit. Continuam pendentes áudio completo/UI Lua/combate/RifleDemo,
HUD/menu, Demo, distribuição, validação final e licenças/segurança. Nenhuma
tag/Release criada. Não confundir catálogo e bolsa runtime com lançamento.

## Alpha — E/Delete/hotbar

Inventário PUBLICADO em e4863f3, relatório75168ad, push confirmado.
Interação e controles IMPLEMENTADOS/TESTADOS/VALIDADOS NO PLAYER em 1258b0a5943d6e39e94faf98718d984f2a6f11e3.
E cancela por release/alcance/pausa; linha de visão bloqueia paredes; Delete
solta sem perder ID/dados; slots/páginas dinâmicos funcionam no Studio/Player.
FAST15/15; IMPACT20/20 (global parcial); GUI final31,350 s,720p/1080p,
fonte vetorial e captura inspecionada. Relatório27 e INTERACOES.md.

PENDENTE imediato: combate genérico e RifleDemo. Depois HUD circular, menu
lateral, Demo, pacote/instalação limpa e revisão final. Alpha ainda sem
tag/Release.

## Alpha — UI por Lua

UI Lua IMPLEMENTADA/TESTADA (relatório 29): `game::UiModel` com cinco tipos,
âncoras em tela virtual 1280×720, ids monotônicos, dono por script, limites de
128 elementos/4 níveis/64 mutações por quadro/512 bytes, eventos `click` e
`hover` entregues como tarefas, limpeza ao desabilitar o script ou parar o Play.
Renderização compartilhada (`DrawLuaUi`) na textura do Studio e na tela do
Player; cliques só sem captura de câmera. Três exemplos públicos: munição,
notificação interativa e HUD de vida ligado a `health_changed`.

Evidência: FAST 15/15 com `linblux_lua_interface` (modelo, layout, eventos,
limites, limpeza e os três exemplos); `linblux_lua_ui_render` (pixels em 960×600
e 1280×720); `linblux_lua_ui_player` (Player real, exemplo validado e pintado);
`linblux_visual_studio` 213 verificações sem falha, com o painel Lua encontrado
no framebuffer do Studio durante o Play.

## Alpha — áudio concluído e regressões de teste

Áudio da Fase 3 IMPLEMENTADO/TESTADO (relatório 28): `ProbeAudio` usa metadados
FFmpeg com orçamento de 2 s e whitelist (PCM esparso de 1 h: 3600,00 s com
+6,2 MiB de pico RSS, sem decodificar o arquivo), vozes aceitam `owner` e caem
com o objeto, `device_ready`/`is_streaming`/`duration` expostos em Lua, Player
abre o dispositivo antes dos scripts, Studio tem prévia e exclusão transacional
(backup temporário + rollback em falha de salvamento).

Regressões gráficas de persistência corrigidas: `linblux_editor_restart`
compara rotações com tolerância float32 explícita (sem esconder valores
ausentes ou eixos trocados) e `linblux_engine_default_restart` localiza o cartão
por cor e o Play pelo id do elemento no diagnóstico `LOBLOX_TRACE_UI`, que
agora imprime deslocamentos absolutos e é reemitido a cada redimensionamento.
`linblux_experience_startup` deixou de exigir `faceImage` (a fábrica não impõe
mais rosto) e `linblux_rml_ui` voltou a apontar para o botão de importação da
página de recursos (`assets-import-dialog-`), que usa escopo próprio.

Publicação: `7176e36` (áudio), `08a9f72` (harness de reinício), `f78d396`
(TURBO 0.2.0) e `4de8517` (seletores de UI); `origin/main` confirmado em
`4de8517e771a2e50848d1d668554718391e71994`.

Validação gráfica desta etapa: `linblux_rml_ui` integral, 1051 verificações e 0
falhas em tela 1920x1080 supervisionada (a execução anterior exigia o mesmo
ambiente; uma tentativa em 1600x1000 falhava só porque o Play de 1920 ficava
fora da tela). `linblux_editor_restart` e `linblux_engine_default_restart`
aprovados, além de `linblux_experience_startup`. Nenhum código de imagens, NPCs
ou modelos foi refeito.

