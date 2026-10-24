# Manipulação — continuidade

Base auditada: c8a1dfa. Alterações locais desconhecidas/assets preservados.
Baseline: 1593 checks core e 1518 Studio passaram, apesar do problema real.
Probe ligado à biblioteca anterior: Scale X+ em World, Part Y90, alterava
Size Z 6 -> 6.82750, mantendo Size X 4; centro movia no X global.

Primeiro checkpoint: Scale de ferramenta identifica faces LOCAIS, independentemente
do referencial Move/Rotate. Alt conserva centro; Snap é delta desde o início.
Plano de eixo congelado na face agarrada, com fallback de tela em incidência
rasante; viewport resize cancela. Câmera do Studio não foi modificada.

Mesh, OBB de seleção e wireframe usam OrientedBox. Picking da edição usa a
geometria visível, sem depender de contatos Bullet. Collider sincroniza durante
o drag. View > Colliders (tooltip Show Collision Shapes) lê shapes Bullet reais (ciano; magenta
se erro > 0.001), desligado por padrão e fora do Play.

Half-extents de caixas iniciais estavam corretos. Encontrados: margem antiga
retida depois de resize; cache de contatos não invalidado; static_cast inseguro
ao trocar Box/Sphere. Corrigidos. Margem conserva a política btBoxShape de até
0.04 ou 10% da menor meia-dimensão. A caixa externa coincide com o visual;
arredondamento de contato dos cantos é a margem do solver, não um offset extra.

Move atualizado pela especificação posterior: SEIS setas permanentes, ±X
vermelho, ±Y verde, ±Z azul. Hastes e pontas triangulares, IDs independentes,
picking de ponta antes de plano/hastes. Comprimento/área de clique em pixels;
projeções quase colineares separam as pontas pelo comprimento, mantendo eixo
e pivô. Snap usa delta desde início; Shift libera temporariamente o Snap.
Planos usam quadriláteros projetados (incluindo bordas no picking); planos
vistos de perfil não oferecem um arrasto com direção inventada.

Scale: seis círculos nas faces locais, negativos vazados; Alt escala centrada.
Rotate: três anéis com arcos frontais e marcadores, junções arredondadas.
View > Gizmo size permite 85/100/125/150%; World/Local ao lado do Snap aplica
a Move/Rotate, sem reinterpretar faces de Scale. Readout segue o lado ativo.

Fixture: tests/ManipulationFixture.h (cubo, longo, fino, Y90, composto37,
adjacentes, Baseplate e Model aninhado). Novo teste: 1123 checks, erro máximo
de cantos OBB/Bullet 0.000001966, tolerância 0.0001 na fixture. Inclui raycasts
Bullet nas superfícies redimensionadas, seis lados de Move, Snap ON/OFF,
ajuste livre de 0.017 stud, planos e faces opostas imóveis. Core: 1683/0.
Teste gráfico: 300 casos / 8957 checks / zero falhas.

Capturas inspecionadas da UI real RmlUi: /tmp/loblox-rml-ui-O3LMkm/ (1280,
1366, 1600; Move, Scale rotacionado, Rotate e collision shapes). Arrastos X11
dos seis lados em Local, tamanho 125%, seleção estável, câmera imóvel, um Undo
por drag, save/reload e Play/Stop passaram. Corrigida a fixture de spawn e a
comparação textual estrita de Euler: diferenças medidas de até 0.00002 grau
no roundtrip; IDs/strings são exatos e números toleram 0.0001.
Capturas do binário principal build/loblox:
build/manipulation-validation/main-captures/{move,scale,rotate}-verified.png.
Suíte Release completa: CTest 23/23, 389.63 s, Xvfb/llvmpipe. Após ampliar os
arrastos reais dos seis lados e corrigir clipping do rótulo Colliders/seletor
de tamanho, a fixture RmlUi passou novamente (41.74 s). Captura final limpa
inspecionada; cópias estáveis em build/manipulation-validation/gui-captures/.
Executável principal atualizado via CMake. Nenhum push; assets não rastreados
do usuário preservados. Próximo trabalho grande: Model/pivôs, sem alterar
câmeras e demais sistemas já aprovados.

Pendentes da missão ampla: transformações agregadas de Model, pivô customizado
e validação no notebook/Sway real. Models aninhados já participam de picking,
renderização e Bullet, mas NÃO têm gizmo agregado nesta entrega. Não anunciar
esses recursos como implementados. R6 reutiliza gizmo, sem mudar offsets/animação.

`cmake --build build --target loblox_manipulation_tests loblox -j2`
`./build/loblox_manipulation_tests`
`./build/loblox_manipulation_tests --fixture build/manipulation-validation/fixture.json`
Referências Next-Gen verificadas pelo JSON público do fórum e três imagens
em build/manipulation-validation/references (somente referência, não assets).
Fonte: https://devforum.roblox.com/t/beta-next-gen-draggers-new-move-rotate-scale-tools/4703155
O padrão positivo-only dessa referência foi substituído pelo pedido expresso
de seis setas. Imagem exata adicional mencionada pelo usuário não chegou como
anexo local identificável; pedido de caminho pendente, sem bloquear implementação.
