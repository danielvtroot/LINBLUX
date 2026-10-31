# LINBLUX Studio 2.0 — execução incremental

## Recuperação de 29/10/2026

HEAD local e origin/main: `e1ff4416532365b872d779d434a4c7d4223406e4`, sem divergência.
ConstructionPlacement.{h,cpp} e ConstructionUI.cpp já estão no remoto.
O relatório anterior que os chamava de não publicados está desatualizado.
19 arquivos rastreados modificados + 2 documentos não rastreados foram preservados
em `.turbo/checkpoints/studio2-initial/` (manifesto SHA-256, patch e ZIP dos arquivos).
Não foram coletadas mídias, experiências ou builds nesse checkpoint.

## Ordem e critérios

1. Recuperação e regressões: corrigir aquisição de manipulação, contratos de ESC,
   capturas Player e restart; distinguir timeout por orçamento de deadlock.
2. Move/Scale: aquisição direta por face para Parts/Models; planos de rig não
   podem reativar silenciosamente os eixos no pivô para Parts.
3. Sliders: trilho/preenchimento/marcador e interação coerentes em Câmera, Áudio
   Player e biblioteca; backend real, teclado, mudo e retomada de volume.
4. Layout: reorganizar abas Início/Modelo/Personagem/Script/Visualizar/Jogo;
   secundários em menus/painéis contextuais; capturas 1366, 1600 e 1920.
5. Explorador: nomes novos previsíveis, busca/classe, seleção, F2, reparenting e
   serviços. Nunca renomear conteúdo existente nem apagar a pasta pessoal `s`.
6. Construção: simplificar apresentação mantendo ConstructionPlacement,
   prévia, contato OBB, cancelamento, histórico e persistência.
7. Builds: inventariar caches, processos e pacotes; só remover caminhos
   comprovadamente gerados e obsoletos depois de validar o build oficial.
8. Validação final: Player real, persistência, defaults, inventário, Lua, áudio,
   física, gizmos, snap, UI, `.lo` e pacote Linux. Sem declaração de Release
   enquanto houver regressão crítica.

Cada incremento: build incremental, testes focados supervisionados, FAST/IMPACT
conforme impacto, capturas inspecionadas, commit específico, push normal e HEAD
remoto conferido. O build oficial é `build-linblux`; iniciar com
`./build-linblux/linblux` e consultar o Player com
`./build-linblux/linblux_player --help` (help não substitui teste gráfico).

## Diagnóstico inicial das seis regressões

- Precision UI ainda clicava nas coordenadas do gizmo antigo, apesar de Move
  usar aquisição de face. A regressão precisa exercitar a face real e provar
  movimento, dimensão imutável e contato; não reduzir asserções.
- Script input esperava Paused no ESC, contrariando o contrato publicado:
  menu captura input, mas simulação continua. Verificar ambos explicitamente.
- Lua UI Player comparava pixels com RGB sem considerar o alfa E6 solicitado
  pelo próprio exemplo; validar composição e localização do painel.
- RifleDemo esperava 11 segundos de simulação usando atraso fixo de relógio real;
  verificar a conclusão das mesmas asserções por marcador, com prazo limitado.
- Models recebeu só 16,5 segundos restantes no IMPACT. Repetição isolada passou
  em 51,16 segundos, abaixo do timeout CTest existente: não há evidência de deadlock.
- Restart: clique emitido durante os primeiros quadros de carregamento não era
  processado. Eventos separados entre quadros resolveram a fixture; timeout
  preservado, repetição passou em 28,95 s.

## Escopo ainda não autorizado como concluído

Layout abrangente, Explorador, simplificação adicional da construção e limpeza
permanecem NÃO INICIADOS nesta execução. WIPs de grade, nomenclatura e áudio
continuam separados até revisão/validação. Não iniciar gameplay ou trocar backend.
