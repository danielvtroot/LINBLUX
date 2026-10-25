# Etapa 3 — interface e vídeo

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
