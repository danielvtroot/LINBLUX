# Continuidade — Experiences 1.0

Base: c0ed911; worktree inicial sem mudanças rastreadas. Untracked do usuário
preservados (Testing, assets PNG, referencias, labubu.png, s.png).

Implementados: ExperienceManager/manifest/revisões atômicas, assets relativos,
templates reais, My Default/Character, `.lo` binário com limites e staging,
JPEG (raylib local não tinha decoder), Home/wizard/details RmlUi, diálogos
nativos assíncronos, loading com future e preparação isolada, File/Back Home.
Factory resources são JSON versionados; não são templates pessoais compilados.
Builtin face foi incluída no pacote e resolvida por experiência sem mudar UV.

R6: seleção do rig inteiro agora manipula root por proxy no centro visível do
torso, não restPose do Torso. Scale uniforme preserva conexões e acompanha
capsule dimensions quando é o default player. Renderer usava parentAnchor
antes da pose para a linha amarela; agora deriva o ponto da junta posada.
Center internal root preserva geometria e keys; runtime normaliza só sua cópia.

Testes no primeiro checkpoint: core Experiences 110 verificações/0 falhas;
GUI integrada passou, incluindo PNG/JPEG via Yad, Move real +5X/+3Y, Rotate,
defaults, rename, loading, Play/Stop, export/import, Delete/Archive/Restore.
GUI: /tmp/loblox-experience-ui-fc9abee39adbc713c4ac0d7c5983af7d.
Capturas Home 1280/1366/1600/1920, wizard, native chooser, loading, rig e card
menu foram abertas e inspecionadas. Sidebar flex foi corrigida com spans.

Regressão de 25 testes está em execução: /tmp/loblox-exp-ctest-final.log.
Build completo atualizado. Instalação de teste /tmp/loblox-experience-install,
executada de diretório vazio, mostrou Home e templates. Capturas:
/tmp/loblox-experiences-main-f_98khap. RSS Home 208568 kB sob llvmpipe;
não é medição no notebook/Vega/Sway. Não editar fontes durante build.

Busca/paginação, menu contextual, localizar entradas movidas e instalação já
implementados. Próximo: finalizar regressão, ajustar crop de thumbnails muito
largas, testar importação com outro HOME em processo real, métricas isoladas
e sanitizer do núcleo quando disponível. Revalidar o principal após mudanças.
Crop interativo opcional não implementado. Native helpers: zenity/yad/kdialog.
Exclusão é recuperável no Archive, sem remoção permanente automática.
`robloxs.png` e referenciaspngs não existem no checkout; pergunta de caminho
foi enviada. Não alegar comparação com uma imagem que não foi recebida.

Comandos: `cmake --build build --target loblox -j2`;
`./build/loblox_experience_tests`;
`ctest --test-dir build --output-on-failure -j2`;
`cmake --install build --prefix /tmp/loblox-experience-install --component Loblox`.
Sem push, reset ou clean. A missão não está concluída neste documento.
