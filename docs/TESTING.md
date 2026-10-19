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
147 checagens, 0 falhas
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
| Escrita atômica | arquivo final existe, `.tmp` não permanece |
| Validação | JSON inválido, versão 99, Lighting ausente, id duplicado, tamanho negativo, pai inválido, arquivo inexistente |
| Carga + física | corpos são reconstruídos; peça carregada cai e colide |
| Limpeza | `clearObjects` remove Parts e corpos, mantém a cena utilizável |

## 4. Smoke test gráfico

Foi executado com `Xvfb` (1280×720×24), `xdotool` para mouse/teclado e `scrot`
para capturas. A aplicação foi iniciada com renderização por software.

Procedimento usado (reproduzível em uma máquina com Xvfb):

```bash
Xvfb :99 -screen 0 1280x720x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/loblox &

# clique longo o suficiente para atravessar frames (llvmpipe é lento)
xdotool mousemove 100 128 mousedown 1
sleep 0.5
xdotool mouseup 1              # Toolbox > Block
sleep 2
xdotool key ctrl+s             # salva projects/default.loblox.json
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
- **Save**: `Ctrl+S` criou `projects/default.loblox.json` com
  `"format": "loblox-project"`, `"version": 1`.
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

## 5. O que não foi verificado

- Execução em Loc-OS e Sway reais (apenas Xvfb neste ambiente).
- GPU física, consumo de memória e uso de CPU no notebook de destino.
- Redimensionamento interativo da janela por um gerenciador de janelas real.
- Sessões longas, estabilidade com milhares de peças e autosave.
- Acessibilidade, atalhos de teclado além dos documentados e internacionalização.

Essas limitações estão registradas também em `docs/PROGRESS.md` e no README.
