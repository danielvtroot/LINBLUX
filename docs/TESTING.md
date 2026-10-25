# Validação

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Suítes de núcleo verificam objetos, persistência transacional, física, Lua,
spawn, animação, manipulação e experiências. `linblux_defaults` verifica edição,
persistência, cópias independentes e restauração dos padrões. `linblux_video`
percorre o clipe completo (~24 s) e verifica loop, cancelamento e fallback.
`linblux_home_video` renderiza em múltiplas resoluções, confere movimento nas
bordas, interação, liberação no editor, confirmação de exclusão e padrões na UI.

Os testes gráficos usam Xvfb/Mesa; testes de interação usam xdotool e seletor
nativo (zenity/yad/kdialog). Ausência dessas ferramentas pode remover testes da
configuração: consulte `ctest --test-dir build -N`, não confunda ausência com sucesso.

```sh
ctest --test-dir build -R 'experiences|defaults|home_video|video' --output-on-failure
python3 scripts/generate-theme.py --check
```

Capturas do executável e os relatórios em relatorios/ complementam os testes.
Medições em llvmpipe são de rasterização por CPU; desempenho de GPU integrada real
permanece uma verificação separada. Fixtures antigas receberam atualização de
marca e geometria: não se deve aprovar apenas por preservar aparência da marca anterior.

O teste `linblux_engine_default_lifecycle` cobre o ciclo de autoria/reinício em
processos separados, importação, runtime e proteção do padrão. O teste
`linblux_engine_default_restart` reinicia o Studio real e usa o cartão e os botões
da interface, exigindo Pillow (`python3-pil`). As suítes legadas usam os títulos
traduzidos das janelas; a verificação da logo compara a proporção do próprio
arquivo oficial, sem impor a silhueta da marca anterior.
