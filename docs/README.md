# Documentação LINBLUX

Comece pelo [estado da Alpha 0.1.0](ALPHA_0_1_0.md): instalação, candidato Linux,
validação e limites. A interface oficial atual é em português brasileiro.

## Usar e criar

| Guia | Conteúdo |
| --- | --- |
| [Studio](GUIA_STUDIO.md) | Cena, seleção, painéis e fluxo de autoria |
| [Player](PLAYER.md) | Abrir experiências, controles e limites de execução |
| [Experiências](EXPERIENCES.md) | Biblioteca, manifestos, importação/exportação `.lo` |
| [Construção e precisão](EDITOR_PRECISAO.md) | Move/Scale por face, Snap e colocação por superfície |
| [Modelos](MODELOS.md) · [Attachments](ATTACHMENTS.md) | Agrupamento, pivôs e conexões |
| [Imagens](RECURSOS_DE_IMAGEM.md) · [Áudio](AUDIO_MUSICA.md) | Importar recursos locais e usar em cenas |
| [Inventário](INVENTARIO.md) | Itens, coleta, hotbar e descarte |
| [Padrões da engine](PADROES_DA_ENGINE.md) | Mundo e personagem usados em experiências novas |
| [Galeria](MIDIA.md) | Capturas reais e procedimento para vídeos |

## Programar

- [API Lua e exemplos](SCRIPTING_LUA.md): objetos, eventos, física e recursos.
- [UI Lua](UI_LUA.md): widgets e limites da interface em execução.
- [Arquitetura](ARCHITECTURE.md): targets e separação entre autoria e runtime.
- [Compatibilidade](COMPATIBILIDADE.md): plataformas, nomes legados e formatos.

## Compilar e contribuir

- [Build Linux](BUILD.md): dependências e diretório oficial `build-linblux/`.
- [Contribuição](../CONTRIBUTING.md): mudanças, relatos de bugs e revisão.
- [Testes](TESTING.md): headless, supervisão gráfica e leitura dos resultados.
- [ATLAS](atlas/README.md) e [TURBO](turbo/README.md): investigação e validação locais.
- [Contratos internos](atlas/module-contracts/): invariantes por módulo.
- [Vídeo](VIDEO.md): backend opcional e fallback do Início.
- [Recursos e licenças](ASSETS_E_LICENCAS.md): proveniência e redistribuição.
- [Backup e restauração](BACKUP_E_RESTAURACAO.md): recuperação de trabalho.

## Versões e registros

[Changelog](CHANGELOG.md) · [Roadmap](ROADMAP.md) ·
[Relatório da Alpha](releases/ALPHA_0_1_0_LANCAMENTO.md)

[Relatórios técnicos](relatorios/) e [histórico](historico/) preservam decisões,
checkpoints e resultados de suas respectivas revisões. Não são um catálogo atual
de recursos nem prova de que testes antigos passaram nesta versão. Logs, caches
e builds locais ficam fora do Git; nenhum histórico foi apagado nesta organização.
