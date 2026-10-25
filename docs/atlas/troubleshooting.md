# Troubleshooting do ATLAS

## “índice ausente”

```bash
python3 tools/atlas/atlas.py init
```

## “índice incompatível” ou corrompido

O `update` tenta recuperar automaticamente. Para forçar:

```bash
python3 tools/atlas/atlas.py init --force
python3 tools/atlas/atlas.py verify
```

O cache `.atlas/` é descartável; nunca é a fonte da verdade.

## Índice desatualizado

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py status
```

`doctor` usa tamanho/mtime; `verify` recalcula SHA-256.

## `locate` não retorna nada

1. Verifique se o índice tem arquivos: `status`.
2. Use termos do domínio, não uma frase longa.
3. Tente `--module <módulo>` para restringir.
4. Verifique `total_files_considered` no JSON.
5. Adicione sinônimos ao glossário somente após confirmar o vocabulário real.

Ausência de resultado é informação: o índice não encontrou correspondência
textual nem sugestão de glossário.

## Muitos candidatos irrelevantes

- `docs`, `assets` e `tools/atlas` têm peso menor que `src/`.
- Use `--module` quando souber a área.
- Confira `score_breakdown`; FTS alto em documentação pode ofuscar código se o
  termo só existe em docs.
- Não trate `inferred=true` como evidência textual.

## Símbolos C++ faltando ou duplicados

- O ambiente não tem `clangd`/`ctags`; o parser é regex.
- Macros, templates complexos, funções geradas e assinaturas multilinha podem
  faltar.
- Declaração e definição aparecem como entradas separadas.
- Homônimos são esperados; use `symbol Nome` e confirme namespace/arquivo.
- Se uma ferramenta semântica for instalada, reavalie a limitação antes de
  aumentar a confiança do índice.

## Includes de sistema “não resolvidos”

É esperado. `GL/gl.h`, `RmlUi/Core.h`, `X11/Xlib.h` e similares são externos.
O `doctor` os classifica como baixa severidade; não os trate como arquivos
faltantes do projeto.

## Testes condicionais

`add_test` dentro de `if(CMake)` é indexado, mas o ATLAS não avalia a condição.
Confirme no build real:

```bash
ctest --test-dir build-linblux -N
```

## Testes muito lentos

A suíte completa usa software rendering e Xvfb. Para mudanças localizadas:

```bash
ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

Não conclua que a suíte inteira passou sem executá-la quando o risco exigir.

## `python3 tools/atlas/atlas.py` falha com `ModuleNotFoundError`

Execute a partir da raiz do repositório ou use `--root`:

```bash
python3 tools/atlas/atlas.py --root /caminho/linblux status
```

O entry point injeta `tools/atlas` no `sys.path`; não é necessário instalar
pacote.

## Espaço em `/tmp`

O benchmark e os testes usam `.atlas/` dentro do projeto, não `/tmp`, para evitar
tmpfs pequeno. Se `/tmp` estiver cheio, limpe arquivos temporários antigos e
verifique `df -h /tmp`.

## `verify` falha em `query-smoke`

A consulta usa o primeiro símbolo de classe/struct/interface indexado em
`src/` com pelo menos 4 caracteres; se não houver, usa o primeiro símbolo de
código-fonte. Se o índice não tiver símbolos ou o `locate` não retornar
candidatos, verifique o parser e rode `init --force`. Em um projeto sem símbolos,
isso é um problema de indexação, não de busca.

## Como reportar um problema do ATLAS

Inclua:

- comando exato e saída com `--json`;
- `status`, `doctor` e `verify`;
- versão do Python/SQLite;
- arquivo ou consulta que reproduz;
- se possível, um fixture mínimo em `tests/atlas/fixture/`.

Não anexe experiências, segredos, vídeos ou caches `.atlas/`.
