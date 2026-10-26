# 23 — Gizmos e undo de Modelos / continuidade

Código: `271a5cbf684bc367fc8da4dd807f5a1f846dcddf`. Esta entrega fecha o incremento de edição de Modelos;
a missão integral continua EM DESENVOLVIMENTO. Encerramento solicitado pelo
usuário devido ao orçamento de créditos, sem iniciar novas funcionalidades.

## Implementado e testado

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

## Evidências reais

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

## Já publicado nesta recuperação

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

## Próxima ação exata

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
