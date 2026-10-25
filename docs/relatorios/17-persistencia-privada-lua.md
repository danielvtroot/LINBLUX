# Missão integral — persistência privada Lua

Implementado ProgressStore com get/set/remove/status, JSON limitado, escopo
fornecido pelo Studio/Player, salvamento atômico e trava entre sessões. Dados
privados separados de assets/exportação/Git. Cenas sem documento existente,
padrão da engine e --validate usam memória. Início cancelado descarta alterações;
pausa/Stop tentam salvar. Falhas preservam arquivo existente e geram diagnóstico.

Limites/política/formato em PERSISTENCIA_PRIVADA.md e SCRIPTING_LUA.md. Sem API
de caminho, arquivo arbitrário, rede ou comandos do Linux. Estado de jogo não
é cena autoral nem segue no pacote público. Destino de backup privado externo
continua pendente e não bloqueia os demais recursos.

Arquivos: ProgressStore.{h,cpp}, Runtime.{h,cpp}, LuaRuntime.{h,cpp}, Player/main,
ExperienceUI, trecho de GamePanels, CMake, testes studio/rml/player, persistencia.lua,
.gitignore e exclusão na política do AutoBackup. Diffs alheios do TURBO, editor
e locales preservados fora do commit.

ATLAS localizou requestPlay, Runtime::start e identificadores de instalação.
Captura do escopo ocorre no frontend antes do trabalho assíncrono; Lua não recebe
permissão para escolher namespace/pasta. O getter protege a alocação Lua com pcall
interno sem perder a liberação dos objetos C++ em caso de cota de memória.

Compilação inicial Studio/Player/studio_tests: 118,67 s (headers). Compilação
final desses alvos e rml_ui_tests: 24,19 s. FAST final 20261025T172332Z-11941:
14/14, zero falhas/timeouts, build 4,4 s + testes 12,7 s = 17,5 s totais.
Integração supervisionada progress-studio-player-integration: 4/4 em 14,43 s,
timeout global 75 s, cada gráfico CTest limitado a 45 s. Studio salva/para/reabre
e recupera valor; dois processos gráficos do Player avançam contador 1→2;
--validate mantém os bytes de progresso e da cena intactos; arquivo modo 0600.

IMPACT 20261025T172935Z-17013: 18/18 em 49,4 s (50 s totais), sem falhas/timeouts.
Sem rebuild redundante; gráficos gerais excluídos, pertinentes executados acima
com supervisor. Cobertura global parcial, sem FULL ou alegação de missão completa.

Regressões: cancelar início não grava; outra sessão não toma a trava; corrupção
preservada; link para arquivo válido recusado sem alterar destino; limites de
valor/leitura/alteração, funções/ciclos/metatabelas/tabelas mistas recusados.
Progresso normal recupera após fechar, JSON autoral permanece idêntico.

Limites: gravação pequena síncrona a cada 2 s de simulação e na pausa/saída;
hardware de disco com falha não tem garantia de latência. Encerramento abrupto
pode perder valores pendentes. Cenas avulsas movidas mudam escopo; instalações
gerenciadas preservam ID ao mover, novas importações recebem ID novo. Não existe
reset silencioso de dados corrompidos. Próximo: ampliar física/controle de personagem.
