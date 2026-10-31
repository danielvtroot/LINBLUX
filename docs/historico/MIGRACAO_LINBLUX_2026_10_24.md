# Migração LINBLUX — 24/10/2026

Registro histórico consolidado das etapas 01–06. Textos, limitações, hashes e
evidências das revisões originais são preservados abaixo; não são instruções
atuais nem aprovação da Alpha de hoje. Consulte [o estado atual](../ALPHA_0_1_0.md).

## Etapa 01

Origem: `docs/relatorios/01-seguranca.md`.

### Etapa 1 — segurança (24/10/2026)

- Branch original `fix/unified-3d-gizmos`, HEAD `0e088fd`, alterações locais
  preservadas em `eb19e90`; branches antigas e histórico preservados em bundle.
- Remoto anterior preservado como `legacy`; origin configurado para o oficial.
- Histórico inicial remoto unido por merge `5c68449`, push confirmado para main.
- Clone independente em `/tmp/linblux-restore-check` usado para verificar restauração.
- .gitignore ampliado; nenhuma fonte/dependência necessária excluída. Varredura
  por padrões de chaves privadas, tokens GitHub e AWS sem achados no conteúdo próprio.
- Vídeo oficial de 2,7 MB: abaixo dos limites usuais; Git LFS indisponível e
  desnecessário nesse tamanho. midia é rastreado, duplicata na raiz é ignorada.
- Validação inicial: build antigo falha pois CMakeCache referencia o diretório
  removido `/home/labubu/projeto/loblox`. Será criado build novo. Os PNGs antigos
  removidos pelo usuário também precisam de migração dos caminhos no CMake.
- Limitação: arquivos pessoais ignorados e /tmp não têm proteção remota.
- Próxima etapa: identidade, mídia oficial e compilação a partir de configuração limpa.

## Etapa 02

Origem: `docs/relatorios/02-identidade.md`.

### Etapa 2 — identidade

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

## Etapa 03

Origem: `docs/relatorios/03-interface.md`.

### Etapa 3 — interface e vídeo

Implementado: vídeo MP4 real como camada de fundo da janela inteira, cover,
overlay preto, logo original, título e ações em pt-BR, recentes sobre o vídeo.
Sem barra lateral, lista de templates ou descoberta na página inicial. Tema
compartilhado JSON para ImGui/RmlUi, catálogo pt-BR, Player com identidade própria.
O formulário passa a oferecer apenas ícone e banner; o campo legado de imagem
de carregamento permanece somente para leitura/compatibilidade dos projetos.

Arquivos principais: src/media/VideoBackground.*, src/ui/ExperienceUI.*,
src/ui/Localization.*, src/ui/StudioFrontend.cpp, documentos/estilos RML,
assets/ui/theme.json, scripts/generate-theme.py e CMakeLists.txt.

Validação visual real: capturas em /tmp/linblux-home-1280x720.png,
/tmp/linblux-home-1920x1080.png e /tmp/linblux-home-1000x800.png mostram vídeo em
toda a janela, inclusive atrás dos recentes. Diferença média entre frames
separados: aproximadamente 31 níveis RGB. A captura inicial preta da etapa 2
foi insuficiente e substituída por essas verificações após a renderização.
Teste de codec completou o clipe e o loop em ~24 s, com cancelamento e fallback.

Medição em Mesa llvmpipe/Xvfb (1280×720): RSS estabilizou em 252968–252988 KiB
em três amostras; CPU do processo 219–221% (inclui toda a rasterização por
software), frames ~75 ms. Isso revela limitação de fluidez no rasterizador de
CPU e não é medição de GPU integrada real. A decodificação usa até 2 threads;
o teste isolado registra seu custo separadamente. Não foi possível validar uma
GPU integrada física neste ambiente.

A localização cobre as superfícies principais e usa catálogo central; mensagens
adicionais dos painéis e erros serão auditados na manutenção final.
Commit: consulte git log --grep='feat(ui)'. Push normal para origin/main.
Próxima etapa: exclusão permanente segura e validação dos fluxos de experiências.

