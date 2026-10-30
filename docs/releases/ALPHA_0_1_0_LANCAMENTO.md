# LINBLUX Alpha 0.1.0 — relatório de lançamento

Data: 30/10/2026. Código/documentação em publicação incremental; binário bloqueado.

## Código

Explorador compacto, grade visual WIP removida e Snap preservado. Sliders de áudio
validados. Move/Scale não foram refeitos; Decal não teve UVs alteradas.

## Documentação

README pt-BR/inglês, logo real, captura real de construção, índice de manuais,
galeria de mídia, guias de build/Player, contribuição e templates preparados.
Nenhum relatório histórico foi apagado. Nenhum vídeo foi adicionado.

## Distribuição

Build oficial `build-linblux/`, CMake 0.1.0; Studio e Player compilados.
Empacotamento e smoke do candidato serão registrados no próximo incremento.
Binário e Release pública bloqueados por obrigações FFmpeg GPL e avisos dos
componentes embutidos da raylib ainda incompletos. Não foi criada tag.

## Git

HEAD inicial: `ef53733b5766b0ead44146ae9355bccb5051c406`.
Primeiro incremento: `eaf38203174394057e040fff3ed00c2e992b3eb4`, push confirmado.
Estado inicial e patch em `.turbo/alpha-launch-initial/` (somente local).
`backup/wip-atlas21` existe localmente, sem alegação de backup remoto.
WIPs de Scene e relatórios não relacionados preservados. Nenhum projeto pessoal removido.

## Testes

Build incremental completo: PASSOU. Sete headless de núcleo: PASSOU.
TURBO: construção, Decal, física UI, áudio RmlUi/nativo nas duas resoluções: PASSOU.
Explorador, RifleDemo Player e menu Player: PASSOU após ajustes nas fixtures.
Imagem e RifleDemo headless: PASSOU. Gerador de mídia: 4 testes PASSOU.
Tentativas anteriores do Explorador/RifleDemo falharam e foram investigadas;
os resultados finais não apagam essas falhas. Logs em `.turbo/logs/alpha-launch-*/`.
FULL, hardware gráfico/áudio físico e outra instalação Linux: NÃO EXECUTADO.

## Próximas versões

Concluir obrigações de redistribuição e ampliar validação Linux; depois consolidar
Studio/renderização, personagens, importação 3D e API Lua conforme o roadmap.
