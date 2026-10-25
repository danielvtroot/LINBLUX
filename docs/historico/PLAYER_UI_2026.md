> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Player UI — referência e validação

Atualização prioritária: [CAMERA_HUD_FIXES.md](CAMERA_HUD_FIXES.md).
O cartão rotulado de Health descrito abaixo foi rejeitado e substituído pela
barra minimalista anterior. Não restaurar esse cartão ao polir menu/topbar.
O restante deste documento registra a reconstrução anterior.

Referências locais examinadas diretamente: `referencias/topbar.jpeg` (1600×738)
e `referencias/menudojogo.jpeg` (1777×1408). Originais preservados.

| Componente | Medida observada | Alvo Loblox / adaptação funcional |
|---|---|---|
| Topbar | Marca circular, espaço ~10 px, cápsula compartilhada; ícones claros de mesmo peso | Marca Loblox + cápsula Menu/Settings; sem chat/voz fictícios |
| Menu | x≈10,4%, largura≈79,4%; y≈8,4%, altura≈85,5%; superfície quase preta translúcida | 84% de largura (92% na viewport estreita), mundo visível, dentro da viewport |
| Abas | Linha horizontal, ícone+rótulo, sublinhado claro na ativa | Game / Settings / Controls / About, sem serviços online falsos |
| Settings | Rótulo/descrição à esquerda (~38%), controle à direita (~60%); setas laterais, valores centrados | Seletores de câmera/On-Off, sliders e valor numérico; linhas confortáveis |
| Rodapé | Três botões iguais com atalho enquadrado; Resume azul | Stop / Respawn / Resume, ações reais; foco de teclado |
| Health | **Não aparece em nenhum dos dois JPEGs** | Componente próprio discreto, valor real legível; não apresentado como reprodução de HUD padrão Roblox |

Antes: dois círculos separados; menu com título adicional e limite estreito de
850 px; Settings com campos de texto e checkboxes; Health sem valor e sempre
verde. A migração mantém RmlUi 6.3, PNG premultiplicado/mipmaps e MSAA 4×.

As preferências de câmera no menu continuam sendo da sessão de Play (contrato
existente); Game Config continua responsável pelos defaults persistidos do mapa.
Não se altera o documento autoral durante Play.

Checkpoint de implementação: `5538899`. Validação final: CTest **19/19**,
408,99 s; integração RML **984 verificações / zero falhas**; qualidade gráfica
1366×768 e 1920×1080, **83 verificações / zero falhas em cada execução**.
Depois do ajuste puramente RCSS de altura máxima/About: integração focada
1366×768 + fullscreen 1920×1080, **331 verificações / zero falhas**; About
1280×720 também reaberto e inspecionado no executável principal.

## Implementação

- Estilo de gameplay separado em `assets/ui/styles/player.rcss`; Studio não
  redesenhado. Ícones originais rasterizados/cacheados pelo loader existente.
- Topbar: marca 46 px, margem 16 px, intervalo 9 px, cápsula 102×46 px com
  hitboxes de 46 px. Settings abre diretamente a aba correta.
- Menu: quatro abas iguais; linhas com rótulo/descrição (39%) e controle (61%);
  valores On/Off entre setas. Sensibilidade e distância têm slider + número.
  Erros aparecem no próprio menu; NaN/Inf e limites inválidos são rejeitados.
- Altura máxima de 800 px em telas amplas evita a área vazia excessiva que
  ainda apareceu na primeira captura fullscreen. About usa conteúdo centralizado.
- Tipografia: 19 px nas abas amplas / 14 px na viewport estreita; rodapé
  58 px / 48 px. Scroll preserva o rodapé; trocar abas retorna ao início.
- Health 206×47 px, sempre visível quando jogando, oculta em pausa/edição:
  valor nativo, verde/amarelo/vermelho, preenchimento e rastro suavizados.
- Tab/Shift+Tab ficam no menu; Enter/Space ativam botões. Esc após usar slider
  retoma em um pressionamento; em campo numérico cancela a edição antes.
- O foco de imagens/rótulos internos é redirecionado ao botão, em vez de
  tornar um `span` uma folha independente de foco. Regressão testada com
  Respawn → Tab → Enter → Resume.
- Space também ativa Resume, mas pertence à UI até terminar esse evento:
  não é retransmitido como pulo. O teste observa a velocidade a cada frame,
  evitando perder um impulso por medir somente depois do ápice.
