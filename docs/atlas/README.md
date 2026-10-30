# LINBLUX ATLAS

Sistema local de inteligência estrutural, navegação, impacto e diagnóstico do
LINBLUX. A interface principal é uma CLI Python independente de OpenCode, rede,
servidor ou serviço externo. O índice é reconstruível e fica em `.atlas/`, fora do
Git.

## Início rápido

```bash
python3 tools/atlas/atlas.py init
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py status
python3 tools/atlas/atlas.py locate "a camera nao acompanha o personagem"
python3 tools/atlas/atlas.py symbol EditorCamera
python3 tools/atlas/atlas.py impact src/rendering/EditorCamera.h
python3 tools/atlas/atlas.py tests-for src/rendering/EditorCamera.h
python3 tools/atlas/atlas.py doctor
python3 tools/atlas/atlas.py verify
```

Fluxo mínimo para qualquer agente de programação:

```bash
python3 tools/atlas/atlas.py update                       # índice incremental
python3 tools/atlas/atlas.py diagnose "sintoma do bug"    # ≤ ~1.100 caracteres
# abrir somente os arquivos/linhas indicados, editar, então:
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.cpp
python3 tools/turbo/turbo.py test --mode fast --changed
```

Todas as consultas aceitam `--json`, `--root <diretório>` e limites. Exemplos:

```bash
python3 tools/atlas/atlas.py locate "salvar mundo padrao" --limit 5 --json
python3 tools/atlas/atlas.py impact src/scene/Scene.h --depth 3 --limit 20
python3 tools/atlas/atlas.py text "ExperienceManager" --limit 5
python3 tools/atlas/atlas.py trace "carregamento do mundo padrao"
python3 tools/atlas/atlas.py docs
python3 tools/atlas/atlas.py eval
python3 tools/atlas/atlas.py bench --iterations 5
```

## Comandos

| Comando | Função |
| --- | --- |
| `init` | cria o índice completo; `--force` remove o cache antes |
| `update` | detecta novos/alterados/removidos por SHA-256 e atualiza em transação |
| `status` | contagens, tamanho, defasagem estimada e ferramentas disponíveis |
| `locate` | ranking explicável de módulos/arquivos/símbolos/testes para um sintoma |
| `symbol` | definições/declarações e referências textuais de um símbolo |
| `text` | pesquisa textual FTS5 com trecho |
| `impact` | consumidores diretos/indiretos, testes, riscos e documentação |
| `trace` | fluxo provável a partir de uma pergunta |
| `tests-for` | testes associados, com motivo e confiança (`include`, target, cadeia) |
| `diagnose` | resposta curta: arquivos, linha, símbolo, hipóteses, testes e próximo passo |
| `fix-plan` | plano de correção somente leitura, passo a passo, sem editar código |
| `doctor` | inconsistências estruturais com severidade |
| `verify` | integridade SQLite/FTS, versão, hashes e consulta de fumaça |
| `docs` | regenera `docs/atlas/generated/inventory.md` |
| `bench` | mede indexação, atualização, consulta, memória e tamanho |
| `eval` | mede o ranking em perguntas reais mantidas manualmente |

## Camadas implementadas

- **Registro estrutural:** arquivos, módulos, linguagens, símbolos C++/Lua/Python/
  RML/Markdown, targets CMake, testes e dependências externas.
- **Grafo:** includes resolvidos, `lua-require`, fontes/dependências de targets
  CMake, testes e associações. Relações não confirmadas são `inferred`.
- **Documentação viva:** inventário gerado + contratos manuais separados em
  `docs/atlas/module-contracts/`.
- **Localizador:** FTS5 BM25 + caminho + glossário de domínio + símbolos + testes,
  com pesos transparentes em `score_breakdown`.
- **Impacto/validação:** BFS reverso limitado, testes mapeados, riscos e
  comandos de validação.
- **Interfaces universais:** CLI, JSON, Markdown, `AGENTS.md`, skills e comandos
  OpenCode. MCP local fica como extensão futura.

## Fonte da verdade e cache

- Fonte da verdade: código, `CMakeLists.txt`, testes, assets versionados e
  contratos manuais.
- Cache: `.atlas/index.sqlite` (+ WAL/SHM), ignorado pelo Git e reconstruível.
- O índice guarda SHA-256 do conteúdo; `update` não depende só de mtime.
- `doctor` usa tamanho/mtime como estimativa rápida; `verify` recalcula hashes.
- Bibliotecas terceiras não são indexadas como código próprio; aparecem em um
  inventário resumido (`external_deps`). Builds, caches, vídeos, binários e logs
  são excluídos.

## Precisão e limitações

Este ambiente não possui `clangd`, `rg` nem `ctags`. A extração C++ é textual/
regex e marcada como `origin=regex-cpp`, `confidence=medium`. Ela não resolve
namespaces, sobrecargas, templates ou chamadas dinâmicas com precisão semântica.
Lua, RML, Python e Markdown também usam extração própria. O ATLAS nunca deve ser
apresentado como um parser C++ completo.

O `locate` é determinístico e explicável, mas não “entende” linguagem natural
como um modelo. Consultas vagas retornam candidatos e podem usar módulos
sugeridos pelo glossário, marcados como inferidos. Sempre confirme no código
real e nos testes.

## Segurança

Consultas são somente leitura. `update` grava apenas em `.atlas/`; `docs`
regenera `docs/atlas/generated/inventory.md`, que é versionado de propósito.
O ATLAS não executa scripts indexados, não usa rede, valida caminhos dentro da
raiz e usa SQL parametrizado. Conteúdo de arquivos é tratado como dado, nunca
como instrução. Não use `git reset --hard` ou push forçado como parte do fluxo.

## Testes e evolução

```bash
python3 -m unittest discover -s tests/atlas -v
ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure
```

Consulte:

- [`architecture.md`](architecture.md) — desenho interno, esquema e ranking.
- [`json.md`](json.md) — contrato JSON.
- [`workflows/`](workflows/) — navegação, depuração, impacto e validação.
- [`module-contracts/`](module-contracts/) — contratos por módulo.
- [`generated/inventory.md`](generated/inventory.md) — inventário regenerável.
- [`benchmarks.md`](benchmarks.md) — medições e metodologia, sem números inventados.
- [`troubleshooting.md`](troubleshooting.md) — falhas comuns do próprio ATLAS.

Para adicionar um novo módulo: crie o diretório em `src/`, registre o target no
CMake, rode `atlas.py update`, adicione um contrato manual e teste o ranking com
`locate`, `impact` e `tests-for`. Não é necessário reescrever o ATLAS.
