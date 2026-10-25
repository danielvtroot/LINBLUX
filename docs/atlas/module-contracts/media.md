# Módulo `media`

**Responsabilidade:** decodificar e desenhar o vídeo de fundo da tela inicial de
forma assíncrona, com fallback estático quando FFmpeg não está disponível.

**Fonte:** `src/media/VideoBackground.{h,cpp}`.

**Entradas:** caminho do vídeo, dimensões da área de desenho e ciclo de
start/stop controlado pela UI.

**Saídas:** textura/frame atual, estado `running`/`failed`, desenho em recorte
(`cover`) e liberação de recursos.

**APIs principais:** `start`, `stop`, `draw`, `running`, `failed`, `cover`.

**Invariantes:** decodificação ocorre fora da thread principal; o objeto não
bloqueia o fechamento; falha de FFmpeg/codec não derruba o Studio; nenhum
arquivo de mídia é regravado.

**Dependências:** raylib/OpenGL, threads e, opcionalmente, FFmpeg
(`LINBLUX_ENABLE_VIDEO`). Não depende de ImGui, cena ou domínio.

**Ciclo de vida e erros:** `start` cria recursos e thread; `stop` sinaliza e
junta a thread; destrutor chama `stop`. Erros viram estado `failed` e fallback
visual.

**Testes:** `linblux_video` (com FFmpeg), `linblux_home_video` (UI/Xvfb) e
execução sem vídeo para validar o build de fallback.

**Como estender:** mantenha o decode fora da thread de render e proteja buffers
com mutex/estado atômico. Adicione teste de start/stop repetido para regressões
de thread.

**Limites:** apenas vídeo local; não há streaming, áudio sincronizado ou
legendas. A medição de desempenho de GPU não está disponível neste ambiente.
