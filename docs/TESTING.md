# Testes e verificação

## 1. Ambiente usado nesta etapa

- Linux (contêiner de desenvolvimento), GCC com C++17, CMake 3.28.
- raylib 6.2.0 instalada em `/usr/local` (`pkg-config --modversion raylib`).
- Bullet 3.24 (`libbullet-dev`).
- Sem GPU: a verificação gráfica usou `Xvfb` + `LIBGL_ALWAYS_SOFTWARE=1`
  (llvmpipe). Isso mede o pior caso de rasterização por CPU e **não** representa
  o desempenho no notebook de destino com GPU.
- Não foi possível testar em Loc-OS/Sway reais nem em uma GPU física.

## 2. Compilar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Resultado: compilação concluída sem erros. Os alvos do projeto usam
`-Wall -Wextra` e não emitiram warnings na última compilação.

## 3. Testes automatizados (headless)

```bash
ctest --test-dir build --output-on-failure
```

Saída real:

```
Test project /home/labubu/projeto/loblox/build
    Start 1: loblox_tests
1/1 Test #1: loblox_tests .....................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 1
```

Executando o binário diretamente:

```bash
./build/loblox_tests
```

Saída real (final):

```
149 checagens, 0 falhas
RESULTADO: OK
```

### 3.1 Cobertura da suíte

| Área | Verificações |
| --- | --- |
| Cena padrão | Workspace/Lighting, Baseplate + SpawnLocation, ids, pais, proteção |
| Criação/remoção | ids únicos, nomes (`Part`, `Part2`...), seleção limpa, contagem |
| Listener de remoção | dispara para Parts, não para serviços |
| Propriedades | posição, tamanho (clamp 0.2), rotação, cor, material, flags |
| Anchored ↔ dinâmico | estático não cai; dinâmico cai e repousa em y≈2; alternância para o movimento; volta a cair |
| CanCollide | corpo sem colisão atravessa a Baseplate (y < −5); corpo sólido repousa |
| Sincronização | `setPosition` move o corpo; corpo move a Part; raio acerta a Part |
| Raycast | acerta a peça na posição certa; deixa de acertar após mover/excluir |
| Corpo único | `addPart` duas vezes não duplica; corpoCount consistente |
| Redimensionamento | `setSize` mantém um corpo e a peça repousa com a nova altura |
| Baseplate | permanece imóvel; pode ser movida manualmente; não pode ser excluída |
| JSON | round-trip completo, ids, hierarquia, cor, material, lighting, nextId |
| Escrita atômica | final existe, temporário único removido; falha de rename preserva o destino anterior |
| Validação | JSON inválido, versão 99, Lighting ausente, id duplicado, tamanho negativo, pai inválido, arquivo inexistente |
| Carga + física | corpos são reconstruídos; peça carregada cai e colide |
| Limpeza | `clearObjects` remove Parts e corpos, mantém a cena utilizável |

## 4. Smoke test gráfico

Foi executado com `Xvfb` (1280×720×24), `xdotool` para mouse/teclado e `scrot`
para capturas. A aplicação foi iniciada com renderização por software.

Procedimento usado (reproduzível em uma máquina com Xvfb):

```bash
XDG_DATA_HOME=/tmp/loblox-test-data Xvfb :99 -screen 0 1280x720x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 XDG_DATA_HOME=/tmp/loblox-test-data ./build/loblox &

# clique longo o suficiente para atravessar frames (llvmpipe é lento)
xdotool mousemove 100 128 mousedown 1
sleep 0.5
xdotool mouseup 1              # Toolbox > Block
sleep 2
xdotool key ctrl+s             # salva no caminho absoluto do XDG_DATA_HOME
```

### 4.1 Resultados observados

- Janela abre e desenha: toolbar, Toolbox, viewport, Explorer e Properties nas
  posições esperadas; céu azul visível no topo da viewport; Baseplate com studs
  e grade; toolbar sem botões Play/Test/Stop.
- **Criar Block** pelo Toolbox: Part surge e cai até repousar sobre a
  SpawnLocation (centro y ≈ 3.0), `anchored: false` no JSON.
- **Criar Sphere** e **SpawnLocation**: ambos aparecem na cena e no Explorer;
  segundo SpawnLocation recebe nome único (`SpawnLocation2`).
- **Selecionar** na viewport: Explorer/Properties passam a mostrar a Part;
  alterações seguintes aplicam-se a ela.
- **Anchor**: alterna `anchored` para `true` no JSON; a peça para de cair.
- **Move**: arrasto no gizmo alterou X de 0 para −1,79 no JSON.
- **Scale**: arrasto no eixo X alterou `size.x` de 4 para 15,53, mantendo um
  único corpo físico.
- **Rotate**: arrasto no anel Y produziu rotação de ≈ 23,55° em Y no JSON.
- **Delete**: removeu a Part selecionada; o JSON voltou a conter apenas
  Baseplate/SpawnLocation.
