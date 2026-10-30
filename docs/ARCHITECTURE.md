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

O build oficial é `build-linblux/`. A distribuição Alpha 0.1.0 usa CMake 0.1.0;
identificadores legados de formatos e APIs continuam compatíveis. O empacotador
instala recursos selecionados e registra dependências em manifesto separado do
build local. Veja [BUILD](BUILD.md) e [Alpha](ALPHA_0_1_0.md).

`Scene::parts()` mantém todos os dados autorais, inclusive armazenamento.
`worldParts()` e `isWorldObject()` usam um índice da árvore Workspace, reconstruído
sob demanda após mudanças estruturais e mantendo a ordem de criação das Parts.
Renderização, Bullet, navegação, NPCs, spawn, picking e consultas de gameplay
usam essa classificação comum. Hierarquia não é percorrida por Part a cada frame.
Reparenting notifica a física para remover/restaurar corpos; edição de uma Part
armazenada não pode criar um corpo por `applyChanges`. A serialização e o cálculo
de geometria autoral de Modelos continuam usando todos os objetos.

ServerScriptService é um serviço de scripts do runtime local. ServerStorage e
ReplicatedStorage guardam dados e modelos inativos no Mundo; não representam
processos de rede. O inventário encontra protótipos nessa hierarquia e instancia
cópias explicitamente sob Workspace, restaurando os scripts do clone.
