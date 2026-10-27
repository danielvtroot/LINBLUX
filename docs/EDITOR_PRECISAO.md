# Construção e encaixe no Studio

Durante **Mover**, segure **Ctrl** para aproximar superfícies de caixas.
O gizmo mantém a restrição do eixo/plano escolhido; não gira a seleção.
**Shift** libera a grade e pode ser combinado com Ctrl. Sem Ctrl, o movimento
mantém os incrementos configurados na barra de ferramentas.

O contato usa dimensões e orientações das caixas (SAT, 15 eixos), não a
distância entre centros. Um Modelo usa as caixas reais dos filhos, mesmo com
pivô deslocado. A aproximação permite corrigir até 10 pixels de distância
projetada na profundidade do pivô, limitada a 0,02–2 unidades de mundo.
O encaixe não é aceito se a correção introduzir interseção com outra caixa.
O marcador vermelho indica o contato. Um clique sem arraste não encaixa nada.

O gesto continua usando o histórico existente: soltar confirma uma operação,
Esc cancela; Ctrl+Z/Ctrl+Y desfazem/refazem inclusive as poses dos filhos e pivôs.
Comparações geométricas usam tolerância proporcional à precisão float, sem
exigir identidade textual de quaternions normalizados.

Limites: somente Parts em forma de caixa, incluindo pisos/blocos. Esferas,
malhas arbitrárias, alinhamento automático de rotação e seleção explícita de
vértices/arestas não são oferecidos por este incremento. O encaixe é assistido,
não um bloqueio global de sobreposição: fora dele o criador ainda pode construir
geometria intersectante. A multisseleção continua com o comportamento anterior;
para mover um conjunto com este encaixe, agrupe-o como Modelo.

Testes: `linblux_studio` cobre contato, restrições, rotações, dimensões, pivô,
filhos e histórico. `linblux_gizmo_visual` verifica o marcador renderizado e os
gizmos existentes; gera capturas em 1280×720 e 1920×1080 em diretório temporário.

## Leitura da construção e seleção

O Editor mostra arestas discretas respeitando a profundidade da cena; não altera
o material nem ativa linhas no Player. **F8** alterna contornos, **Shift+F8**
alterna caixas AABB e **Ctrl+F8** mostra normais das faces das caixas selecionadas.
O comando existente **Mostrar colisores** continua consultando o Bullet real;
uma AABB de depuração não representa um novo collider.

**Alt+clique** percorre as Parts que o raio cruza, da frente para trás, retornando
à primeira. **Shift+clique** seleciona o Modelo ancestral mais próximo.
Ctrl+clique mantém a multisseleção. O gizmo continua tendo prioridade quando uma
alça é clicada. Geometria com alfa zero não captura seleção. F enquadra a seleção,
e F2 renomeia pelo Explorador, preservando nomes com espaços.

Novas peças inseridas pela interface usam `Peça` ou `Esfera`. Ao duplicar nomes
existentes, a engine usa sufixos `_002`, `_003` etc., sem concatenar números a
cada geração. Nomes salvos anteriormente não são alterados e referências usam
IDs. O limite de 128 bytes é respeitado sem cortar uma sequência UTF-8 no meio.

O passe de contornos não corrige superfícies duplicadas: coplanaridade continua
um problema de geometria. Não há SSAO/PBR/sombras de contato novas neste trabalho.
Desempenho de GPU integrada ainda não foi medido.
