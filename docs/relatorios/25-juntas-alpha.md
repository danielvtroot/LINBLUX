# 25 — Juntas fixas / Alpha em desenvolvimento

Código: e6f65914de0664343d004fe6c4e89e2853bfaae1. IMPLEMENTADO/TESTADO/VALIDADO NO PLAYER.
Pares de Attachments de Parts distintas alimentam btFixedConstraint. Conectar
preserva poses; alterar quadro local anima a peça dinâmica; remover qualquer
ponta libera a constraint antes do corpo. Peças conectadas não colidem entre si.
Clone remapeia IDs internos, desconecta referências externas na cópia; JSONv6
persiste/valida os vínculos sem alterar leitura v1–v5. Sem corpos duplicados.
Propriedades RmlUi/ImGui permitem escolher destino por nome. API pública Lua:
connect_to, disconnect e joint; exemplo juntas.lua executado no Player real.
Arquivos: Instance/Scene, PhysicsWorld, ProjectIO, LuaRuntime, StudioPanels,
EditorActions/StudioFrontend, testes studio/experiences/lua_player/rml_ui.

Validação ATLAS (impacto/contratos) e TURBO:
- Build focado inicial falhou por cast em trecho de adoção; guardas movidas
  para canParent. Build corrigido58,91 s; Studio/Player/UI/pacotes80,74 s.
- Native inicialmente detectou desvio físico0,14764 sob gravidade: 16 iterações
  eram insuficientes para a alavanca. Override local64 corrigiu, sem relaxar
  tolerância e sem elevar solver global. Regressão focada1,719 s aprovada.
- FAST20261026T143742Z-27681: build151,4 s,17/17 em12,6 s (total164,7 s).
- IMPACT20261026T144210Z-31357:17/17 em10,4 s; global parcial.
- models_ui supervisionado45,444 s aprovado: seletor de junta, âncoras,
  salvar/reabrir/Play/Stop/desagrupar. Captura inspecionada em
  /tmp/loblox-rml-ui-Asba8a/attachment-properties.png.
- Pacote .lo/defaults com vínculo e Player em outro HOME validados no teste
  experiences; erros de ID preservam o documento anterior. Nenhum FULL.

Limites explícitos: juntas fixas apenas; sem motor livre/dobradiça com limites,
sem novo rig arbitrário. Destino dinâmico necessário para animação física.
Benchmark de grandes montagens pendente. Inventário/interação/hotbar não foram
implementados por este incremento. AutoBackup ativo, push95d1977da501 confirmado.

Retomada: inventário real sobre Modelos e IDs, estados world/stored/equipped,
controle Lua e preservação de scripts/propriedades; depois E/Delete/hotbar.
ATLAS não localizou inventário de jogo: hits inventory.py são inventário de
TESTES do TURBO, não reutilizar como sistema de itens. Runtime/input e UI
compartilhada Studio/Player precisam integrar os novos estados. Continuam
pendentes áudio completo, UI Lua, combate/RifleDemo, HUD/menu, Demo, pacote,
instalação limpa, auditoria de segurança/licenças, tag e Release.
