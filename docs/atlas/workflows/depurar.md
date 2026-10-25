# Workflow: diagnosticar um problema

## 1. Descreva o sintoma

Escreva o comportamento observado, o esperado e o contexto (Studio, Player,
Play, importação, UI, físico, Lua). Uma frase vaga ainda pode ser usada, mas
quanto mais específica, melhor o ranking.

## 2. Colete candidatos

```bash
python3 tools/atlas/atlas.py locate "sintoma em português" --limit 8
python3 tools/atlas/atlas.py locate "sintoma" --json > /tmp/atlas-locate.json
```

Verifique se os módulos fazem sentido. Se não, tente termos do domínio:
`camera`, `rig`, `personagem`, `mundo padrao`, `salvar`, `fisica`, `interface`,
`experiencia`, `video`, `animacao`.

## 3. Siga o fluxo provável

```bash
python3 tools/atlas/atlas.py trace "carregamento do mundo padrao" --limit 4
python3 tools/atlas/atlas.py symbol NomeDoSimbolo
```

Abra os arquivos reais indicados. O `trace` mostra um roteiro; não prova que uma
função foi chamada.

## 4. Reproduza antes de corrigir

- Se houver teste automatizado próximo, execute-o primeiro.
- Se for UI/renderização, prefira um teste focado ou inspeção visual sob Xvfb.
- Registre comando, entrada, saída e ambiente.

```bash
ctest --test-dir build-linblux -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$' --output-on-failure
```

## 5. Formule uma hipótese verificável

Exemplo: “`Scene::buildDefaultScene` não instala o script padrão quando o
manifesto não tem `scripts`”. A hipótese deve prever um teste que falha antes e
passa depois.

## 6. Faça a menor correção

- Respeite o contrato do módulo em `docs/atlas/module-contracts/`.
- Não “conserte” um sintoma em outro módulo sem evidência.
- Preserve dados e experiências do usuário.

## 7. Valide impacto

```bash
python3 tools/atlas/atlas.py impact src/modulo/arquivo.cpp --depth 2
python3 tools/atlas/atlas.py tests-for src/modulo/arquivo.h
```

Rode os testes focados, depois os testes de integração afetados. A suíte completa
é lenta sob software rendering; não a omita se a mudança tocar renderização,
UI ou ciclo de vida.

## 8. Encerre com evidências

Informe:

- causa confirmada;
- arquivos e linhas alterados;
- comandos executados e resultados;
- o que permanece hipótese ou não foi testado.
