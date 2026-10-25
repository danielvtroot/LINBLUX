# Arquitetura LINBLUX

| Target | Responsabilidade |
| --- | --- |
| linblux_engine | objetos, cena, física Bullet, Lua, rigs/animação, persistência, experiências |
| linblux_editing | histórico, gizmos, transformações e câmera do editor |
| linblux_renderer | renderização da cena e personagem com Camera3D, sem ImGui/RmlUi |
| linblux_presentation | catálogo de localização compartilhado |
| linblux_media | vídeo local assíncrono, sem dependência do editor |
| linblux_studio | RmlUi, painéis ImGui, ações e composição do Studio |
| linblux | executável LINBLUX Studio |
| linblux_player | executável de jogo, sem link com Studio, ImGui ou RmlUi |

`linblux_core` é uma fachada transitória Engine + ferramentas de edição para
compatibilidade dos testes/embutidos. `linblux_editor` é alias de Studio.
O namespace C++ legado é preservado por enquanto; consulte COMPATIBILIDADE.md.

Scene possui os objetos e seus IDs estáveis. PhysicsWorld possui corpos Bullet.
ProjectIO valida antes de alterar a cena e grava por arquivo temporário, fsync e
rename. ExperienceManager gerencia manifestos, mídia e cópias independentes.
RuntimeSession recebe uma cópia serializada do mundo autoral e é dona da cena,
física e controlador de execução. Lua executa somente nessa cópia.

O renderizador recebe Camera3D no runtime. Adaptadores de EditorCamera ficam na
camada de edição. Player carrega diretórios existentes sem regravar o manifesto;
a importação de um pacote cria uma cópia local. Nenhum serviço de rede é iniciado.

Limites: Scene ainda contém seleção/listeners usados pela autoria; a serialização
conhece dados de rig compatíveis de seis juntas; recursos de imagem ainda usam
raylib no núcleo. São pontos de acoplamento remanescentes, documentados para a
evolução gradual. LINBLUX Server e transporte multiplayer não estão implementados.
