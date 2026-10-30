# Histórico de mudanças

## 0.1.0-alpha.1 — preparação Alpha, 30/10/2026

Código/documentação em `main`; sem tag ou binário público aprovado.

- Explorador compacto com linhas de 21 px, recuo menor e nomes existentes preservados.
- Grade visual WIP retirada; Snap e encaixe por superfície preservados.
- Sliders alinhados ao controle de Câmera, com arraste, teclado e mudo.
- CMake alinhado a 0.1.0; build oficial `build-linblux/`.
- README pt-BR/inglês, galeria por metadados e índice de documentação.
- Templates de issues/PR, guia Player/build e política de segurança.
- Fixture RifleDemo aguarda inventário real preservando suas asserções.

Resultados e bloqueios em [ALPHA_0_1_0.md](ALPHA_0_1_0.md) e no
[relatório](releases/ALPHA_0_1_0_LANCAMENTO.md).

### Recursos da base anterior

- HUD circular vermelho compartilhado, saúde real e opção Lua de ocultação.
- Menus laterais do Studio RmlUi/Player, animação, captura e configurações reais.
- ItemActions genérico, munição/intervalos/recarga, eventos e limites numéricos.
- RifleDemo recuperado, controles E/1/R/Delete, dano por Attachment/raycast,
  som por IDs estáveis, efeitos com descarte independente do Script do item.
- Demo com NPCs/rig, patrulha animada, decal e roundtrip `.lo`.
- Correção: inicialização preserva NPCs já configurados pelos Scripts.
- Empacotador CMake Linux, manifesto/SHA-256 e verificação fora do checkout.
- Vídeo excluído da instalação por padrão até confirmar redistribuição.

Bloqueios de Release e escopo explícito em ALPHA_0_1_0.md. Resultados detalhados
nos relatórios30–33; testes selecionados não significam aprovação global.
