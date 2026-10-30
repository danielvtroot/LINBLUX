# Limpeza do repositório Alpha

Data: 30/10/2026. Base: `7e140a263642ad2a140c080f382999848034199d`.
Limpeza executada; resultados finais de validação serão registrados após os checks.

## Inventário e decisões

- **Manter:** engine, Studio/Player, dependências, Lua, recursos oficiais, fixtures,
  ferramentas ATLAS/TURBO/backup, scripts de manutenção, templates GitHub e adaptadores OpenCode.
- **Consolidar:** três guias curtos nos manuais atuais e seis etapas da migração em
  um registro histórico com textos, evidências e hashes preservados.
- **Remover:** rascunho de Release substituído e protótipo sem target/consumidores.
- **Histórico:** checkpoint de continuidade deslocado para `docs/historico/`;
  demais regressões, licenças, decisões, patches e checkpoints mantidos.
- **Proteger:** WIPs de Scene, três relatórios não rastreados, experiências, mídia
  pessoal, builds alternativos, candidatos Alpha e logs. Nada disso foi apagado.

## Caminhos retirados e destinos

| Origem | Motivo / destino |
| --- | --- |
| `docs/INTERACOES.md` | Consolidado em `docs/INVENTARIO.md`, incluindo API, limites e testes |
| `docs/EXCLUSAO_SEGURA.md` | Consolidado em `docs/EXPERIENCES.md`, preservando proteções POSIX e falhas parciais |
| `docs/LOCALIZACAO_E_TEMA.md` | Incorporado a `CONTRIBUTING.md` |
| `docs/relatorios/01-seguranca.md` | Migração histórica consolidada, seção Etapa 01 |
| `docs/relatorios/02-identidade.md` | Migração histórica consolidada, seção Etapa 02 |
| `docs/relatorios/03-interface.md` | Migração histórica consolidada, seção Etapa 03 |
| `docs/relatorios/04-experiencias.md` | Migração histórica consolidada, seção Etapa 04 |
| `docs/relatorios/05-padroes.md` | Migração histórica consolidada, seção Etapa 05 |
| `docs/relatorios/06-independencia-e-validacao.md` | Migração histórica consolidada, seção Etapa 06 |
| `docs/CONTINUIDADE.md` | Movido para `docs/historico/CONTINUIDADE_2026_10_24.md`, com aviso de validade histórica |
| `docs/releases/v0.1.0-alpha.1.md` | Rascunho redundante; notas atuais em `ALPHA_0_1_0_NOTAS.md` |
| `prototype/legacy_main.cpp` | Main independente, sem includes da engine, target CMake ou consumidores em scripts/Lua/empacotamento; removido |

Os seis relatórios estão completos em
[Migração 24/10](../historico/MIGRACAO_LINBLUX_2026_10_24.md), com igualdade do
conteúdo verificada antes da exclusão. Origens removidas continuam recuperáveis
na revisão-base do Git. Não houve reescrita de histórico.

## Escopo preservado

Nenhuma imagem, vídeo, som, exemplo Lua ou asset foi excluído. A única duplicata
binária exata identificada, logo na demo e em `midia/`, é necessária à portabilidade
da experiência. Arquivos gerados encontrados sob `tests/atlas/fixture/build/`
são fixtures intencionais. Não havia objetos, bibliotecas, caches ou logs rastreados
indevidamente; `.gitignore` já cobre essas categorias e não precisou ser alterado.

`build-linblux/` permanece oficial e intacto. `build/` tem caminho antigo e
`build-no-video/` é configuração alternativa; não foram apagados, pois o cache
não prova ausência de dados exclusivos. Backups/candidatos/logs não foram varridos
nem limpos. OpenCode permanece opcional; ATLAS/TURBO funcionam pelo terminal.

## Validação e publicação

Pendente neste primeiro registro: links finais, build incremental Studio/Player,
headless afetados, testes Python ATLAS/TURBO e conferência do empacotador.
Não há mudança de UI/renderização e não será executada suíte gráfica por rotina.
Bloqueios de redistribuição do binário Alpha continuam vigentes.
