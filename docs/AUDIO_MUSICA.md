# Áudio e música da experiência

No Studio, **Início → Áudio** abre a biblioteca. MP3, WAV e OGG são importados
para `assets/` da experiência; renomear o recurso não muda seu ID. Os botões
Ouvir/Pausar/Parar usam a prévia, e Copiar código/Inserir no script oferecem
chamadas Lua com o ID real. Exemplos Lua de áudio cria um Script editável.

Música de fundo e playlist são opcionais. Nada começa automaticamente apenas
porque um áudio foi importado ou marcado como música. Escolha uma faixa em
**Música de fundo**, ou adicione faixas à **Playlist**. Playlist tem precedência;
usa uma voz por vez e pode repetir. O volume final combina o volume da faixa
com o volume de música configurado. IDs ausentes são indicados e ignorados.

Criadores podem controlar tudo por Lua em vez dessa configuração: as APIs
`linblux.audio.play`, `pause`, `resume`, `stop`, `stop_all`, `set_volume` e
`set_loop` permanecem disponíveis. Não configure simultaneamente duas músicas
se desejar só uma: Script e configuração declarativa são escolhas explícitas.
`stop`/`stop_all` não reiniciam a playlist por engano; somente o término natural
de uma faixa avança a lista. A pausa explícita do runtime também pausa a música.

Configuração e IDs são salvos com a experiência, inclusive exportação/importação
`.lo`. Remover o áudio exige resolver referências de Scripts, objetos, música
de fundo e playlist. Remover uma faixa da playlist não apaga o arquivo original.
Projetos sem música configurada mantêm a serialização anterior sem esse campo.
Leitores antigos podem ignorar a configuração opcional; use a versão atual para
editar experiências que dependem dela.

O backend usa streaming. Sem dispositivo, testes podem verificar contrato e
tempo pela duração decodificada; isso não comprova som audível. A reprodução
em dispositivo físico e o mixer de música/efeitos do Player ainda precisam de
validação/desenvolvimento específicos. Não há novo áudio espacial neste fluxo.
