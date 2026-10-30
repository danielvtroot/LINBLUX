# Limpeza do repositório Alpha

Data: 30/10/2026. Base: `7e140a263642ad2a140c080f382999848034199d`.
**11 arquivos excluídos e 1 relocalizado; 9 documentos consolidados.**
Markdown versionado: **154 → 147**; arquivos versionados: **504 → 496**.

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

## Tamanho e documentação ativa

Soma dos arquivos versionados no checkout: **15540747 → 15536737 bytes**,
redução de **4010 bytes**. Isso não reduz o histórico ou o tamanho de `.git`.
Os WIPs preexistentes têm os mesmos hashes antes/depois e não alteram a comparação.
Permanecem **60 documentos de uso/manutenção**: 29 públicos (25 guias em `docs/`
e README pt-BR/inglês, CONTRIBUTING e SECURITY), 30 manuais ATLAS/TURBO/backup e
um único AGENTS. Adaptadores OpenCode, templates, histórico e licenças são contados
separadamente. AGENTS foi reduzido de 195 para 72 linhas com as proteções essenciais.

## Validação e publicação

- **PASSOU:** diff completo sem erros de whitespace e 214 referências Markdown/imagem
  locais verificadas, sem destinos ausentes; referências CMake/instalação preservadas.
- **PASSOU:** build incremental de `linblux` e `linblux_player` em `build-linblux/`.
- **PASSOU:** 7 headless — experiences, game, tests, defaults,
  engine_default_lifecycle, item_actions e rifledemo (6,91 s).
- **PASSOU:** Python ATLAS 40/40 (52,62 s), TURBO 41/41 (18,15 s).
- **PASSOU:** empacotador, 85 hashes, executáveis/recursos/licenças presentes,
  manual de inventário com interações e links locais do pacote válidos.
  Player validou RifleDemo sem erros Lua, sem janela gráfica.
- A conferência inicial encontrou uma cópia redundante do README-fonte instalada em
  `share/linblux/`, com links inválidos. Ela foi retirada somente do staging do
  pacote; o README de instalação permanece. O pacote foi regenerado e aprovado.
- **NÃO EXECUTADO:** gráficos/FULL, pois não houve mudança de UI/renderização.
  Não se repetiu o smoke gráfico da missão anterior. Bloqueios de redistribuição
  Alpha continuam; nenhum binário ou tag foi publicado. Candidatos anteriores mantidos.

Commits: `3fffd5f` (documentação), `16f3ba9` (protótipo); o commit que contém a
versão final deste relatório reúne os ajustes do empacotador e o encerramento.
Push normal e igualdade HEAD local/remoto são conferidos na entrega; hashes e
resultado ficam em `.turbo/cleanup-alpha/final-git.json` (local). O commit de
encerramento é identificável por `git log -1 --format=%H -- docs/releases/LIMPEZA_REPOSITORIO_ALPHA.md`.

Pendências: WIPs e dados de papel incerto permanecem protegidos, não classificados
como lixo. A configuração legacy e as ferramentas de recuperação têm consumidores
e foram mantidas. Inventário e logs concisos de validação em `.turbo/cleanup-alpha/`.
