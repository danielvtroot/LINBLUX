# Construção e encaixe no Studio

Durante **Mover**, segure **Ctrl** (ou ative Encaixe) para colocar a seleção
na face de caixa sob o cursor. Esse modo posiciona livremente em 3D; sem
encaixe o gizmo mantém o eixo/plano escolhido. O alvo vem do raycast OBB mais
próximo, com desempate estável; não da menor correção global.
**Shift** libera a grade e pode ser combinado com Ctrl. Sem Ctrl, o movimento
mantém os incrementos configurados na barra de ferramentas.

O contato usa dimensões e orientações das caixas (SAT, 15 eixos), não a
distância entre centros. Um Modelo usa as caixas reais dos filhos, mesmo com
pivô deslocado. Em **Modelo → Construção**, escolha **Face**, **Borda** ou **Vértice**. A opção
**Alinhar à normal** é desligada por padrão; quando ativa gira o Y local da
seleção para a normal, incluindo filhos e pivô do Model. A prévia tem tom
translúcido e um marcador de contato. Se houver interseção, a última pose
aceita é mantida e o status identifica a peça bloqueadora e a ação sugerida.
Um clique sem arraste não encaixa nada.

**Grade → Incremento** preserva o arredondamento relativo ao início do gesto.
**Grade → Absoluta** arredonda a coordenada projetada no eixo/plano ativo;
componentes inativos não mudam. Os atalhos antigos continuam válidos.

O gesto continua usando o histórico existente: soltar confirma uma operação,
Esc cancela; Ctrl+Z/Ctrl+Y desfazem/refazem inclusive as poses dos filhos e pivôs.
Comparações geométricas usam tolerância proporcional à precisão float, sem
exigir identidade textual de quaternions normalizados.

Limites: encaixe de face/borda/vértice somente em caixas. Seleções que contêm
esferas e esferas sob o cursor exibem indisponibilidade; nunca são tratadas
como caixas invisíveis. Movimento livre, picking esférico e escala uniforme
continuam disponíveis. Para transformar um conjunto, agrupe como Model.
Fora do modo de encaixe não há bloqueio global de sobreposições.

Testes: `linblux_studio` cobre contato, restrições, rotações, dimensões, pivô,
filhos e histórico. `linblux_gizmo_visual` verifica o marcador renderizado e os
gizmos existentes; gera capturas em 1280×720 e 1920×1080 em diretório temporário.

## Leitura da construção e seleção

As arestas começam desligadas. Visualizar → Arestas/F8 ativa guias respeitando a profundidade da cena; não altera
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
