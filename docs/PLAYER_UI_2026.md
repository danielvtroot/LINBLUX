# Player UI — referência e validação

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

Checkpoint funcional: integração gráfica focada em 1366×768, 330 verificações,
zero falhas; qualidade gráfica em 1366×768 e 1920×1080, 83 verificações por
execução, zero falhas. Suíte completa será repetida antes da entrega.

## Implementação

- Estilo de gameplay separado em `assets/ui/styles/player.rcss`; Studio não
  redesenhado. Ícones originais rasterizados/cacheados pelo loader existente.
- Topbar: marca 46 px, margem 16 px, intervalo 9 px, cápsula 102×46 px com
  hitboxes de 46 px. Settings abre diretamente a aba correta.
- Menu: quatro abas iguais; linhas com rótulo/descrição (39%) e controle (61%);
  valores On/Off entre setas. Sensibilidade e distância têm slider + número.
  Erros aparecem no próprio menu; NaN/Inf e limites inválidos são rejeitados.
- Tipografia: 19 px nas abas amplas / 14 px na viewport estreita; rodapé
  58 px / 48 px. Scroll preserva o rodapé; trocar abas retorna ao início.
- Health 206×47 px, sempre visível quando jogando, oculta em pausa/edição:
  valor nativo, verde/amarelo/vermelho, preenchimento e rastro suavizados.
- Tab/Shift+Tab ficam no menu; Enter/Space ativam botões. Esc após usar slider
  retoma em um pressionamento; em campo numérico cancela a edição antes.
- O foco de imagens/rótulos internos é redirecionado ao botão, em vez de
  tornar um `span` uma folha independente de foco. Regressão testada com
  Respawn → Tab → Enter → Resume.
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
