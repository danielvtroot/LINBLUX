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
