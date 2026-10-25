# Módulo `ui`

**Responsabilidade:** camada de apresentação do Studio — frontend RmlUi, UI de
experiências, ações do editor, localização, diálogos nativos e suporte de
renderização RmlUi.

**Fonte:** `src/ui/{ExperienceUI,StudioFrontend,EditorActions,Localization,
NativeFileDialog,RmlFrontendSupport}.{h,cpp}`; assets em `assets/ui/`.

**Entradas:** documentos RML/RCSS, locale, eventos de UI, ações do usuário,
cena/experiência ativa e resultados de diálogos nativos.

**Saídas:** documentos renderizados, comandos despachados, valores de
formulário, estado de carregamento, texto localizado e seleção de arquivos.

**APIs principais:** `StudioFrontend::draw/enableExperiences`,
`ExperienceUI::open/save/frame/dispatch`, `EditorActions::dispatch`,
`ui::NativeFileDialog::begin/poll`, `Localization.cpp`.

**Invariantes:** a UI não é dona da cena/física; comandos passam por
`EditorActions`/`EditorUI`; textos visíveis vêm do catálogo de localização e de
assets versionados; diálogo nativo é assíncrono e não bloqueia o loop; layout
deve tolerar redimensionamento.

**Dependências:** RmlUi/rcss, ImGui (painéis), raylib/OpenGL, `linblux_studio`,
`linblux_presentation`, mídia e engine. Assets em `assets/ui/`.

**Ciclo de vida e erros:** frontends são inicializados/possuídos pela aplicação;
documentos são carregados e fechados com estado de “unsaved”; falhas de arquivo
ou asset devem mostrar mensagem e manter a UI utilizável.

**Testes:** `linblux_rml_ui`, `linblux_studio_ui`, `linblux_experience_ui`,
`linblux_home_video`, `linblux_editor_input`, `linblux_game_ui`,
`linblux_manipulation_ui` e `linblux_mouse_lock`; exigem Xvfb/software.

**Como estender:** adicione texto em todos os locales suportados, IDs estáveis
no RML, comando testável e caminho de fallback. Não coloque regra de domínio no
documento; dispatch deve chamar a API do editor/engine.

**Limites:** testes de UI são lentos e dependem de Xvfb/xdotool; acessibilidade
e internacionalização além do catálogo atual não estão completas. Não há
servidor/rede.

**Home:** `experiences.rcss` é mantido manualmente e carrega antes do tema
gerado (`theme.rcss`). O menu dos cartões deve ter somente uma âncora vertical;
na Home, `top` exige `bottom: auto`. Preserve as larguras independentes de Abrir
e `…`. Botões com texto direto usam bloco e altura de linha explícita: o layout
flex do RmlUi não deve ocultar os nós de texto. A regressão curta de geometria,
cliques e capturas está no modo `--layout-smoke DIRETÓRIO` de
`linblux_home_video_tests`; execute-o pelo supervisor `run_command` do TURBO,
com Xvfb, timeout e diretório de recursos isolado.
`--actions-smoke DIRETÓRIO` verifica as mesmas ações sem repetir screenshots.
Na correção de 25/10/2026, as capturas de Home/menu foram inspecionadas a
1280×720; a regressão de ações passou em 23,76 s, supervisionada com limite de
60 s. Exportação por diálogo nativo e exclusão confirmada não foram exercitadas
nessa regressão curta; o cancelamento da exclusão foi verificado.
