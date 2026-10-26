# Menu lateral — 26/10/2026

IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER: Studio RmlUi e Player independente
usam painel à esquerda, entrada/saída de 180 ms, marca oficial e navegação
vertical. Continuar, jogador/respawn, câmera, controles, áudio, vídeo, ajuda e
saída preservam as ações reais. Pausa bloqueia movimento e captura; primeira
pessoa/trava recuperam o mouse ao continuar. Volume controla SetMasterVolume;
sem dispositivo, a UI informa indisponibilidade em vez de simular reprodução.
Player usa tela cheia sem bordas e restaura o tamanho anterior.

ATLAS consultado para Runtime/Audio/StudioFrontend. Build dos hosts aprovado
(53,55 + 5,02 + 57,55 s; últimas recompilações focadas 17,54/3,47/11,98 s).
`linblux_player_menu_ui`: aprovado em43,36 s, 133 verificações; testa layout
720p/1080p, captura, movimento bloqueado, câmera/sensibilidade/Shift, páginas,
respawn, fechar e parar. `linblux_player_menu`: aprovado em16,42 s no executável
real, Esc, mouse, áudio/vídeo, tela cheia/restauração e saída, duas resoluções.
Capturas do Studio720p e Player1080p inspecionadas.

Falhas corrigidas: texto anônimo em botões flex RmlUi não aparecia (usar span),
barra superior sobrepunha a marca (ocultar durante menu), fullscreen exclusivo
não expandia no Xvfb (usar sem bordas). Pulsos X11 curtos eram perdidos durante
renderizações de software; o harness mantém tecla/botão por250 ms. A primeira
rodada RmlUi excedeu75 s; o teste agora concentra funções em720p e confere
layout em1080p. Não foram repetidos testes aprovados sem alteração relevante.

Limites: não foi ouvido áudio neste ambiente sem dispositivo. O fallback
ImGui histórico de depuração ainda usa seu menu anterior; o Studio Alpha usa
RmlUi. Seletores históricos de regressão que exigiam o menu central e a barra
RML ainda precisam migrar antes do fechamento de regressão/Release.

FAST final20261026T225715Z-20329:17/17 em8,8 s, cobertura global desconhecida.
A rodada anterior encontrou o defeito real do RifleDemo em andamento (resolvido
no exemplo; não incluído no commit deste menu). Núcleo recompilado: studio,
core e UI Lua3/3 aprovados; não há aprovação global/FULL implícita.

IMPACT20261026T225754Z-20749:23/23 em59,0s, cobertura global parcial.