Resultado posterior do perfil isolado do decodificador: 721 frames, 1 loop,
24,22 s decorridos, 6,85 s de CPU user + 0,12 s de CPU system (aproximadamente
29% de um núcleo), RSS máximo 70520 KiB. Teste gráfico dedicado passou em 19,37 s;
nove testes de núcleo/inicialização/Home passaram. Push de `c703edc` confirmado.

## Etapa 04

Origem: `docs/relatorios/04-experiencias.md`.

### Etapa 4 — experiências

Implementado: exclusão permanente real pelo gerenciador e pela confirmação em
pt-BR; nome explícito, aviso de remoção do disco, cancelamento, atualização da
biblioteca e status real. Arquivamento permanece separado. Percurso POSIX por
descritores protege a biblioteca e não segue links para diretórios externos.
Ícone e banner são os únicos campos visuais novos; metadados antigos continuam
legíveis sem exigir uma terceira imagem. Criação, importação `.lo`, abertura e
salvamento usam os mecanismos transacionais existentes.

Arquivos: ExperienceManager.*, ExperienceUI.cpp, tests/experiences_main.cpp,
tests/home_video_main.cpp, tests/experience_ui_main.cpp e EXCLUSAO_SEGURA.md.
Testes adicionados: ID adulterado, raiz externa, biblioteca como alvo, raiz
simbólica, destino externo de link interno preservado, vizinho preservado,
repetição de exclusão, fechamento do projeto ativo e índice sem entrada fantasma.
O teste gráfico cobre cancelar e confirmar exclusão pelo controlador real da UI.

Testes da rodada anterior: experiências e Home passaram (2/2). A rodada final
inclui a nova confirmação de exclusão e está registrada no log de validação.
Commit: consulte git log --grep='feat(experiences)'. Push normal para main.
Próxima etapa: recursos versionados de mundo e personagem padrão.

Rodada final: linblux_experiences e linblux_home_video passaram (2/2,
21,43 s), incluindo confirmação com nome, cancelamento e exclusão real.

## Etapa 05

Origem: `docs/relatorios/05-padroes.md`.

### Etapa 5 — mundo e personagem padrão

Implementado: recursos de fábrica versionados em assets/defaults; índice v1 e
snapshots imutáveis/atômicos, comandos de editar e salvar mundo e personagem,
configurações no Início, herança independente na criação e confirmação de
restauração. O Sentinela LINBLUX usa proporções, cores e visor originais.
Preservam-se cenas antigas, propriedades e clipes compatíveis. O diretório-fonte
dos padrões de desenvolvimento é mantido fora dos builds gerados.

Arquivos: ExperienceManager.*, ExperienceUI.*, assets/defaults, home.rml,
experiences.rcss, tests/defaults_main.cpp, tests/home_video_main.cpp, CMakeLists.txt.

Testes: defaults, experiências, jogo, spawn e Home passaram (5/5). A suíte nova
verifica edição, salvar/reabrir, herança de mundo/corpo, independência, restauração
sem sobrescrever experiências e recusa de versão futura. O teste gráfico também
exercita os comandos de edição/salvamento e confirmação pelo controlador da UI.

Limite explícito: corpo/esqueleto arbitrário e importação de malha externa ainda
não existem no runtime. O padrão é independente visualmente, com armazenamento
editável; o formato de seis juntas permanece para compatibilidade. Não se declara
concluído o suporte geral a personagens externos.

Commit: git log --grep='feat(defaults)'. Push normal para main.
Próxima etapa: separação Engine/Studio/Player, documentação, licenças e regressões.

Rodada ampliada: 8/8 suítes passaram em 26,08 s, incluindo os comandos de padrões
na UI. Após ajustar IDs de novas inserções e a descoberta da biblioteca legada,
experiências/jogo/padrões passaram novamente (3/3). Novas inserções e o fallback
nativo usam o Sentinela; createR6 permanece disponível para compatibilidade.

## Etapa 06

Origem: `docs/relatorios/06-independencia-e-validacao.md`.

### Etapa 6 — independência e manutenção (24/10/2026)

#### Implementação real

Engine, edição, renderização, apresentação, mídia e Studio têm targets separados.
O executável `linblux_player` carrega e executa experiências sem link com ImGui,
RmlUi ou Studio. A cena autoral permanece independente do runtime. Server está
no roadmap: não há implementação multiplayer funcional.

