# Áudio MP3/WAV/OGG — metadados, vozes com dono e exclusão segura

Base remota: `e719270bb2c3017da2085d21b7138b11ce6c1023`. Diffs locais alheios
(áudio, inventário, Player, TURBO) foram preservados; este incremento publica
apenas a unidade de áudio já implementada e agora validada.

## Problemas reais corrigidos

- `ProbeAudio` decodificava o arquivo inteiro com `LoadWave` só para descobrir a
  duração. Uma música longa custava RAM/tempo na importação e no primeiro
  `play`. Agora a leitura usa metadados do container (FFmpeg/libavformat) com
  orçamento de 2 s, `protocol_whitelist=file`, `format_whitelist=wav,ogg,mp3`,
  `probesize`/`analyzeduration` limitados e cache por tamanho+mtime. Sem
  FFmpeg, a raylib só inspeciona com dispositivo aberto — e isso é declarado no
  erro, não mascarado.
- O `Player` chamava `InitAudioDevice` **depois** de iniciar os scripts; um
  `linblux.audio.play` no `EngineStart` caía no modo sem dispositivo. O
  dispositivo agora é aberto antes da sessão, e fechado por guarda RAII também
  nos caminhos de erro.
- Vozes não tinham dono: remover a peça deixava o som tocando. `play` aceita
  `owner`; `AudioSystem::update(dt, scene)` libera a voz quando o objeto some.
  O `RuntimeSession` também atualiza vozes durante a pausa, então pausar não
  congela nem vaza áudio.
- O contrato Lua não expunha duração da biblioteca, `device_ready` nem se a voz
  é streaming. Foram adicionados `list()[i].duration`, `audio.device_ready()` e
  `audio.is_streaming(handle)`, além do limite de 16 operações de áudio por
  quadro.
- Exclusão de áudio não era transacional. `DescribeSoundUsage` lista
  objeto/atributo/script que cita arquivo, `id` ou nome; `RemoveSoundAsset`
  recusa uso, valida caminho/symlink, move para backup temporário, salva e
  restaura metadados e arquivo se o salvamento falhar.
- `GameAssets` valida nome (1–128 bytes) e duração finita/não negativa, e a
  duração é persistida em `assets.json`. A UI de Sons locais ganhou prévia
  (ouvir/pausar/parar), volume, loop, renomear e excluir com dependências.

## Evidência

- Build incremental: `cmake --build build-linblux -j2` sem erros; define
  `LINBLUX_AUDIO_METADATA_FFMPEG` ativa (flags.make).
- `build-linblux/linblux_audio_tests`: `Probe PCM1h: duração 3600.00s, aumento
  de pico RSS 6292 KiB` — arquivo PCM esparso de 1 h não é decodificado na RAM.
- TURBO supervisionado `.turbo/logs/audio-finish`: `linblux_audio`,
  `linblux_studio`, `linblux_lua_player_validation` e `linblux_experiences`
  aprovados (4/4), cobertura parcial.
- `linblux_audio` cobre MP3/WAV/OGG reais, volume/pause/loop, 64 vozes,
  voz com dono removida junto do objeto, biblioteca persistida/reaberta,
  duração salva, recusa de exclusão em uso, reversão em falha de salvamento e
  remoção autorizada com arquivo + metadados apagados.

## Regressões de integração corrigidas nesta etapa

- `linblux_editor_restart`: a comparação aceita apenas o arredondamento float32
  da rotação persistida (15° → 14,999998) e o bloco `body` padrão que o editor
  grava, sem tolerar eixo trocado ou rotação ausente.
- `linblux_engine_default_restart`: cartão do mundo padrão localizado pela cor
  da marca e Play localizado pelo id `play` no diagnóstico de geometria
  (`LOBLOX_TRACE_UI`), reemitido após cada redimensionamento.
- `linblux_experience_startup`: `faceImage` vazio é válido desde a remoção do
  rosto obrigatório de fábrica.
- `linblux_rml_ui`: a página de recursos usa o escopo `assets`, então o botão de
  importação é `assets-import-dialog-`; o teste também confirma que a seção de
  sons locais é construída mesmo sem dispositivo de áudio.
- Execução integral supervisionada de `linblux_rml_ui` em 1920x1080: 1051
  verificações, 0 falhas.

## Limitações

- Sem áudio 3D/atenuação por distância e sem mixer por categoria.
- A prévia do Studio e a reprodução do Player continuam 2D globais.
- Sem FFmpeg instalado, importação headless falha explicitamente; a gravação
  nunca ocorre sem metadados válidos.
- Streaming é o `Music` da raylib (arquivo aberto no `play`), não decodificação
  parcial própria.
- Pacote/instalação limpa, HUD, menu lateral, UI Lua, RifleDemo e Demo integrada
  continuam pendentes do Alpha.
