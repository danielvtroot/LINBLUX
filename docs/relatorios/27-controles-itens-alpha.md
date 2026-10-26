# 27 — Interação, E, Delete e hotbar

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
Guia INTERACOES.md e exemplo interacao_porta.lua usam a API pública real.

Próxima fase: áudio. Auditoria encontrou ProbeAudio usando LoadWave para a
música inteira só para duração, e Player abrindo dispositivo depois dos scripts.
Corrigir duração com metadados limitados/streaming e ordem de inicialização;
validar memória/reprodução, completar prévia/gerenciamento sem reescrever backend.
Depois UI Lua, RifleDemo/combate, HUD circular/menu lateral, Demo e distribuição.
Não existe pacote/tag/Release Alpha validado nesta entrega.
