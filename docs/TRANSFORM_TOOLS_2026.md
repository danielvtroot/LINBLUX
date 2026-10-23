# Ferramentas de transformação — 2026-10-23

Base: `4185c00`. Escopo: Move, Scale e Rotate do editor 3D. Câmera de
gameplay, Shift Lock, câmera FPS/Focus, física, Lua, menu Esc, Health, céu,
animações e rosto R6 preservados. Referências consultadas (implementação C++
própria, nenhum código ou recurso copiado): Next-Gen Draggers
(devforum.roblox.com/t/4703155), Parts e Models (create.roblox.com/docs),
Pivot Tools e Dragger QoL (devforum.roblox.com/t/2661668).

## O que já existia e foi preservado

O picking, o layout de tela, o snapping por delta inicial, os seis handles de
escala, os anéis de rotação, os planos XY/XZ/YZ e a suspensão do corpo físico
já estavam implementados em `Gizmo{,Math,Layout}` e `TransformSession`. Nada
disso foi reescrito: a correção de picking (mesmo `Layout` para desenho e hit
test), o fallback para projeção em tela quando o eixo fica de frente para a
câmera, e a prioridade de input (modal → drag → UI → seleção → câmera)
continuam sendo a base.

## O que faltava e foi implementado

### 1. Medidas reais durante o arraste (`editor/TransformMetrics`)

Um `Context` imutável é criado no instante em que o arraste começa e guarda:

- a grandeza medida (Move, MovePlane, Size, Angle);
- o eixo, o lado da face e o espaço (World/Local);
- a transformação inicial completa.

A medida é sempre `f(pose inicial, pose atual)`, nunca um acumulador por
frame. Isso elimina dependência de FPS e drift, e mantém a mesma regra do
snapping: quantiza-se o delta desde o início do arraste.

Textos mostrados no viewport:

```
X  +3.50 studs          Move no eixo X
X +2.00 | Z -1.00 studs Move no plano XZ
Size X: 8.50 studs      tamanho final da dimensão
Y  +45.00 deg           giro aplicado
```

O chip é desenhado junto da alça ativa e permanece 3 s após soltar o mouse.
Ele é desenhado e testado por retângulo próprio (não por um item ImGui sobre o
botão que cobre a viewport), então clicar no valor nunca seleciona outro
objeto nem inicia movimento de câmera.

### 2. Edição numérica pós-drag

Clicar no chip abre um campo de texto. Enter aplica, Esc cancela (e clicar
fora também cancela sem alterar nada).

- Move: digitar `4` posiciona a peça exatamente 4 studs à frente da pose em
  que o arraste começou — não a partir do frame anterior.
- Scale: digitar `8.5` define o tamanho final dessa dimensão; a face oposta
  permanece estável (ou o centro, com Alt).
- Rotate: digitar `45` aplica 45° sobre a rotação inicial, sobre o eixo ativo.

Movimento em plano tem dois graus de liberdade; o chip mostra os dois eixos e
o campo de digitação fica indisponível (com dica explicando), para não
inventar o segundo eixo.

### 3. Escala centrada (Alt)

Segurar Alt no início de um arraste de Scale mantém o centro da Part parado e
afasta as duas faces (`gizmomath::ScaleCenteredFace`). A escolha é fixada no
início do arraste; soltar Alt no meio não troca a matemática. O valor digitado
depois respeita o modo usado no arraste.

Shift continua desligando o Snap temporariamente, como antes.

### 4. World | Local visível e real

- Controle segmentado `World | Local` na ribbon Home, ao lado das ferramentas.
- Atalho `Ctrl+L` (não conflita com 1–4, F, Ctrl+S/O/D/C/V, Delete, F2, F5,
  F10/F11) e tooltip com o atalho.
- Alternar encerra o arraste atual de forma segura e limpa o chip.
- A escolha muda o cálculo: `gizmomath::Axis(..., local)` alimenta picking,
  desenho, movimento, escala e rotação; o anel e as faces acompanham a Part
  rotacionada.

### 5. Undo/Redo de um arraste por operação (`editor/EditHistory`)

