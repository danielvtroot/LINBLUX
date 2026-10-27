# 39 — Recuperação das dependências do Studio Audio — 27/10/2026

Recupera o WIP existente, sem reconstruir áudio ou seletor nativo. A interface
publicada em2ed404f/cbce14d já referenciava GameAssets.music, mas suas classes,
persistência e ações ainda não estavam no histórico main. Auditoria identificou
a dependência; este incremento reúne o suporte faltante com testes adicionais.

Errata:7674972 incluiu também `music_.stop(audio_)` num hunk combinado com reset
da apresentação. O backend completo é publicado agora, sem reescrever histórico.
Os builds anteriores usaram o conjunto com WIP, não um checkout isolado daquele
commit. Não tratar os hashes intermediários como candidatos de distribuição.

WIP aproveitado: MusicSettings/MusicPlaylist/AudioSystem, JSON opcional por IDs,
referências/remoção, ciclo de Play/Pause/Stop, ações de biblioteca/playlist e
inserção segura de exemplos instalados, ícones próprios e estilos de áudio.
Exemplos são copiados/instalados no CMake. UI Lua de áudio existente preservada.

Correção nova: teste reproduziu reinício após Lua stop_all. A playlist agora
distingue término natural de parada explícita; histórico limitado a64 términos.
Só término natural avança faixa. Não duplica vozes, dispositivos ou biblioteca.
Rótulos Volume/Repetir/Tipo receberam spans para o layout flex real do RmlUi.
studio.rcss é manual; gerador só escreve theme.rcss/cabeçalhos de paleta.

Testes:
- Antes: `music-wip-before.log` falhou em stop_all; prova da causa.
- Build principal198,53s; configure/fixtures7,64s; rebuild final11,28s.
- FAST `20261027T212714Z-24113`:17/17, testes8,2s, build179,9s, escopo desconhecido.
- Audio+AudioPanel2/2 em14,252s, `music-recovery-verified.log`. UI33 verificações
  sem falha; importação, configuração por botões, playlist, salvar/reabrir,
  rejeição de caminho de exemplo indevido e inserção de Script real.
- Audio nativo testa MP3/WAV/OGG reais, playlist/fundo, volume, pausa, repetição,
  parada por Lua no Runtime, JSON transacional, referências e exportar/importar
  `.lo` com arquivo e início de áudio no runtime importado. Teste headless não
  comprova saída audível. Probe PCM1h:3600s, pico adicional6556KiB neste run.
- IMPACT `20261027T213103Z-27089`:23/23 em76,0s, cobertura parcial; inclui também
  o novo rastreador de queda. Sem FULL. Capturas de biblioteca/configurações
  inspecionadas em `/tmp/loblox-rml-ui-iXiXN2/`.
- Novos testes CTest: linblux_editor_precision_ui e linblux_audio_panel_ui,
  gráficos com timeout60s, para uso pelo TURBO.

Guia: AUDIO_MUSICA.md. Sem mixer por categorias ou áudio espacial novos.
Não resolve licenças/áudio físico/outro Linux/regressão RmlUi longa; Alpha não
liberado. Próximo requisito funcional: ESC como menu, sem pausar simulação.
