# Contribuir com LINBLUX

A Alpha aceita relatos reproduzíveis, correções pequenas, documentação e exemplos
com direitos claros. Antes de trabalhar, consulte [AGENTS.md](AGENTS.md),
[Arquitetura](docs/ARCHITECTURE.md) e os contratos da área afetada.

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
