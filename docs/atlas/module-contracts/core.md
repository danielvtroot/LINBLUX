# Módulo `core`

**Responsabilidade:** tipos e utilitários fundamentais — configuração, vetores/
quaternões, geometria orientada, validação de imagem e diagnóstico estruturado.

**Fonte:** `src/core/{Config,Diagnostics,Geometry,ImageInfo,Math,NumberFormat}.{h,cpp}`.

**Entradas:** parâmetros numéricos, caminhos de configuração/assets, texto de
diagnóstico e dados de imagem.

**Saídas:** estruturas de configuração, funções matemáticas determinísticas,
caixas orientadas, metadados seguros de imagem e eventos de log.

**APIs principais:** `AppConfig`, `DefaultProjectPath`, `Finite`, `V3`,
`QuatFromEulerDegrees`, `CanonicalQuaternion`, `QuaternionSlerpShortest`,
`OrientedBox`, `ReadSafeImageInfo`, `diag::configure/log/setFrame`.

**Invariantes:** conversões de quaternion são determinísticas e contínuas;
funções de validação não alteram estado; `Diagnostics` não deve depender de UI.
`ImageInfo` valida limites antes de expor dimensões.

**Dependências:** biblioteca padrão; `ImageInfo` usa raylib; não possui cena,
física, Lua ou ImGui.

**Ciclo de vida e erros:** funções livres e estruturas de valor; não há
singleton de domínio. `diag` mantém estado de logging configurável; erros de
arquivo/parse são retornados por valor/booleano e descrição.

**Testes:** `linblux_tests`, `linblux_manipulation`, `linblux_defaults` e testes
de câmera/gizmo que dependem de `Math`.

**Como estender:** prefira funções puras e determinísticas; adicione teste
numérico antes de mudar convenções de euler/quaternion. Não introduza dependência
de UI ou de `Scene` neste módulo.

**Limites:** não é uma biblioteca matemática genérica; convenções de eixos e
ordem de rotação são específicas do LINBLUX. `ReadSafeImageInfo` não decodifica
a imagem inteira.
