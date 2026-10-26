# Modelos e transformações

Um Modelo contém Parts, outros Modelos e Scripts reais da cena, com IDs estáveis.
No Explorador, Ctrl+clique seleciona várias peças. Ctrl+G agrupa e Ctrl+Shift+G
desagrupa; a aba Modelo também oferece esses comandos. Os menus nativos possuem
as mesmas operações. Agrupar/desagrupar preserva as transformações mundiais dos
filhos. Objetos protegidos, ciclos e seleção de pai/filho sobrepostos são recusados.

As Propriedades permitem mover e girar o conjunto e aplicar escala uniforme.
O pivô é editável sem deslocar as peças. Modelos antigos sem pivô explícito usam
o centro das caixas envolventes de suas Parts; ao definir/transformar o pivô,
a referência fica persistida. Escala é positiva (0,001–1000), sem reduzir uma
peça abaixo do mínimo da engine (0,2). A validação ocorre antes de alterar os
filhos. Modelos aninhados preservam suas referências quando o pai muda.

A seleção múltipla é estado do editor, não conteúdo salvo. Peças dentro do Modelo
continuam editáveis individualmente. Clonagem copia filhos, scripts, componentes
e pivôs com novos IDs. A cópia da lista de filhos evita invalidar iteradores
quando a clonagem de uma Part altera temporariamente seu Parent.

## Lua

- `linblux.scene.create("model", {name=..., parent=..., position=..., rotation=..., scale=...})`.
- `linblux.scene.group({pecaA, pecaB}, "Nome")` cria o grupo (1–256 objetos).
- `modelo.position`, `modelo.rotation`, `modelo.scale`: posição/rotação mundiais
  e escala uniforme absoluta; atribuir transforma o conjunto.
- `modelo.transform` / `:get_transform(referencia?)` / `:set_transform(tabela, referencia?)`.
  Tabela contém position, rotation e scale. Referência opcional: Part ou Modelo.
- `modelo.pivot` / `:get_pivot(referencia?)` / `:set_pivot(tabela, referencia?)`: ler
  o mesmo referencial e editar o pivô sem mover a geometria; set_pivot não altera escala.
- `:point_to_world(v)`, `:point_to_local(v)`, `:vector_to_world(v)`,
  `:vector_to_local(v)` incluem rotação e escala do Modelo.
- `:scale_by(numero)`, `:clone()`, `:destroy()`, `:ungroup()` e
  `:add_script(fonte, nome?)` usam objetos/componentes reais.

Rotação da API nova de Modelos é Euler ZYX em graus, usando o par consistente
já existente na engine. APIs legadas de Parts continuam com sua convenção.
Não há escala não uniforme com cisalhamento. Até 32 operações custosas de Modelos
por quadro compartilhadas pelos scripts; impede loops de transformações nativas
sem limite. Leitura de um pivô explícito é constante; calcular pivô de Modelo
legado entra na cota. O exemplo modelos.lua é executado no Studio e Player.

## Persistência e compatibilidade

Cenas que usam pivô explícito adotam JSON v4. Model.modelPivot contém position,
rotation em quaternion [x,y,z,w] e scale. Parts dessas cenas incluem orientation
em quaternion, que é autoritativa; rotation Euler é mantida para inspeção.
Isso preserva rotações de 90° e composições sem converter repetidamente para
Euler. Não alteramos a interpretação dos arquivos antigos: v1/v2/v3 continuam
lidos com as regras anteriores. Cenas sem pivô explícito mantêm a versão anterior.
A migração ocorre no salvamento explícito; abrir não regrava o arquivo pelo
ProjectIO. Leitores anteriores ao v4 recusam a nova versão. Pacotes .lo usam
o mesmo documento e preservam IDs, hierarquia e referências de recursos.

Agrupar não cria corpo rígido ou solda automaticamente. Parts dinâmicas ainda
são simuladas individualmente; vínculos físicos e Attachments são a próxima
etapa. Mover, girar e escalar usam os mesmos gizmos das Parts, aplicados ao
conjunto. A escala do Modelo é uniforme, inclusive ao arrastar uma alça de eixo.
O pivô e as transformações pelas Propriedades também participam de Ctrl+Z/Ctrl+Y.

Um arraste inteiro gera uma única operação de histórico, com as poses das Parts
e os pivôs dos Modelos aninhados. Cancelar restaura o estado inicial exato,
inclusive pivôs implícitos antigos. Durante o arraste, os corpos dos filhos ficam
em edição; o auxiliar do gizmo não cria corpo Bullet. Referências removidas ou
retiradas da hierarquia invalidam a restauração antes de alterar qualquer peça.
O histórico limita-se a 128 operações e 32.768 registros de poses/pivôs.
Agrupar/desagrupar e outras mudanças de topologia ainda não têm undo estrutural.
