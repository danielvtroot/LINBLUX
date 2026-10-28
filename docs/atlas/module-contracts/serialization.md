# Módulo `serialization`

**Responsabilidade:** ler/gravar a cena autoral em JSON e Lua, validar o formato
e oferecer escrita atômica.

**Fonte:** `src/serialization/ProjectIO.{h,cpp}`, `SceneLua.{h,cpp}`.

**Entradas:** `Scene`, texto JSON/Lua, caminho de destino e versão de formato.

**Saídas:** JSON/Lua serializado, cena reconstruída, erros legíveis e arquivo
gravado atomicamente (temporário + fsync + rename).

**APIs principais:** `projectio::toJson/fromJson/save/load/writeFileAtomically`;
`scenelua::toLua/fromLua/save/load/ValidateSchema`.

**Invariantes:** `kFormatVersion=7`; versões futuras são rejeitadas; a cena só é
limpa após validação estrutural; o arquivo anterior permanece intacto se a
escrita falhar antes do rename; IDs, hierarquia e propriedades são preservados;
JSON é a persistência canônica e Lua é um formato suportado.

**Dependências:** `Scene`, `objects`, `game::GameAssets`/Rig, nlohmann/json,
filesystem. Não depende de ImGui/física concreta.

**Ciclo de vida e erros:** funções retornam `bool` + `std::string error`; não há
estado global de sessão. Escrita é atômica e sincroniza o diretório.

**Testes:** `linblux_defaults`, `linblux_experiences`, `linblux_engine_default_lifecycle`,
`linblux_engine_default_paths`, `linblux_player_validate` e testes de
importação/ciclo de vida.

**Como estender:** adicione campo com valor padrão, leitura tolerante a dados
antigos, validação e teste de round-trip; incremente a versão se a
compatibilidade quebrar. Atualize `experiences`/`scene` quando necessário.

**Limites:** cenas com recursos muito novos podem ser rejeitadas por versão;
não há banco de dados ou migração automática de campos desconhecidos. A
serialização conhece o formato R6 atual, não esqueletos arbitrários.

Objetos Decal/Texture v3 persistem `surface` via EncodeSurface/DecodeSurface,
compartilhado com RigDecal. Leitura em candidato valida campos/face/UVs e cria
as superfícies após as Parts, independente da ordem do arquivo. Projetos antigos
continuam aceitos; parâmetros novos de adesivos usam defaults compatíveis.


**Material3D v7:** campo opcional por Part, validado antes de alterar a cena.
Sem ele, o escritor conserva a versão mínima v1–v6 anterior; não migra todos os
arquivos automaticamente. BaseColor, parâmetros físicos, emissão, UV e quatro
mapas passam pelo codec Material3D. Campo em versão anterior é rejeitado;
valores não finitos/fora de limites não alteram o mundo. Referências de mapas
entram na normalização/relocação portátil de imagens, com teste .lo em outro HOME.
