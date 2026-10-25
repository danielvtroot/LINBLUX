# Workflow: analisar impacto

Use antes de alterar cabeçalhos, serialização, cenas, física, Lua ou UI
compartilhada.

## 1. Impacto estrutural

```bash
python3 tools/atlas/atlas.py impact src/scene/Scene.h --depth 2 --limit 30
python3 tools/atlas/atlas.py impact Scene --json
```

O resultado mostra:

- níveis de dependência reversa por include;
- dependências de target CMake;
- arquivos afetados e contagem;
- testes mapeados;
- riscos específicos do módulo;
- contrato manual, se existir.

## 2. Testes associados

```bash
python3 tools/atlas/atlas.py tests-for src/scene/Scene.h
```

A associação pode vir de:

- fonte do alvo do teste;
- include reverso (teste inclui o cabeçalho);
- alvo CMake do arquivo.

Ela não prova cobertura funcional. Abra o teste e confirme o caso.

## 3. Consumidores indiretos

Aumente a profundidade somente se necessário:

```bash
python3 tools/atlas/atlas.py impact src/core/Config.h --depth 3
```

O BFS limita visitas para evitar explosão. Se o limite for atingido, o resultado
é parcial; investigue por módulo com `--module` no `locate` ou por includes
diretos.

## 4. Riscos que o grafo não vê

- chamadas por função virtual, callback, Lua ou evento;
- macros que geram código;
- dependências condicionais de CMake;
- dados persistidos em `.lo`, JSON ou arquivos de experiência;
- mudanças de layout de UI sem teste visual.

Para dados persistidos, procure testes de ciclo de vida e compatibilidade antes
de mudar campos.

## 5. Plano mínimo de validação

1. compilar os targets afetados;
2. rodar testes diretos;
3. rodar um teste de integração por módulo tocado;
4. para UI/render, validar visualmente ou com teste sob Xvfb;
5. atualizar o índice e repetir `doctor`/`verify`.