O cartão fixo **Mundo Padrão da Engine** abre um documento protegido fora da
biblioteca. Publicar o padrão exige confirmação, faz backup do índice anterior
e publica mundo/personagem juntos. Restauração também exige confirmação e backup.
O editor permite substituir o personagem, editar as juntas compatíveis e importar
personagens de experiências `.lo`/JSON, copiando imagens e animações.

O formato atual tem seis corpos/juntas. Importação de malhas e esqueletos
arbitrários, terreno avançado e controladores externos não estão implementados.
Esses limites impedem declarar concluído o objetivo amplo de importação irrestrita.

#### Arquivos principais

`CMakeLists.txt`, `src/player/main.cpp`, `src/editor/SceneRendererAdapter.cpp`,
`src/experiences/ExperienceManager.*`, `src/ui/ExperienceUI.*`,
`src/editor/AnimationEditor.cpp`, `src/ui/StudioFrontend.cpp`,
`tests/engine_default_main.cpp`, `tests/engine_default_restart.py`,
`packaging/*`, README, CONTRIBUTING e guias atuais em docs.
Os documentos obsoletos foram preservados em `docs/historico` com aviso explícito.
Licenças e créditos foram mantidos, inclusive o aviso de copyright anterior.

#### Regressões encontradas e corrigidas

A primeira rodada completa encontrou dez falhas em 32 testes. Elas incluíam
buscas por títulos ImGui antigos, coordenadas de botões dependentes de texto em
inglês, geometria de logo antiga, formulário com ação fora da área visível e
roteamento de foco dos painéis na execução. As verificações foram atualizadas
para os controles e a arte reais, preservando as asserções funcionais.

A suíte RmlUi revelou ainda a expansão excessiva dos botões com `flex-basis:auto`.
O ribbon passou a ter larguras explícitas, textos curtos e rolagem em janelas
estreitas. A confirmação de animação usa medidas dos textos traduzidos.
Uma asserção recém-adicionada que supunha WASD obrigatório com botão direito
foi corrigida para o comportamento efetivo da câmera existente.

No Player, o filtro de recursos rejeitava a logo oficial fora do projeto aberto.
A exceção agora é específica para a logo interna; caminhos externos continuam
bloqueados. A imagem é desenhada com alfa pré-multiplicado.

#### Evidências e limites

O teste de ciclo de vida publica um personagem original com hierarquia, animação,
imagem, física, câmera e script; reinicia em outro processo, cria uma experiência
e confirma o rig correto em movimento. Também verifica importação, publicação
atômica, cópias independentes, backup e preservação de experiências antigas.
O teste com o Studio real abre o cartão, encerra/reinicia, cria pela interface e
confirma o personagem herdado no runtime. A autoria detalhada da fixture é feita
pelo teste de núcleo; não se apresenta isso como edição manual de cada campo.

O decoder percorreu 721 frames e um loop em 24,22 s, com 6,97 s de CPU e pico de
68,9 MiB RSS. O Home completo em Xvfb/llvmpipe ficou próximo de 247 MiB RSS, com
variação de 20 KiB nas três amostras e 219–221% de CPU. A rasterização é por CPU;
esses números não representam uma GPU integrada real e não provam desempenho
adequado em todos os notebooks. É necessária medição física adicional.

A configuração sem FFmpeg compilou e foi renderizada: fundo grafite, zero frames
decodificados, sem substituição por foto de carro. A instalação em prefixo isolado
abriu o Studio usando os recursos instalados. A logo original manteve SHA-256
`5693046f85abee20fbc1cb164a3ef0ea80e1ec02827474ff2ee95c8c300dcf78`.

Commits: `aaddc8d` separação/runtime/padrão protegido; `4359a25` logo do Player.
Ambos enviados por push normal e confirmados no remoto oficial.

A revisão de assets deixou imagens pessoais de licença desconhecida fora da
instalação e do índice atual; cópias locais e históricas foram preservadas.
A licença dos materiais fornecidos e o empacotamento de FFmpeg GPL requerem
revisão antes de distribuição de binários. Veja ASSETS_E_LICENCAS.md.
