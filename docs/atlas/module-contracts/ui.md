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