- **Color**: paleta aplicou verde `[80,190,90,255]` à Sphere.
- **Material**: menu aplicou `Neon` à Sphere.
- **Save**: `Ctrl+S` criou o documento JSON v1 (naquela execução o caminho era
  relativo ao cwd; a seção 7 registra a correção e a reprodução em cwd externo).
- **Open**: `Ctrl+O` abriu o modal; o botão **Abrir** carregou o arquivo salvo
  e restaurou o Block que havia sido substituído por uma Sphere na cena atual.
- **Redimensionar janela**: o layout acompanha `WorkSize`; a render texture é
  recriada quando o painel muda de tamanho (verificação visual, sem medição
  automatizada de FPS).

### 4.2 Desempenho medido (software)

Com llvmpipe em 1280×720, a aplicação ficou entre **9 e 19 FPS**. Isso é
esperado para rasterização por CPU com uma viewport de ~762×638, sombreamento
por triângulos e UI. O valor **não** foi medido em GPU e não deve ser usado como
previsão para o notebook de destino. Não foi feito benchmark de memória.

O limite de FPS é 60 por padrão (`AppConfig::targetFps`) e a física usa passo
fixo de 1/60 s com até 5 subpassos, independente desses 9–19 FPS.

## 5. Redesign da interface — 2026-10-20

Build e suíte executados após as alterações:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Resultado: `loblox` e `loblox_tests` compilaram; CTest passou (1/1 teste,
0 falhas). O rebuild final não emitiu warnings.

Smoke gráfico executado em Xvfb a 1280×720 com llvmpipe. A janela inicializou,
criou o contexto OpenGL e a render texture da viewport (775×538), e permaneceu
aberta até a captura e encerramento controlado. A captura foi convertida de XWD
para PNG e inspecionada. A validação visual confirmou menus e ribbon no topo,
Toolbox à esquerda, viewport ampla ao centro e Explorer/Properties empilhados à
direita; tema claro e seleção azul; céu, Baseplate com studs e SpawnLocation
renderizados. A aba Home contém seleção, transformações, inserção, Anchor, Color,
Material, painéis e Save. A captura final está em `/tmp/loblox-ui-final.png`; ela
também confirma a quebra de linha do texto de ajuda de Properties. Não houve
automação de interação por mouse.

O teste headless cobre engine e serialização, não a UI. Abrir/salvar, resize,
filtros, docking e sincronização visual dos painéis não foram exercitados por
automação nesta etapa; os contratos existentes de `ProjectIO`, cena e física
permanecem sem alteração.

## 6. O que não foi verificado

- Execução em Loc-OS e Sway reais (apenas Xvfb neste ambiente).
- GPU física, consumo de memória e uso de CPU no notebook de destino.
- Redimensionamento interativo da janela por um gerenciador de janelas real.
- Sessões longas, estabilidade com milhares de peças e autosave.
- Acessibilidade, atalhos de teclado além dos documentados e internacionalização.

Essas limitações estão registradas também em `docs/PROGRESS.md` e no README.

## 7. Correção da câmera, gizmos e SceneLua — 2026-10-20

Validação executada após as alterações:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/loblox_tests
```

Resultado: build concluído; CTest passou (1/1); executável reportou **241
checagens, 0 falhas**. Os testes cobrem matemática de movimento nos eixos e
planos, snap, lados positivo/negativo da escala, face oposta fixa, tamanho
mínimo, esfera uniforme, rotação nos eixos, movimento de câmera com delta time,
limites de pitch, zoom/foco, pausa de Bullet durante edição, round-trip Lua,
preservação de IDs/hierarquia/Lighting, validação de dados e rejeição de código
executável/classes/propriedades não suportadas. Os testes JSON v1 continuam
passando.

Smoke gráfico realizado sob Xvfb/llvmpipe em 1280×720. O app inicializou o
backend GLFW/X11, renderizou a viewport e foi encerrado de forma controlada. A
captura `/tmp/loblox-camera-gizmo-lua.png` foi inspecionada e mostra a janela
completa, incluindo ribbon clara, Toolbox, viewport 3D, Explorer e Properties.
Isso é validação visual de inicialização e composição; não é teste automatizado
de captura RMB/teclas nem validação no Sway real.

Não há teste unitário de `DisableCursor`/`EnableCursor`, perda de foco da janela
ou roteamento real de teclado: esses comportamentos dependem do sistema de
eventos GLFW e requerem uma sessão gráfica interativa. O código tem caminhos de
liberação explícitos para soltar botão, sair da viewport, perder foco, cancelar
e destruir a UI. Ainda falta validar esses casos manualmente em XWayland/Sway.

## 7. Auditoria de estabilidade — 2026-10-20

Build e testes finais:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/loblox_tests
```

Resultado: build passou; CTest 1/1 passou; **306 checagens, 0 falhas**. Foram
adicionados testes para seis âncoras de Scale e colisões de ícones projetados,
projeção de escala Global para objeto rotacionado, wrap angular de ±180°,
salvamento e reabertura JSON completa, e falha de rename preservando o
destino.

