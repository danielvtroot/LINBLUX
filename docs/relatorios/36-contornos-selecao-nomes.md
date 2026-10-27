# 36 — Construção legível, seleção e nomes — 27/10/2026

Estado: IMPLEMENTADO / TESTADO. Encaixe anterior PUBLICADO em
`0142aa98daf5c9edf818c9b34298088be7a9311a` (remoto confirmado).

- Contornos opcionais com profundidade no Editor; hover discreto, AABB e normais
  de caixas selecionadas. F8 / Shift+F8 / Ctrl+F8; Player não chama este passe.
- Alt+clique percorre Parts sob o raio; Shift+clique escolhe o Modelo ancestral
  mais próximo. Geometria com alfa zero não captura cliques. Gizmo tem prioridade.
- Sobreposição volumétrica/coincidência entre caixas selecionadas e outras
  caixas gera diagnóstico no viewport. Não muda nem apaga a geometria.
- Novas peças da UI: Peça/Esfera. Duplicatas usam `_002`, `_003`, preservando
  nomes antigos, espaços, IDs e limites UTF-8. APIs/identificadores legados mantidos.
- Marcador de encaixe desenhado após o gizmo, com fundo para leitura.

Fontes: Scene, SceneRenderer, EditorUI, Gizmo; testes studio/gizmo_visual/rml_ui.
Guia: EDITOR_PRECISAO.md. Capturas próprias com câmera/material iguais em
`docs/screenshots/editor-precision/{antes,depois}.png`.

Evidências:
- Build Studio/Player/gizmo/studio/rml_ui: passou, 56,54s após correção de include.
- FAST `20261027T210048Z-4136`: 19/19, testes 9,7s, build 180,7s; cobertura
  desconhecida. Cabeçalho Scene afeta muitos consumidores. WIP de áudio presente.
- Numérico+gizmo gráfico: 2/2 em15,766s; comparação de pixels 720p/1080p,
  materiais preservados. Capturas antes/depois inspecionadas.
- Models UI recompilado: passou em39,268s; inclui agrupar, arrastar, undo/redo,
  propriedades, salvar e Play/Stop. `.turbo/logs/editor-models-ui-current.log`.
- Fixture curta nova `LINBLUX_EDITOR_PRECISION_ONLY=1`, X11/RmlUi real:
  63 verificações/0 falhas, 23,558s. Seleção por Alt, Modelo por Shift, F8,
  arraste com Ctrl, contato sem penetração e undo. Screenshot inspecionada.
  Supervisão TURBO, timeout55s, log `.turbo/logs/editor-precision-x11.log`.
- Build após legibilidade do marcador:5,17s. Revalidação final: gizmo gráfico
  aprovado em22,505s e X11 63/0 em26,246s; captura final inspecionada. Logs
  `editor-contact-final.log` e `editor-precision-x11-final.log`.

Não há alegação de ganho de FPS, sombras de contato, PBR ou correção universal
de coplanaridade. Diagnóstico refere-se a caixas com interseção volumétrica;
malhas arbitrárias e superfícies meramente tangentes não são classificadas.
WIP preexistente de áudio/frontend/CMake preservado fora dos commits.
Próxima correção investigável: giro de NPC calculado por Euler Y (asin), que
pode limitar/oscilar o heading depois de90°. Reproduzir antes de corrigir.