- Transições curtas de abertura/abas/hover; nenhuma biblioteca adicional.

## Fontes complementares

[Settings e controles oficiais](https://en.help.roblox.com/hc/en-us/articles/203314230-In-experience-Settings-and-Help)
confirma Classic/Follow e Shift Lock. A composição vem dos dois JPEGs, não
de uma captura web substituta.

[Humanoid](https://create.roblox.com/docs/reference/engine/classes/Humanoid)
distingue Health e as opções de exibição acima do personagem. Não se confunde
essa barra com uma HUD personalizada de experiência. A HUD local Loblox não
altera o contrato do Humanoid nem as opções de nome/vida acima da cabeça.

Diferenças intencionais: quatro abas locais em vez de serviços online;
Settings no lugar de chat/voz na cápsula; marca Loblox; Health própria;
adaptação compacta para a viewport dockada. Sem cópia de assets Roblox.

A fixture de qualidade foi adaptada à geometria nova mantendo os limites de
cobertura/nitidez. Corrigido seu estimador: a escolha da cor de borda comparava
com o último candidato mutável e podia travar num pixel de cobertura 50%.
Superfícies translúcidas agora são calibradas com um pixel totalmente coberto;
não se pressupõe que todo painel escuro é mais claro que o fundo. Resultado:
três níveis intermediários de cobertura na marca, cápsula e cantos da Health,
em ambos os fundos. MSAA 4× e filtragem de imagens não foram alterados.

## Capturas e diferenças verificadas

Capturas reais, abertas para inspeção, em `build/player-ui-2026/validation/`:
Play/Health 100 e 25, Game/Settings/Controls/About em 1280×720, 1366×768,
1600×900 e 1920×1080; fullscreen; Health 70, 0 e respawn. O teste usa X11,
RML/GL3 e os objetos/runtime reais; não são mockups. `application/` contém
capturas do executável principal. `previous/` é histórico anterior disponível,
**não** um comparativo A/B do mesmo commit a10f2fb.
`polish/` contém a rodada final após limitar a altura máxima, incluindo todas
as abas fullscreen e a integração por teclado. O limite só afeta telas amplas;
o tamanho do menu na viewport compacta não mudou.

| Área | Referência | Resultado observado / diferença restante |
|---|---|---|
| Topbar | Marca + cápsula, ícones claros | Grupo e alinhamento reproduzidos; dois controles locais em vez de chat/voz; escala lógica de desktop, não cópia de pixels JPEG |
| Menu | Superfície translúcida, abas sublinhadas, rodapé em três partes | Mesma organização; quatro abas; limites medidos dentro da viewport deslocada; texto e botões sem clipping nas capturas |
| Settings | Fileiras de rótulo/descrição + seletor com setas | Estrutura reproduzida com controles reais; sliders + número acessível; menos opções que a referência, sem inventar serviços |
| Health | Ausente nos JPEGs prioritários | Design Loblox próprio: valor legível e três cores de estado; oculta no menu; não se alega equivalência à HUD padrão Roblox |
| Tipografia/ícones | Fonte e ícones da referência | DejaVu Sans e line art original; diferenças de desenho e densidade permanecem |

## Medições e limites

Medição serial disponível: llvmpipe, 1366×768, cena padrão, dois threads Mesa,
MSAA 4×. RSS idle 231408 KiB (~225,98 MiB), Play/pausa 231960 KiB (~226,52 MiB).
Frame idle ~76–79 ms, Play ~83–85 ms, pausa ~87–94 ms. Apresentação CPU da UI
~0,3–0,5 ms; passagem de renderização RML ~35–50 ms em software. Sem baseline
A/B equivalente nesta missão e sem inferir esses números para a Vega 8.

Pendentes: avaliação visual do usuário; FPS/memória na Vega 8; Sway físico,
Alt+Tab real e DPI fracionário. Não se declara fidelidade de 100% nem aprovação
no notebook. Studio, física, Lua, rigs, animações, céu e persistência do mundo
não foram reimplementados.

Comandos finais: `cmake --build build --target loblox -j2`;
`ctest --test-dir build --output-on-failure -j2`; `git diff --check`.
Executável: `build/loblox`. F5 inicia; ícone de engrenagem abre Settings;
Esc alterna pausa; F11 fullscreen; Shift+F5 retorna ao Studio.
