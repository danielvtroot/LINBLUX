# Conclusão da engine LINBLUX — acompanhamento verificável

Missão integral **não concluída**. Estados: PENDENTE, EM DESENVOLVIMENTO,
IMPLEMENTADO, TESTADO, VALIDADO NO PLAYER, PUBLICADO, BLOQUEADO. Uma publicação
de incremento não encerra sua fase quando ainda faltam requisitos obrigatórios.

## Ponto de partida confirmado nesta execução

HEAD local/remoto main: cf53b096be2c00693b56dbf7775ef93828b53537.
Scripts clonados/reativação, destruição, contatos Bullet, passo fixo e entrada
Lua já estão publicados. Resultados do relatório 12 são históricos e não
substituem os testes futuros. Alterações locais de outras tarefas preservadas.

## Fases

| Fase | Estado | Implementação/evidência atual | Próximo requisito obrigatório |
| --- | --- | --- | --- |
| 0 AutoBackup | PUBLICADO | cd4fb406cfccc6b030e56eadbede0c3965ef66f3, push main confirmado. Monitor labubu ativo via cron, @reboot/watchdog, checkpoints e push automático isolados. Clone remoto recuperou 315 arquivos; índice/main intactos; reinício em 20,005 s. Offline 20/20 e IMPACT 17/17, cobertura global parcial. Relatório 13. | Destino externo privado não informado (cópia externa não configurada). Backend systemd não executável neste host. |
| 1 Lua/scripting | EM DESENVOLVIMENTO | Commits 5139c20, 5380967, cf53b09; guia SCRIPTING_LUA e testes studio/script_input_ui existentes. | Componentes/transformações/câmera/tempo/persistência restantes; áudio e UI integrados nas fases dependentes; novos testes. |
| 2 NPCs/navegação | PENDENTE | Não se declara sistema implementado. | Entidades, rig/animações, vida/movimento, navegação física, oito demonstrações reais. |
| 3 Áudio | PENDENTE | Nenhuma API de áudio anunciada como pronta. | Importação MP3/WAV/OGG, streaming, Editor/Player/Lua, referências, persistência/exportação. |
| 4 Imagens/decals | PENDENTE | Há infraestrutura de imagens e superfícies a auditar com ATLAS. | Verificar/ampliar operações de assets, propriedades/Lua, exclusão segura, persistência/exportação. |
| 5 Rosto de fábrica | PENDENTE | Não remover imagens de experiências existentes. | Identificar aplicação automática e alterar só fábrica com testes de rig/spawn. |
| 6 UI Lua | PENDENTE | Nenhuma UI Lua declarada pronta. | Componentes reais, eventos, renderização, persistência, separação da UI protegida. |
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
| Volumes Bullet e controle Lua do jogador | VALIDADO NO PLAYER | Relatório 18: overlap_box/overlap_sphere reais, filtros, cotas, intenção de movimento, salto, velocidade, força/impulso. FAST/IMPACT 13/13. | script_input_ui 1/1 em 20,792 s; publicação em andamento. Regiões contínuas e NPCs pendentes. |
| Áudio por Lua | PENDENTE | Não há namespace documentado como implementado. | Dependência da Fase 3: backend/importação/streaming/referências reais. |
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