- `Ctrl+Z` restaura e `Ctrl+Y` (ou `Ctrl+Shift+Z`) refaz.
- Cada arraste completo grava **uma** entrada (`antes`/`depois`), nunca uma por
  frame. Uma edição numérica também é uma entrada.
- Clicar num handle sem arrastar não cria operação (`SameTransform` compara
  posição, tamanho e orientação, tratando `q` e `-q` como a mesma pose).
- O histórico local tem limite de 128 entradas; operações sobre peças que não
  existem mais são descartadas com aviso no status.
- Ao aplicar Undo/Redo, o corpo físico recebe `physics.applyChanges` e as
  invariantes de cena/física são checadas.

### 6. Ticks de Snap

Durante um arraste de Move em um eixo, com Snap ativo, marcas perpendiculares
são desenhadas em cada incremento (mais longas a cada 5). São apenas guias
visuais, desenhadas no mesmo passe do gizmo, sem custo de raycast.

### 7. Properties sem notação científica

`Num` (frontend RmlUi) passou a usar `core/NumberFormat.h`:

- `|v| < 1e-4` aparece como `0` (o caso `-9.7066e-12` da screenshot);
- até 4 casas, sem zeros à direita e sem notação científica;
- o valor interno não é alterado — a edição por texto continua enviando o
  número exato digitado;
- `Parse` aceita apenas um número finito completo (rejeita `4studs`, `nan`,
  `1e999`).

## Integração com o Rig Editor

O Rig Editor já reutiliza `Gizmo`, `GizmoMath` e `GizmoLayout` para desenho,
picking, snap e estado de arraste; nada disso foi duplicado. A semântica R6
(juntas, bind pose, offsets, keyframes) permanece no editor de rig, que tem
seus próprios campos numéricos. O chip numérico do viewport foi ligado apenas
a Parts; mover juntas como Parts independentes continua proibido pelo fluxo
existente.

## Testes

Headless (`./build/loblox_studio_tests`): 1518 checks, 0 falhas. Cobre
medida/aplicação de Move (World e Local, positivo e negativo), tamanho final
com face oposta estável em Part rotacionada, mínimo `kMinPartSize`, World
mapeando para o componente local correto, escala centrada, ângulo com sinal
(45°, -45°, 90°, wrap 190° → -170°, `q`/`-q`), plano XY, formatação/parse,
Undo/Redo (uma operação por arraste, redo descartado por edição nova, limite).

X11 real (`loblox_editor_input`, llvmpipe): 392 checks, 0 falhas. Cobre chip
durante e depois do arraste, clique no chip, digitação de `4` studs,
`8.5` studs e `45` graus, Alt com centro parado, Ctrl+Z/Ctrl+Y (uma operação
por arraste e por edição), Ctrl+L alternando a matemática (eixo local de uma
Part girada 90°), câmera imóvel durante drag e durante a edição, perda de foco
e cancelamento com Esc.

Regressão: 21/21 CTest aprovados em execução completa (`-j2`), incluindo
`loblox_rml_ui` (457 s com llvmpipe), `loblox_gizmo_visual`,
`loblox_studio_ui`, `loblox_game_ui` e `loblox_editor_input`. O teste
`loblox_editor_restart` estourou o timeout de 15 s quando rodou junto do
`loblox_rml_ui` sob llvmpipe e passou isolado (4 + 2 lançamentos do binário
real, transformações e propriedades preservadas após salvar/reabrir).

## Limites conhecidos

- Movimento em plano mostra os dois eixos, mas não aceita valor digitado.
- O chip é screen-space, como o resto do overlay; ele não é ocluído pela peça
  (decisão existente para as alças continuarem clicáveis).
- Não existe editor de pivô de Model. Selecionar um Model continua sem gizmo;
  transformar Parts filhas uma a uma preserva a hierarquia, mas não há pivô de
  conjunto nem escala de Model.
- Escala em Sphere é uniforme (diâmetro), como antes; o chip mostra esse
  diâmetro.
- Ticks de Snap aparecem apenas no arraste axial de Move.
- Nada foi validado visualmente por captura de tela; a validação é funcional
  (pixel/hit test no `loblox_gizmo_visual` e eventos X11 reais no
  `loblox_editor_input`). Fluidez na Vega 8 física continua pendente.
