# Etapa 6 — independência e manutenção (24/10/2026)

## Implementação real

Engine, edição, renderização, apresentação, mídia e Studio têm targets separados.
O executável `linblux_player` carrega e executa experiências sem link com ImGui,
RmlUi ou Studio. A cena autoral permanece independente do runtime. Server está
no roadmap: não há implementação multiplayer funcional.

O cartão fixo **Mundo Padrão da Engine** abre um documento protegido fora da
biblioteca. Publicar o padrão exige confirmação, faz backup do índice anterior
e publica mundo/personagem juntos. Restauração também exige confirmação e backup.
O editor permite substituir o personagem, editar as juntas compatíveis e importar
personagens de experiências `.lo`/JSON, copiando imagens e animações.

O formato atual tem seis corpos/juntas. Importação de malhas e esqueletos
arbitrários, terreno avançado e controladores externos não estão implementados.
Esses limites impedem declarar concluído o objetivo amplo de importação irrestrita.

## Arquivos principais

`CMakeLists.txt`, `src/player/main.cpp`, `src/editor/SceneRendererAdapter.cpp`,
`src/experiences/ExperienceManager.*`, `src/ui/ExperienceUI.*`,
`src/editor/AnimationEditor.cpp`, `src/ui/StudioFrontend.cpp`,
`tests/engine_default_main.cpp`, `tests/engine_default_restart.py`,
`packaging/*`, README, CONTRIBUTING e guias atuais em docs.
Os documentos obsoletos foram preservados em `docs/historico` com aviso explícito.
Licenças e créditos foram mantidos, inclusive o aviso de copyright anterior.

## Regressões encontradas e corrigidas

A primeira rodada completa encontrou dez falhas em 32 testes. Elas incluíam
buscas por títulos ImGui antigos, coordenadas de botões dependentes de texto em
inglês, geometria de logo antiga, formulário com ação fora da área visível e
roteamento de foco dos painéis na execução. As verificações foram atualizadas
para os controles e a arte reais, preservando as asserções funcionais.

A suíte RmlUi revelou ainda a expansão excessiva dos botões com `flex-basis:auto`.
O ribbon passou a ter larguras explícitas, textos curtos e rolagem em janelas
estreitas. A confirmação de animação usa medidas dos textos traduzidos.
Uma asserção recém-adicionada que supunha WASD obrigatório com botão direito
foi corrigida para o comportamento efetivo da câmera existente.

No Player, o filtro de recursos rejeitava a logo oficial fora do projeto aberto.
A exceção agora é específica para a logo interna; caminhos externos continuam
bloqueados. A imagem é desenhada com alfa pré-multiplicado.

## Evidências e limites

O teste de ciclo de vida publica um personagem original com hierarquia, animação,
imagem, física, câmera e script; reinicia em outro processo, cria uma experiência
e confirma o rig correto em movimento. Também verifica importação, publicação
atômica, cópias independentes, backup e preservação de experiências antigas.
O teste com o Studio real abre o cartão, encerra/reinicia, cria pela interface e
confirma o personagem herdado no runtime. A autoria detalhada da fixture é feita
pelo teste de núcleo; não se apresenta isso como edição manual de cada campo.

O decoder percorreu 721 frames e um loop em 24,22 s, com 6,97 s de CPU e pico de
68,9 MiB RSS. O Home completo em Xvfb/llvmpipe ficou próximo de 247 MiB RSS, com
variação de 20 KiB nas três amostras e 219–221% de CPU. A rasterização é por CPU;
esses números não representam uma GPU integrada real e não provam desempenho
adequado em todos os notebooks. É necessária medição física adicional.

A configuração sem FFmpeg compilou e foi renderizada: fundo grafite, zero frames
decodificados, sem substituição por foto de carro. A instalação em prefixo isolado
abriu o Studio usando os recursos instalados. A logo original manteve SHA-256
`5693046f85abee20fbc1cb164a3ef0ea80e1ec02827474ff2ee95c8c300dcf78`.

Commits: `aaddc8d` separação/runtime/padrão protegido; `4359a25` logo do Player.
Ambos enviados por push normal e confirmados no remoto oficial.

A revisão de assets deixou imagens pessoais de licença desconhecida fora da
instalação e do índice atual; cópias locais e históricas foram preservadas.
A licença dos materiais fornecidos e o empacotamento de FFmpeg GPL requerem
revisão antes de distribuição de binários. Veja ASSETS_E_LICENCAS.md.
