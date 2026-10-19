# Roadmap

Sequência sugerida. Nenhum item abaixo está implementado além do que consta em
`docs/PROGRESS.md`.

## Etapa 1 — Fundação do editor (concluída)

- Viewport 3D, câmera, Baseplate com studs.
- Modelo de objetos, hierarquia, Explorer e Properties.
- Física Bullet contínua, Anchored/CanCollide.
- Ferramentas Select/Move/Scale/Rotate, cor, material, âncora, exclusão.
- Salvamento/carregamento JSON v1.
- Testes headless e documentação.

## Etapa 2 — Fluxo de construção

- Undo/redo de criação, propriedades, transformações e exclusão.
- Duplicar seleção (`Ctrl+D`).
- Multi-seleção e transformação em grupo.
- Snap de grade e de rotação (ex.: 1 unidade / 15°).
- Renomear pelo Explorer (duplo clique).
- Reordenar hierarquia e agrupar em pastas/modelos.
- Atalhos de ferramenta (`1`–`4`).

## Etapa 3 — Personagem e spawn

- Avatar R6 em blocos.
- Controlador de personagem usando a física existente.
- Uso real da SpawnLocation.
- Câmera de personagem separada da câmera de editor.

## Etapa 4 — Modo de execução (etapa separada)

- Play/Stop **somente quando o editor estiver estável**.
- Isolamento entre estado de edição e estado de execução.
- Reinício de cena e restauração do estado editado.
- Nada disso deve ser antecipado na fundação atual.

## Etapa 5 — Conteúdo e ferramentas avançadas

- Mais formas (cilindro, cunha, tubo) e operações de união/subtração.
- Terreno simples e pintura.
- Biblioteca local de modelos (sem catálogo online).
- Materiais com mais variação visual e, se viável, shader de iluminação.
- Alinhamento e distribuição de peças.

## Etapa 6 — Scripts e lógica

- Sandbox de scripts do usuário.
- Eventos de toque/colisão expostos de forma controlada.
- Depuração e console.

## Etapa 7 — Persistência e produtividade

- Autosave e recuperação de sessão.
- Arquivos recentes e caminho padrão configurável.
- Exportação/importação de subárvores.
- Migração de versões antigas do formato.

## Etapa 8 — Desempenho e portabilidade

- Medições reais em Loc-OS/Sway com GPU fraca.
- Resolução dinâmica da viewport, se necessário.
- Instancing para muitas peças repetidas.
- Backend Wayland nativo, se a raylib oferecer suporte estável.
- Empacotamento (AppImage/deb) e instruções de instalação.

## Fora de escopo por enquanto

- Multiplayer, publicação de jogos, loja de assets, clima avançado,
  iluminação global, animações complexas e editor de keyframes.
