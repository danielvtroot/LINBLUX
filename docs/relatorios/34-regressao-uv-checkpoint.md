# Checkpoint de regressão e UV — 26/10/2026

IMPLEMENTADO/TESTADO: reflexão U opcional por atributos `decal_flip_u` e
`texture_flip_u` em peças; `flip_u` em componentes Surface. A imagem é preservada
e os projetos antigos conservam o UV anterior por padrão. A placa da demo
agora aponta a face Back para o spawn e opta pela correção, sem logo espelhada.
Teste Player verifica o desenho assimétrico da logo (glifo à esquerda), além
de executar o fluxo de inventário/combate e NPC animado. Reabertura/.lo
verificam a persistência do atributo. Código:SceneRenderer.cpp, gerador/mundo
RifleDemo e testes. Build Studio/Player/qualidade8,62s; build do teste nativo11,34s.

`linblux_graphics_quality` foi migrado: compõe o HUD circular de Runtime real,
mede arco/centro e confirma a ausência da barra RML, preservando os testes de
ícones/AA/transparência existentes. Qualidade+RifleDemo Player2/2 aprovados em
29,615s, log .turbo/logs/alpha-uv-final.log. Captura final inspecionada e
atualizada na documentação. Não existe aprovação FULL implícita.

BLOQUEADO/pendente: a migração de `linblux_rml_ui` atingiu o limite global180s
(183,043s incluindo encerramento), sem resultado/aprovação. Último artefato:
/tmp/loblox-rml-ui-YatF8O/pause-maximized-1366.png. Não concluir que o problema
é somente seletor: diagnosticar maximizar/retomar/F10/F11 nessa resolução.
O grupo gráfico foi encerrado pelo TURBO; nenhum processo fica executando.

A proposta de migração daquele teste foi guardada em
`34-rml-migration-wip.patch`. Só alterações nossas foram restauradas no arquivo
executável de teste para não publicar uma migração sem aprovação; WIP
já foi preservado e publicado em4e28879. Retomar com:

```sh
git apply --check docs/relatorios/34-rml-migration-wip.patch
# depois de conferir a árvore e o patch:
git apply docs/relatorios/34-rml-migration-wip.patch
```

Próxima investigação curta: adicionar uma seleção de cenário/resolução à
fixture RmlUi e reproduzir somente maximizar/retomar/F10/F11 em1366×768, com
TURBO/timeout individual. Evitar repetir a bateria longa inteira para localizar
o ponto exato. Depois concluir a migração, regressão abrangente e validar pacote
em outra instalação. A Release segue bloqueada também pela revisão GPL/raylib
registrada no relatório33, sem tag/publicação ou áudio físico aprovado.

FAST20261026T234643Z-16356:4/4 em6,7s (escopo conhecido). IMPACT
20261026T234748Z-16941:4/4 em3,6s, global parcial; as duas integrações gráficas
específicas foram executadas separadamente no log citado acima.
