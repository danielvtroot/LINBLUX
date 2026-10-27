# 35 — Encaixe geométrico do Studio — 27/10/2026

Estado: IMPLEMENTADO / TESTADO; publicação registrada no histórico Git.

Ctrl durante Mover aproxima caixas por faces, respeitando eixo/plano do gizmo.
Dimensões, rotação e geometria real dos filhos do Modelo substituem heurísticas
de distância entre centros. SAT verifica contato e recusa correções que deixem
outras caixas penetrando obstáculos. Shift mantém movimento sem grade.
Marcador de contato, dica no viewport, histórico e cancelamento existentes.
Guia: `docs/EDITOR_PRECISAO.md`.

Arquivos: Gizmo/GizmoMath, EditorUI, testes studio/gizmo_visual, contrato editor.
Sem alteração dos arquivos preexistentes de áudio/CMake/UI. Snapshot inicial
ignorado: `.turbo/recovery-20261027/preexisting.diff`; AutoBackup remoto inicial
`5d38a76d8306`, 433 arquivos, zero descobertos naquele instante.

Validação:
- Build Studio/Player/testes via TURBO: 92,65s; build após testes: 46,02s.
- `linblux_studio`: passou em 6,526s (runner); testes numéricos incluem caixas
  0,2–2000, rotações 0/15/30/45/90°, contato, restrições, obstáculos, pivô
  deslocado/filho rotacionado, undo/redo e cancelamento.
- `linblux_gizmo_visual`: passou em 23,002s; capturas 1280×720/1920×1080
  inspecionadas em `/tmp/linblux-surface-snap-PBPIoz/`.
- FAST `20261027T205255Z-29378`: 17/17, testes 10,4s; cobertura desconhecida.
- IMPACT `20261027T205414Z-30819`: 23/23, 68,2s; cobertura parcial, sem gráficos
  automáticos. Binários novos compilados pelo FAST; demais testes não foram
  recompilados por esse IMPACT sem build.
- `linblux_models_ui` passou em 52,087s, mas seu executável não foi recompilado
  neste incremento: evidência auxiliar do baseline, não aprovação da integração
  nova. O gizmo visual e o teste studio foram recompilados.
- Falhas iniciais: setter/tipo incorretos corrigidos no teste; fixture de
  obstáculo corrigida; poses restauradas comparadas com tolerância, não por
  igualdade textual de floats normalizados.

Limites: sem encaixe de esfera/malha, giro automático por normal, modo dedicado
de vértice/aresta ou transformação conjunta de multisseleção sem Modelo.
Sem bloqueio global de sobreposição nem conclusão integral da missão.
Próximo: contraste/contornos no editor e seleção de peças sobrepostas.
