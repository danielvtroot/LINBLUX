# Localização e tema

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
