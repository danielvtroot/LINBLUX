# Vídeo de fundo do Início

O arquivo real `midia/careditfullhd.mp4` é desenhado **antes de todo o RmlUi**,
com cover proporcional sobre a janela inteira. Um overlay preto e a interface
transparente ficam por cima. O vídeo não integra os cards nem as imagens do mapa.
Ele tem HEVC, 1280×720, 30 fps e cerca de 24 segundos. A faixa de áudio nunca é
aberta: não há decodificador de áudio nem dispositivo de som nesse componente.

FFmpeg (libavformat, libavcodec, libavutil, libswscale) faz streaming local em
thread própria, com dois buffers RGBA limitados a 1280 px de largura, decodificador
com duas threads e uma textura reutilizada. Não há leitura integral do vídeo.
A troca dos buffers usa swap; só o frame novo é enviado à GPU. O relógio usa os
PTS do stream. EOF faz seek e flush para repetir. Sair do Início cancela a espera,
encerra a thread, fecha codec/demuxer e libera buffers e textura.

Sem biblioteca, arquivo ou codec, usa fundo grafite estático. Não usa foto do
carro como fallback. Compile com `-DLINBLUX_ENABLE_VIDEO=OFF` para validar fallback.

Referência técnica: [API oficial FFmpeg](https://ffmpeg.org/doxygen/trunk/demux_decode_8c-example.html).
A licença depende da configuração da biblioteca instalada: LGPL 2.1+ no núcleo,
com componentes opcionais GPL. Consulte [FFmpeg legal](https://ffmpeg.org/legal.html).
A ligação é dinâmica; não se redistribuem os binários do sistema neste repositório.
Pacotes binários futuros precisam registrar configuração, fontes correspondentes
e obrigações da versão efetivamente distribuída, inclusive avaliação do HEVC.

O teste `linblux_video` percorre o clipe completo, verifica loop, abertura
assíncrona, cancelamento, caminho inválido e cover. As capturas do executável
validam as camadas e interação. Os números em renderização por software não
substituem medição em GPU integrada real.
