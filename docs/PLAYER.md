# LINBLUX Player

O Player executa experiências locais sem carregar a interface do Studio.
Depois de compilar no [build oficial](BUILD.md):

```sh
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
./build-linblux/linblux_player --project /caminho/experiencia
./build-linblux/linblux_player --validate /caminho/cena.loblox.json
./build-linblux/linblux_player --help
```

`--validate` verifica carregamento e não substitui executar scripts ou inspecionar
renderização. `--check-scripts` faz a execução terminar com erro quando há erros
Lua; `--frames N` limita quadros para smoke tests. A qualidade pode ser escolhida
com `--quality low`, `medium`, `high` ou `legacy`.

WASD move o personagem, espaço pula, botão direito controla a câmera. Esc abre
um menu que captura input enquanto a simulação continua. Itens usam E para
coletar, 1–9 para selecionar e Delete para soltar; ações específicas dependem dos
scripts da experiência. RifleDemo usa clique para atirar e R para recarregar.

A execução não regrava a cena autoral. Inventário de partida não é salvo
automaticamente entre sessões. Não há multiplayer, contas ou servidor online.
Use experiências confiáveis: a API Lua acessa capacidades locais da engine.