### 7.1 Interações gráficas executadas

- Xvfb com GLFW/X11 e llvmpipe, 1280×720: criei um Block, selecionei Scale e
  inspecionei `/tmp/loblox-scale-stabilized.png`. Os seis ícones X±/Y±/Z± estão
  separados e fora da peça.
- Arrastei a alça X−; o JSON gravado em um `XDG_DATA_HOME` isolado mudou
  `size.x` de 4 para 6.5 e `position.x` de 0 para −1.25. A face oposta
  permaneceu em X=2.
- Arrastei um anel de Rotate e salvei; a rotação JSON mudou de zero para
  aproximadamente 30° em Z. Isso verifica um caminho gráfico de anel, não os
  três eixos em toda orientação de câmera.
- Pressionei RMB e movi o cursor relativamente; a viewport mudou de ângulo. Ao
  soltar, `xdotool getmouselocation` retornou para X=700/Y=440, posição anterior
  à captura.
- O botão Save abriu o modal com o caminho XDG absoluto preenchido. A operação
  de escala também salvou o arquivo. Reabertura/fidelidade completa é coberta
  pelo round-trip JSON headless.
- Repeti Save com o processo iniciado em `/tmp/loblox-cwd-run` e
  `XDG_DATA_HOME=/tmp/loblox-cwd-save-data`; JSON v1 válido apareceu em
  `.../loblox/projects/default.loblox.json`, sem criar arquivo em
  `cwd/projects/`.

Essa execução usa Xvfb, não um compositor Sway real. Ela valida o backend GLFW
X11, mas não garante captura em todas as configurações de XWayland. Também não
automatiza edição de campos enquanto WASD é pressionado.

## 8. Unificação dos handles — 2026-10-20

Após a correção:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/loblox_tests
```

O build passou, CTest passou (1/1) e a suíte direta reportou **506 checagens,
0 falhas**. Os novos casos exercitam X±, Y± e Z±, com a mesma função usada pelo
editor, em Parts alinhadas e com rotação arbitrária. Para cada lado verificam
que só a dimensão escolhida muda, a face oposta fica fixa, o centro desloca no
sentido correto e o tamanho permanece positivo. Também testam IDs/picking de
cada um dos seis retângulos visuais e clique fora de qualquer handle. Move em
eixos/planos, Rotate e suspensão do corpo físico seguem cobertos pela suíte.

O build foi aberto em Xvfb/llvmpipe 1280×720 e a captura
`/tmp/loblox-unified-initial.png` foi inspecionada: ribbon clara, viewport,
Baseplate e painéis renderizaram. Não foi possível validar drag gráfico: neste
ambiente Xvfb sem window manager, a janela aparece mas `xdotool click` não
acionou o botão Toolbox. Portanto, os seis drags, hover real, anéis por mouse e
visibilidade em Sway/XWayland **não foram testados visualmente nesta etapa**.
O smoke anterior de X− e de um anel, registrado na seção 7, precede esta
correção Global/projeção e não comprova a matriz completa atual.

## 9. Passe x-ray dos gizmos — 2026-10-20

Após vincular o hover/arraste de Move, Rotate e Scale ao `HandleId` comum e
desenhar os controles com depth test localmente desligado, executei novamente
`cmake --build build -j2`, CTest e `./build/loblox_tests`: build concluído,
CTest 1/1 aprovado e 506 verificações sem falha. Não houve nova validação
gráfica de um gizmo selecionado; o smoke visual desta versão continua limitado
à inicialização em Xvfb. A tentativa anterior de clicar pelo `xdotool` não
acionou os widgets sem window manager.

## 10. Overlay visível e drags reais — 2026-10-20

O desenho foi tornado visivelmente screen-space em Move, Rotate e Scale: eixos
e botões X/Y/Z, planos XY/XZ/YZ, anéis grossos com etiquetas e os seis lados
de escala ficam sobre a geometria. Inspecionei as capturas
`/tmp/loblox-move-overlay.png`, `/tmp/loblox-rotate-overlay.png` e
`/tmp/loblox-scale-overlay.png`; os controles aparecem sobre a Part.

Com `xdotool` mantendo o botão pressionado durante o arraste no Xvfb/llvmpipe
(1280×720), confirmei os resultados no editor:

- Move X: `position.x` mudou de 0 para 3.
- Rotate X: a propriedade mostrou `rotation.x = 15°` após o drag no botão X.
- Scale Y+: `size.y` mudou de 4 para 5 e `position.y` de 6 para 6.5; a face
  oposta permaneceu em Y=4.

As capturas após os drags são `/tmp/loblox-rotate-drag.png` e
`/tmp/loblox-scale-drag-check.png`. O teste visual comprova apenas um eixo/anel/
lado em cada ferramenta; os outros lados de Scale têm cobertura de matemática
automatizada, mas não foram arrastados individualmente. Xvfb usa GLFW/X11 e
llvmpipe; Sway/XWayland real continua sem validação.
