# Candidato Linux — 26/10/2026

Source feature PUBLICADO:4e2887927b3a147f5d9346daa0434491e2c4cf53, push normal
confirmado. HUDbb69e49 e menu4a0be64 também confirmados no origin/main.

IMPLEMENTADO: tools/package_linux.py usa instalação CMake Release, inclui
Studio/Player, raylib/SONAME, recursos oficiais, exemplos, documentação e avisos;
registra dependências ldd, SHA-256 por arquivo e hash do tar.gz. Não copia
experiências pessoais. MP4 excluído; instalação sem vídeo é o padrão agora.
O vídeo local de desenvolvimento continua intacto. Manifesto marca candidato
interno e árvore suja quando houver WIP; o empacotador nunca publica Release.

TESTADO: candidato inicial54 arquivos verificados por hash, extraído em
/tmp/linblux-clean-install-c9gkrcch, HOME/XDG/CWD novos. Player --validate e
renderização real com8 frames/--check-scripts aprovados. Studio abriu a Home,
foi capturado e encerrado pelo supervisor (não é teste de saída normal do
Studio). TURBO run_command, timeout55s, resultado PASSED em16,018s;
.turbo/logs/linux-package-clean.log. Captura da Home inspecionada e publicada
com legenda de fallback. Não equivale a testar outro sistema operacional limpo.

Bloqueios reais: FFmpeg do ambiente declara GPLv2+, revisão de obrigações de
binário ligada a essa build e componentes embutidos da raylib ainda aberta.
Regressão gráfica histórica ainda exige o menu central/barra antiga; precisa
migrar expectativas antes de FULL. Áudio audível não validado sem dispositivo.
Modo --ui legacy fora do Alpha suportado. Destino privado de backup externo
continua não configurado. Não há tag/Release pública nem afirmação100%.

Próximo: migrar testes históricos rml_ui_main.cpp/graphics_quality_main.cpp
para o HUD compartilhado e menu lateral, rodar regressão abrangente com orçamento,
validar dependências noutra instalação Linux e fechar os avisos/licenças antes
de publicar v0.1.0-alpha.1. Candidato fica local em .turbo/dist, nunca no Git.

ATLAS atualizado. FAST20261026T232925Z-7411 não associou testes de empacotamento
(COBERTURA DESCONHECIDA, zero aprovações). Fallback técnico: teste real externo
supervisionado acima, em vez de inventar aprovação. Nenhum FULL foi executado.
