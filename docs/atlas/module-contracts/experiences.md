# Módulo `experiences`

**Responsabilidade:** ciclo de vida de experiências e pacotes `.lo`: manifestos,
diretórios de biblioteca, templates, importação/exportação, imagens, dados
padrão e cópia independente do mundo autoral.

**Fonte:** `src/experiences/ExperienceManager.{h,cpp}`.

**Entradas:** raiz de dados, raiz de factory/defaults, manifesto JSON, diretório
de experiência ou pacote, cena autoral e opções de criação.

**Saídas:** `Experience`, `Manifest`, listas de experiências ausentes, cenas
carregadas, pacotes exportados/importados e caminhos de recursos.

**APIs principais:** `list`, `missing`, `create`, `inspect`, `open`,
`loadReadOnly`, `save`, `exportPackage`, `importPackage`, `importCharacter`,
`restoreFactoryDefaults`, `editEngineDefault`.

**Invariantes:** a escrita atômica do mundo continua sendo responsabilidade de
`ProjectIO`; uma operação que falha não deve substituir a experiência ativa nem
a cena anterior; pacotes importados são copiados localmente; caminhos relativos
são contidos na raiz da experiência.

**Dependências:** `Scene`, `ProjectIO`/serialização, `core`; usa filesystem e
JSON. Não depende de ImGui/OpenGL.

**Ciclo de vida e erros:** a biblioteca pode existir sem experiência ativa;
erros são exceções com mensagem legível. `close` encerra a experiência ativa;
backups de defaults são registrados em `lastDefaultsBackup_`.

**Testes:** `linblux_experiences`, `linblux_defaults`, `linblux_player_validate`,
`linblux_experience_ui`, `linblux_engine_default_lifecycle`,
`linblux_engine_default_paths` e testes de restart.

**Como estender:** qualquer novo campo de manifesto precisa aceitar dados
antigos, validar antes de gravar e manter cópia independente. Atualize contratos
de `serialization`/`scene` se o formato do mundo mudar.

**Limites:** não há serviço de rede nem multiplayer; a importação de rigs
compatíveis cobre o formato R6 atual. Testes de UI de experiências são caros.
