# Guia do LINBLUX Studio

O Início apresenta o vídeo de fundo, Criar experiência, Abrir experiência e os
recentes. A Biblioteca mostra as experiências locais; Arquivo mostra as arquivadas.
Configurações permite escolher a biblioteca e editar os padrões da engine.

No Studio, use o Explorador para selecionar e organizar objetos e Propriedades
para editar os dados. Mover adquire a face e desloca a peça inteira; Girar usa anéis;
Escalar altera as faces locais preservando o lado oposto. 1–4 selecionam ferramentas.
Botão direito + WASD navega; Q desce, E sobe; F enquadra a seleção.

Scripts Lua, propriedades e animações são persistidos no projeto. O editor de
personagem permite editar corpos, juntas e poses compatíveis. F5 inicia um teste
isolado; Esc abre o menu enquanto a simulação continua; Shift+F5 retorna ao Studio.
Ctrl+S salva a experiência. Use Arquivo para detalhes, exportação `.lo` e padrões.

Ícone é quadrado e banner é horizontal; ambos são opcionais e têm placeholder
LINBLUX. O vídeo do Início é independente da mídia da experiência.
Arquivar mantém arquivos; Excluir permanentemente apaga a pasta após confirmação.
Leia EXCLUSAO_SEGURA.md e faça backup dos projetos que deseja preservar.

O cartão **Mundo Padrão da Engine** abre um documento protegido, separado dos
recentes. Salvar rascunho mantém a edição; **Salvar como padrão da engine** pede
confirmação e publica mundo/personagem juntos. Restauração de fábrica faz backup
antes de alterar o índice. Veja PADROES_DA_ENGINE.md para importação e limites atuais.

O Explorador usa linhas compactas, busca por nome/classe e botões de expansão.
Clique seleciona; Ctrl permite múltipla seleção, F2 renomeia e arrastar altera
o pai conforme as regras da hierarquia. Nomes antigos, espaços e IDs são preservados.
A grade visual foi removida. Snap de posição/escala e encaixe por superfície
continuam disponíveis; veja [Precisão](EDITOR_PRECISAO.md).
