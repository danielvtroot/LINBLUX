# Workflow: navegar no código

Objetivo: ir de um sintoma ou termo até os arquivos e símbolos certos sem abrir
o repositório inteiro.

## 1. Atualize o índice

```bash
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py status
```

Se `status` indicar índice ausente ou incompatível:

```bash
python3 tools/atlas/atlas.py init --force
```

## 2. Localize candidatos

```bash
python3 tools/atlas/atlas.py locate "a camera nao acompanha o personagem" --limit 5
python3 tools/atlas/atlas.py locate "o mundo padrao nao salva" --json
```

Leia `confidence`, os módulos e `score_breakdown`. Candidatos marcados
`inferred` vieram do glossário e exigem confirmação.

## 3. Abra o símbolo, não o arquivo inteiro

```bash
python3 tools/atlas/atlas.py symbol PlayerCamera
python3 tools/atlas/atlas.py symbol RuntimeSession::start
python3 tools/atlas/atlas.py text "setDefaultCharacter" --limit 5
```

Confirme a linha no arquivo real. O ATLAS pode listar declaração e definição
separadas; `origin=regex-cpp` e `confidence=medium` são esperados em C++.

## 4. Veja relações e testes

```bash
python3 tools/atlas/atlas.py tests-for src/game/Runtime.h
python3 tools/atlas/atlas.py impact src/game/Runtime.h --depth 2
```

`tests-for` é associação estrutural. Não significa que o teste cobre todos os
caminhos ou que ele passa.

## 5. Registre o que foi confirmado

Separe:

- **fato:** linha/arquivo/teste confirmado no código;
- **hipótese:** comportamento provável ainda não reproduzido;
- **ausência:** informação que o índice não contém.

Próximo passo típico: `docs/atlas/workflows/depurar.md`.
