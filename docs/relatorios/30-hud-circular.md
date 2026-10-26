# HUD circular — 26/10/2026

IMPLEMENTADO/TESTADO: um renderizador compartilhado no Play do Studio e Player,
saúde real no centro e arco vermelho proporcional, interpolação exponencial,
zero imediato na morte e máximo correto no respawn. Não cria textura por frame.
A barra RmlUi antiga fica oculta e a barra nativa foi retirada. O indicador
sobre a cabeça, quando configurado, usa vermelho. `linblux.ui.health_hud(false)`
oculta o HUD padrão; `true` restaura. Novo Play restaura a preferência padrão.

ATLAS localizou Runtime, ExperienceOverlay, GamePanels e StudioFrontend.
Build incremental dos hosts/testes: 137,82 s + 28,25 s. TURBO FAST
20261026T223856Z-8205: 16/16, 9,4 s, cobertura global desconhecida (sem rebuild
adicional; os targets afetados foram compilados antes).
Supervisão TURBO: health_hud + lua_ui_render + lua_ui_player, 3/3 em 28,948 s.
Health_hud verifica dano/cura/morte/respawn, interpolação, ocultação Lua,
pausa/reinício e pixels em 1280×720,1600×900,1920×1080; captura 720p inspecionada.
Logs/capturas em .turbo/logs e build-linblux/.turbo/logs, ignorados.

Limites: a regressão integral ainda precisa atualizar os seletores de testes
históricos que exigiam a antiga barra RML. Não executar esses testes como se a
expectativa visual antiga ainda fosse requisito do produto. Release ainda
bloqueada por menu/Demo/pacote e validação final. Trabalho desenvolvimento de combate
preservado fora do commit deste HUD, snapshot em .turbo/recovery-final e
AutoBackup remoto eed462f8f1d3 confirmado no início.

IMPACT 20261026T223928Z-8644: 22/22, 56,0 s, cobertura global parcial.
