> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Continuidade RmlUi — 22/10/2026

Atualização Player UI, 23/10: [PLAYER_UI_2026.md](PLAYER_UI_2026.md).
Checkpoint `5538899`; topbar/menu/Settings/Health reconstruídos. Última suíte
completa: **19/19**, 408,99 s; RML 984 checks; qualidade 83 checks por resolução.
Último polimento RCSS: altura máxima 800 px / About centralizado; 331 checks
focados posteriores, zero falhas. Capturas: `build/player-ui-2026/`.
Último commit funcional: `2b05fc0` (após a base `5538899`); executável Release
principal recompilado e aberto/encerrado normalmente. Sem push.
Os resultados/medições abaixo são históricos da migração inicial.

- Branch preservado: `fix/unified-3d-gizmos`. Entrada `c461e38`.
- `49c2383`: preserva declarações preexistentes de Surface/Lighting que faltavam
  ao código já rastreado. `b7bd503`: primeiro frontend RML funcional integrado.
- Padrão: `build/loblox`; diagnóstico: `build/loblox --ui legacy`.
  Debug não foi recompilado nesta migração.
- RmlUi 6.3 + GL3 no contexto raylib existente, FreeType, PNG/JPEG com alpha
  premultiplicado. MSAA 4× no framebuffer de camada da UI (23/10/2026; antes
  era 0 e as bordas de `border-radius` saíam serrilhadas). Fonte e ícones
  cacheados.
- RML: shell/ribbon agrupada/Toolbox/Explorer/Properties/Game Config/importador,
  HUD/topbar/menu Esc/diálogos. Splitters, visibilidade e layout persistidos.
- Internos ImGui: código, timeline, rig e Output, dentro do split RML.
  Ainda não há docking arbitrário com painéis flutuantes no modo RML.
- Input: uma fila ordenada e um proprietário; texto RML informado ao backend;
  refresh de formulário não edita dados; Esc fecha diálogo; drags mantêm dono.
- Imagens: corrigidos caminho absoluto Linux e crash no renderer por chaves
  opcionais nulas; transparência ausente agora é opaca. Teste de pixels próprio.
- Referências: sete reabertas; `aa.png`/`images.png` ausentes na retomada.
  Assets originais preservados. Rosto original inspecionado em captura frontal,
  sem alteração de malha/PNG nesta migração.
- Última suíte completa: 16/16, 290,99 s; RML 461 checks; fixture gráfica 29
  checks. Ordenação de popups nativos revalidada junto com a política de
  textura (mip/trilinear, alfa premultiplicado, ícones em 96 px).
- Capturas inspecionadas: `/tmp/loblox-rml-ui-KP2PVw/` e aplicação principal em
  `/tmp/loblox-rml-comparison-v2of3p/`. Não são mockups.
- Medição aproximada, llvmpipe/1366×768: RML 166 MiB idle / ~60 ms por frame;
  legacy 146 MiB / ~101 ms. Layouts diferentes; não é medição do notebook.
- Pendentes: validação física Sway/notebook e DPI fracionário; sanitizers desta
  migração; docking completo; migração interna dos editores especializados;
  importação empacotada por projeto/múltiplos decals (formato antigo preservado).

Próximo comando: `cmake --build build -j2 && ctest --test-dir build --output-on-failure -j2`.
Não iniciar outra refatoração antes de concluir o checkpoint de UI corrente.
