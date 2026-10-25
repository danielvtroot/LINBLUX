# Etapa 2 — identidade

Implementação: projeto CMake LINBLUX; executável `linblux`; bibliotecas
`linblux_core`, `linblux_editor`, `linblux_imgui`, `linblux_rml_gl3`; targets de
teste e instalação atualizados. Logo oficial em janela, Studio e Player, sem
alterar o PNG. Visor original substitui a antiga imagem de rosto distribuída.
Aliases de caminhos e identificadores legados estão em COMPATIBILIDADE.md.
A biblioteca legada é reaproveitada quando existir, sem migração destrutiva.

Arquivos: CMakeLists.txt, src/app/Application.cpp, src/core/Config.h,
src/main.cpp, src/game/ImageAssets.cpp, src/ui/RmlFrontendSupport.*, documentos
RML, referências de marca nos editores, assets/builtin/character-face.png.

Validação: configuração limpa em `build-linblux` (o build antigo é preservado);
seis suítes de núcleo passaram: experiências, Studio, jogo, serialização/cena,
manipulação e spawn (6/6). Smoke gráfico da janela com logo em Xvfb.
A aparência antiga da página inicial ainda será substituída na etapa 3.

Commit: consulte `git log --oneline --grep='feat(identity)'`.
O envio é feito por push normal para origin/main após este registro.
Próxima etapa: vídeo de fundo, nova composição, tema e localização.
