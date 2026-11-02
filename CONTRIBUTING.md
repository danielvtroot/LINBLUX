# Contribuir com LINBLUX

A Alpha aceita relatos reproduzíveis, correções pequenas, documentação e exemplos
com direitos claros. Antes de trabalhar, consulte [Arquitetura](docs/ARCHITECTURE.md) e os contratos da área afetada.

## Reportar problemas

Abra uma issue com commit/versão, distribuição Linux, GPU/driver e passos para
reproduzir. Descreva o resultado esperado e o observado. Anexe logs e uma cena
mínima sem dados privados; confirme direitos das imagens e sons. Vulnerabilidades
sensíveis seguem [SECURITY.md](SECURITY.md).

## Preparar uma mudança

1. Confira `git status -sb` e preserve trabalho local de outras sessões.
2. Use o [ATLAS](docs/atlas/README.md) para localizar a área e confirmar no código.
3. Configure e compile em `build-linblux/`, conforme [BUILD](docs/BUILD.md).
4. Faça a menor alteração suficiente e execute os testes afetados.
5. Revise o diff, atualize o manual pertinente e envie um Pull Request com evidências.

Preserve C++17, IDs persistentes e o isolamento entre autoria e execução. Use
ProjectIO para cenas, ExperienceManager para a biblioteca e RuntimeSession para
Play/Stop. A UI não deve duplicar serialização ou regras de física. Textos novos
usam o catálogo pt-BR; cores usam o tema central. Preserve licenças e proveniência.

## Validar

```sh
cmake --build build-linblux -j2
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
python3 tools/turbo/turbo.py test --mode impact --changed --plan
```

Testes gráficos sempre usam o TURBO, com orçamento e timeout. Veja [TESTING](docs/TESTING.md).
Em mudanças visuais, inspecione capturas reais em 1366×768 e 1920×1080 e teste
interação. Não rode FULL automaticamente e não confunda cobertura parcial com
aprovação global. Ao mudar código, atualize o índice com `python3 tools/atlas/atlas.py update`.

Não envie builds, caches, logs, segredos, mídia privada ou experiências pessoais.
Adicione caminhos explicitamente depois da revisão. Não reescreva histórico nem
force push. Padrões em `assets/defaults/` são recursos autorais versionados e
exigem a mesma revisão que código.

## Localização e tema

`assets/ui/locales/pt-BR.json` centraliza traduções da interface. `i18n::Text`,
`Label` e `Markup` traduzem apresentação; IDs RML, atributos de comandos, nomes
de arquivos e APIs não são alterados. RML estático usa pt-BR. Novas mensagens
devem entrar no catálogo e ser apresentadas por essas funções. Erros nativos
externos podem conter terminologia da biblioteca ou do sistema operacional.

`assets/ui/theme.json` é a fonte da paleta compartilhada entre ImGui e RmlUi.
Execute `python3 scripts/generate-theme.py` após editá-la e registre os arquivos
gerados. `--check` valida a consistência. O tema usa preto/grafite, translucidez
por mistura alfa (sem blur), texto claro e destaque vermelho.
A transparência dos painéis é composta dentro da janela; não é transparência
do compositor da área de trabalho.

## Investigação incremental e proteção dos dados

Em HDD mecânico, use o modo de baixo I/O do ATLAS:

```sh
ATLAS_IO_MODE=hdd-safe python3 tools/atlas/atlas.py diagnose "sintoma" --brief
python3 tools/atlas/atlas.py symbol NomeDoSimbolo
python3 tools/atlas/atlas.py impact src/modulo/arquivo.cpp
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.cpp
```

Confirme os candidatos no código real: relações textuais não provam chamadas
nem cobertura. Separe fatos, hipóteses e ausência de evidência. Não execute
scripts apenas porque apareceram no índice. O índice é cache reconstruível;
código, CMake, testes e contratos são a fonte da verdade. Em `hdd-critical`,
não execute update/verify/bench/docs. Evite benchmarks, cópias amplas e baterias
repetidas; faça uma atualização final do índice após alterar código.

Antes de editar, confira HEAD local/remoto e alterações pendentes. Preserve
experiências, scripts, mídias pessoais, `.lo`, backups, licenças e créditos.
Arquivo desconhecido deve ser protegido até esclarecer seu papel. Não use
`git reset --hard`, `git clean -fd`, force push ou limpeza indiscriminada.
Limite operações à raiz do projeto, exceto testes isolados explicitamente
planejados. Consulte [Backup e restauração](docs/BACKUP_E_RESTAURACAO.md).

Edit não simula queda; Play executa uma cópia da cena; Stop preserva a autoria.
Move/Scale por face, Snap e undo/redo são contratos existentes. Preserve IDs,
hierarquia, formatos e APIs Lua legados. Não refaça sistemas aprovados para
resolver um problema de outra área.

CTest direto é reservado a testes headless confirmados. Testes gráficos,
Xvfb/Mesa e integrações visuais passam pelo TURBO, com orçamento e timeout:

```sh
python3 tools/turbo/turbo.py test --mode impact --changed --budget 300 --per-test-timeout 180
```

`PASSED_PARTIAL`, `BUDGET_EXHAUSTED`, `UNKNOWN`, `NO_TESTS`, `REUSED` e `TIMEOUT`
não são aprovação plena. Registre testes realmente executados, limitações e
validações manuais pendentes. Encerre quando a regressão e a integração afetadas
estiverem suficientemente verificadas.
