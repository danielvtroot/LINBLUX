# LINBLUX Demo / RifleDemo

Abra `world.loblox.json` no Studio e clique Executar, ou use:

```sh
linblux_player --project assets/examples/rifledemo/world.loblox.json
```

Aproxime-se do item, segure E, pressione 1 para equipar, clique para disparar,
R para recarregar e Delete para soltar. E recolhe a mesma instância com sua
munição atual. Esc abre o menu protegido. Há três alvos com rig/vida/respawn,
um guia animado em patrulha e uma placa com a logo oficial.

O item é um Modelo de Parts; BocaDoCano é um Attachment da Part Cano.
`Item.lua` implementa ações, recoil, raycast/dano e UI pelas APIs públicas.
`Demo.lua` monta o cenário e possui os efeitos temporários: sua limpeza continua
quando o Script do item é removido. `tools/examples/build_rifledemo.py` embute
as fontes no mundo e gera os cinco sons originais de síntese (licença MIT do
projeto). A logo é uma cópia exata de `midia/logo.png`.

Áudio usa IDs da biblioteca, preservados na importação/exportação, com streaming
raylib e `owner` do item. Sem dispositivo de áudio a demonstração fica silenciosa
explicitamente. Não há rede/multiplayer nem simulação balística: dano é por
raycast, e o traçante visual é uma Part física sem colisão. A orientação do cano
acompanha o Modelo; esta demo usa mira horizontal, sem sistema FPS completo.

Regerar: `python3 tools/examples/build_rifledemo.py`; conferir: mesmo comando
com `--check`. Testes: `linblux_item_actions`, `linblux_rifledemo` e
`linblux_rifledemo_player` (gráfico supervisionado pelo TURBO).
