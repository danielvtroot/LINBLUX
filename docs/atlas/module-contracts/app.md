# Módulo `app`

**Responsabilidade:** inicializar e encerrar o executável `linblux` (Studio):
janela raylib, ImGui/rlImGui, tema, cena inicial, câmera, renderer, UI e
`ExperienceManager`.

**Fonte:** `src/app/Application.{h,cpp}`, `src/main.cpp`.

**Entradas:** `AppConfig` (tamanho da janela, vsync, FPS, UI moderna e caminho
de projeto inicial), argumentos do processo e ambiente gráfico.

**Saídas:** laço principal do Studio; janela, cena autoral, física, UI e
experiência ativa. Não é o runtime do Player.

**APIs principais:** construtor `Application(AppConfig)`, `run()`,
`initialize()` e `shutdown()` privados.

**Invariantes:** o destrutor chama `shutdown`; erros de janela/projeto são
registrados em `diag` e não devem derrubar uma cena válida. A inicialização sem
`--project` abre a Home quando a UI moderna está ativa. Um caminho explícito
nunca é sobrescrito.

**Dependências:** `linblux_studio` (na prática o agregado `linblux_core`,
renderer, mídia, apresentação, ImGui, RmlUi), raylib/OpenGL e o ambiente
gráfico.

**Ciclo de vida e erros:** `initialize` → `run` → `shutdown`; falha de janela
retorna erro; carregamento de projeto é transacional e mantém a cena padrão em
caso de falha. Saída normal libera recursos na ordem inversa.

**Testes:** `linblux_startup_no_display`, `linblux_help`, testes de restart e
testes de UI do Studio; a suíte gráfica usa Xvfb/software rendering.

**Como estender:** adicione inicialização em `initialize`, libere em `shutdown`,
propague erros por `diag` e mantenha `main.cpp` apenas como adaptador de
argumentos. Evite colocar regra de domínio em `Application`.

**Limites:** não há teste unitário isolado de toda a composição; a janela real e
o driver OpenGL continuam necessários para o caminho gráfico.
