# 24 — Attachments / preparação Alpha 0.1.0

Código: e9d257e4c2e5deb69cbc94145c927c858b831514. IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER.
Ainda não existe Release Alpha validada: não foi criado tag nem pacote novo.

- Componente real filho de Part/Modelo, IDs, nomes, posição/rotação locais,
  pose global e direção, criação/acesso Lua, clone, remoção e Propriedades.
- Modelos girados/escalados mantêm BocaDoCano correta. Reparentar preserva
  âncoras; agrupamento/desagrupamento falho restaura nós/poses/ordem.
- JSONv5 com quaternions; v1–v4 continuam legíveis. Pacote .lo testado em outro
  HOME e Mundo Padrão preservam Attachment e vínculo; salvamento inválido não
  substitui documento. Exportação Lua declarativa legada continua só v1 e
  recusa hierarquia explicitamente; utilizar JSON/.lo para este recurso.
- Exemplo executável assets/examples/lua/attachments.lua e guia ATTACHMENTS.md.

TURBO: build inicial552,9 s, seguido de incremental focado24,51 s.
FAST inicial16/17: exemplo usava pcall, indisponível no sandbox; casos inválidos
passaram para validação separada no Player, sem liberar pcall.
FAST final20261026T142143Z-19162:17/17,22,1 s (22,7 s total).
IMPACT20261026T142246Z-20080:17/17,10,1 s, cobertura global parcial.
models_ui supervisionado47,226 s, timeout60 s/65 s supervisor, aprovado.
Captura /tmp/loblox-rml-ui-AQdAfK/attachment-properties.png inspecionada.
Nenhum FULL; não extrapolar estes resultados para os recursos pendentes.

AutoBackup ativo, último checkpoint remoto e14ff905d7d6 confirmado no início.
Diffs preexistentes preservados; snapshot inicial em .turbo/alpha-recovery.

Próxima unidade: vínculos físicos/juntas entre Parts/Attachments existentes,
com criação que preserve poses, clone com remapeamento de IDs e liberação
segura no Bullet. Depois inventário/E/Delete/hotbar; áudio/UI Lua; combate e
RifleDemo; HUD/menu; Demo integrada; distribuição, segurança/licenças e Release.
Vídeo MP4 não pode ser distribuído sem comprovar autorização; fallback deve ser
usado no pacote público se a licença continuar desconhecida. Não houve auditoria
final de licenças nem instalação limpa nesta unidade.
