# Testes e evidências

Use o build oficial configurado conforme [BUILD](BUILD.md):

```sh
cmake --build build-linblux -j2
ctest --test-dir build-linblux -N
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

A consulta `-N` não executa testes. Os quatro testes selecionados acima são
headless: objetos, Lua, física, persistência, padrões e ciclo de autoria/runtime.
Outros testes devem ser escolhidos pelo contrato alterado e suas labels.

## Integrações gráficas

Nunca execute diretamente CTest para testes com label `graphics`, Xvfb ou Mesa
por software. Use supervisão TURBO para timeout individual, orçamento global,
encerramento do grupo de processos e logs completos:

```sh
python3 tools/turbo/turbo.py test --mode fast --changed --plan
python3 tools/turbo/turbo.py test --mode impact --changed --budget 300 --per-test-timeout 130
python3 tools/turbo/turbo.py report
```

`--plan` mostra a seleção e o que fica fora sem executar. FAST/IMPACT não prometem
cobertura total. `PASSED_PARTIAL`, `BUDGET_EXHAUSTED`, `UNKNOWN`, `REUSED`,
`NO_TESTS` e `TIMEOUT` não são aprovação plena. Não repita baterias pesadas
quando build, regressão específica e integração relevante já fornecerem evidência.

As integrações usam Xvfb/Mesa e, conforme o teste, xdotool, Pillow e seletores
nativos. A ausência de ferramentas pode impedir o registro de testes: teste
não registrado ou não executado nunca conta como aprovado. Capturas em llvmpipe
não medem desempenho de GPU física; backend de áudio null não comprova som audível.

## Ferramentas locais

```sh
python3 -m unittest discover -s tests/atlas -p 'test_*.py'
python3 -m unittest discover -s tests/turbo -p 'test_*.py'
python3 scripts/generate-theme.py --check
```

Execute as suítes das ferramentas quando elas forem alteradas. O [ATLAS](atlas/README.md)
ajuda a encontrar testes, mas associações estruturais não provam execução do
comportamento. O [relatório da Alpha](releases/ALPHA_0_1_0_LANCAMENTO.md) registra
somente as verificações desta missão; relatórios antigos valem para suas revisões.
