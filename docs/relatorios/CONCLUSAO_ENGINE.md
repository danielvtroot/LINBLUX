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
| 0 AutoBackup | EM DESENVOLVIMENTO | Código local ainda não publicado. Sem systemctl; SysV/cron disponível. Estado do usuário labubu possui checkpoint remoto 2a1fd99 de execução anterior, mas monitor parado. O estado root vazio não representa o usuário labubu. | Corrigir inicialização, segurança de snapshots, concorrência; testar envio automático e recuperação real. |
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

1. Continuar a Fase 0 antes de grandes alterações de engine.
2. Usar ATLAS para localizar e TURBO para build/testes; guardar logs em .turbo.
3. Publicar somente arquivos revisados da unidade funcional; preservar diffs alheios.
4. Registrar testes com execução, tempo, resultado, limitações e hashes remotos.
5. Dados privados/ignorados não se tornam protegidos por haver push de código.
