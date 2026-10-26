# 26 — Inventário de Modelos / preparação Alpha

Código e4863f372e8225aedd49b1f211964bba5e748039: IMPLEMENTADO, TESTADO e VALIDADO NO PLAYER.
Protótipos nas Propriedades (tipo e ícone), pré-carregamento antes da VM, clone
com scripts ativos, instâncias world/stored/equipped, bolsa, equipar/desequipar,
descarte com busca de chão/espaço Bullet, remoção e consultas. API pública
linblux.items e inventory_changed; limites e exemplo real em INVENTARIO.md e
assets/examples/lua/inventario.lua. Identidade/atributos/scripts permanecem no
mesmo Modelo ao coletar/soltar/recolher. Cena autoral intacta após Play/Stop.

InventoryHidden centraliza ocultação de peças/decals, queries e contatos.
item_icon participa da consulta central de referências de imagens. Configuração
visual acompanha JSON/.lo/Mundo Padrão; bolsa/equipamento são estado da sessão.
Clone autoral deriva novo ID de tipo; instância não vira outro protótipo.

Arquivos: Inventory, Runtime, LuaRuntime, ImageAssets, PhysicsWorld,
SceneRenderer, Scene, StudioPanels/StudioFrontend/EditorActions, CMake e testes
studio/experiences/lua_player/decal_render/rml_ui. Contrato ATLAS game atualizado.

Evidências ATLAS/TURBO:
- build focado135,45 s, depois23,16 s; integração da autoria174,65 s;
  correção final89,61 s. FAST final20261026T153147Z-24075: build48 s,
  15/15 em15,5 s (total64,5 s), cobertura desconhecida pela seleção/CMake novo.
- IMPACT20261026T153342Z-25703:20/20 em64,5 s (total65,2 s), global parcial.
- Integração supervisionada experiences/models_ui/decal_render:3/3 em57,581 s,
  .turbo/logs/inventory-final-integration.log. Captura inspecionada em
  /tmp/loblox-rml-ui-WP1HT0/model-properties.png.
- Native InventoryLifecycle: duplicação de coleta rejeitada, escala preservada,
  descarte usando bounds reais (inclui esfera não uniforme), referências
  destruídas, scripts clonados e autoria intacta. Player executa inventario.lua.
- GUI cria protótipo, muda tipo, salva e usa spawn/collect/equip durante Play.
  Pacote .lo/defaults preserva tipo/ícone; pixels verificam protótipo e item
  guardado ocultos inclusive com decals, e instância world visível.
- Primeira GUI falhou: checkbox retinha foco e bloqueava atualização das
  Propriedades. Corrigido sem afrouxar o teste; novos campos aparecem ao marcar.
  Nenhum FULL nem pacote de distribuição/instalação limpa desta versão.

Limites Alpha explícitos: estrutura capturada ao instanciar; não adicionar ou
remover Parts enquanto guardado/equipado. Sem transferência multiplayer ou
salvamento automático da bolsa entre partidas; criador usa storage para dados
persistentes. Protótipos não aceitam NPC nem juntas externas. Fluxo do seletor
de ícone compartilha ImagePicker existente; o teste GUI desta unidade usa nome
sem escolher ícone. Exportação/importação inclui ícone configurado no teste.

PENDENTE imediato: interação universal por proximidade/segurar E, Delete e
hotbar dinâmica (a API de inventário não simula esses controles). Depois áudio
completo, UI Lua, combate/RifleDemo, HUD circular, menu lateral, Demo integrada,
regressões finais, pacote Linux, licenças/segurança, instalação limpa e Release.
Não criar tag/Release enquanto os critérios essenciais estiverem pendentes.
