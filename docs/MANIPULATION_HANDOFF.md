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
o drag. View > Show Collision Shapes lê shapes Bullet reais (ciano; magenta
se erro > 0.001), desligado por padrão e fora do Play.

Half-extents de caixas iniciais estavam corretos. Encontrados: margem antiga
retida depois de resize; cache de contatos não invalidado; static_cast inseguro
ao trocar Box/Sphere. Corrigidos. Margem conserva a política btBoxShape de até
0.04 ou 10% da menor meia-dimensão. A caixa externa coincide com o visual;
arredondamento de contato dos cantos é a margem do solver, não um offset extra.

Fixture: tests/ManipulationFixture.h (cubo, longo, fino, Y90, composto37,
adjacentes, Baseplate e Model aninhado). Novo teste: 541 checks, erro máximo
de cantos OBB/Bullet 0.000001966, tolerância 0.0001 na fixture. Core/Studio
anteriores continuam passando. UI/UX e Models ainda em desenvolvimento.

`cmake --build build --target loblox_manipulation_tests loblox -j2`
`./build/loblox_manipulation_tests`
`./build/loblox_manipulation_tests --fixture build/manipulation-validation/fixture.json`
Referências Next-Gen verificadas pelo JSON público do fórum e três imagens
em build/manipulation-validation/references (somente referência, não assets).
Fonte: https://devforum.roblox.com/t/beta-next-gen-draggers-new-move-rotate-scale-tools/4703155
