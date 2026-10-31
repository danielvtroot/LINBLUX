# Benchmarks e avaliação do ATLAS

Números medidos neste ambiente em **24 de outubro de 2026**. Não são garantias
para outra máquina, outro sistema de arquivos ou GPU. O benchmark mede o ATLAS,
não o desempenho do jogo.

## Ambiente

| Item | Valor |
| --- | --- |
| CPU | AMD Ryzen 5 3500U (4 núcleos/8 threads, 2.1 GHz máx.) |
| GPU | Radeon Vega integrada (sem medição de desempenho de GPU) |
| RAM disponível | não medida; pico do processo ATLAS registrado abaixo |
| Sistema | Linux 6.12.113+deb13-amd64, x86_64 |
| Disco | `/dev/sda2`, 916 GB, 788 GB livres no momento |
| Python | 3.13.5 |
| SQLite/FTS5 | 3.46.1 / disponível |
| Ferramentas semânticas C++ | ausentes (`clangd`, `rg`, `ctags`) |

## Metodologia

Comando:

```bash
python3 tools/atlas/atlas.py bench --iterations 5
```

O `bench`:

1. copia somente arquivos indexáveis para `.atlas/atlas-bench-*`, uma cópia
   temporária e isolada (não confundir com o fixture de testes);
2. mede uma indexação completa com `--force`;
3. altera um arquivo e mede o `update` incremental;
4. altera cinco arquivos e mede outro `update`;
5. executa as 12 perguntas de `eval_questions.json` cinco vezes contra o índice
   temporário (60 consultas);
6. mede o tamanho do banco e o pico de RSS do processo;
7. compara os caracteres da saída JSON do `locate` com a soma dos arquivos
   candidatos.

Foram feitas quatro execuções finais e três execuções consecutivas anteriores
após o cache de disco aquecido. A primeira execução logo após gravar os arquivos
foi mais lenta (indexação completa de 5.312 ms e atualização de um arquivo de
1.464 ms), por isso a tabela abaixo usa medições aquecidas. A variância é real e
não foi escondida.

## Indexação e atualização

A medição final foi feita com **235 arquivos indexáveis**, 2.505 símbolos, 895
relações, 34 targets e 34 testes declarados. Foram quatro execuções finais; elas
mostram a variância normal de disco/CPU. A execução mais lenta de atualização
(464 ms para 1 arquivo e 931 ms para 5) coincidiu com carga do ambiente e é
mantida na faixa — não foi descartada.

| Métrica | Quatro execuções finais | Faixa das 3 execuções anteriores |
| --- | ---: | ---: |
| Índice completo (`--force`) | 1.750–2.438 ms | 1.840–2.044 ms |
| Update de 1 arquivo | 202–464 ms | 201–213 ms |
| Update de 5 arquivos | 213–931 ms | 224–225 ms |
| Consulta — média (60) | 30,00–37,51 ms | 31,93–35,09 ms |
| Consulta — máxima | 51,99–104,59 ms | 46,48–88,09 ms |
| Pico de RSS | 30.288–30.540 KiB | 30.196–30.428 KiB |
| Banco SQLite (main + WAL/SHM) | 4.083.712–4.096.000 bytes | 4.046.848 bytes |

As execuções anteriores tinham 234 arquivos; a diferença de um arquivo é este
próprio documento de benchmark. O número de dependências externas no índice
temporário é menor que no índice real porque `third_party/` não é copiado como
código próprio; isso não afeta os tempos de consulta do projeto.

## Economia de contexto

Para a consulta ilustrativa `camera personagem`:

| Medida | Valor |
| --- | ---: |
| Saída JSON do `locate` | 8.008 caracteres |
| Soma dos arquivos candidatos no disco | 85.943 caracteres |
| Razão saída/arquivos | 0,0932 (≈9,3%) |

Isso mede **caracteres**, não tokens de um modelo específico. Também não mede a
qualidade da resposta: um arquivo pequeno e irrelevante seria “econômico”.

## Avaliação de perguntas reais

Comando:

```bash
python3 tools/atlas/atlas.py eval
```

Amostra mantida manualmente em `tools/atlas/data/eval_questions.json`:

| Métrica | Resultado |
| --- | ---: |
| Perguntas | 12 |
| Módulo esperado no top 1 | 12/12 |
| Módulo esperado no top 3 | 12/12 |
| Algum módulo esperado retornado | 12/12 |
| Símbolo esperado retornado | 12/12 |

O rank do símbolo varia (1 a 13) porque a lista de símbolos por módulo é curta e
o teste de avaliação procura o símbolo esperado entre os candidatos, não apenas
no primeiro. **12/12 não é precisão universal**: a amostra é pequena, foi
mantida pelas mesmas pessoas que ajustaram o glossário e não cobre todo o
domínio. Use o resultado para detectar regressões no ranking, não para prometer
qualidade de um modelo de linguagem.

## Testes executados

| Comando | Resultado |
| --- | --- |
| `python3 -m unittest discover -s tests/atlas -p 'test_*.py' -v` | 11 testes, OK, 19,98 s |
| `ctest --test-dir build-linblux -R '^linblux_atlas$' --output-on-failure` | 1 teste, OK, 18,00 s |
| `ctest ... -R '^linblux_(game|tests|defaults|engine_default_lifecycle)$'` | 4 testes, OK, 1,47 s |

A suíte completa da engine é mais lenta sob software rendering/Xvfb e não foi
usada como benchmark do ATLAS.

## Limitações

- Sem `clangd`, `rg` ou `ctags`, a extração C++ é regex e `medium`; os tempos
  não incluem análise semântica que não existe.
- A consulta de benchmark usa a cópia temporária do projeto, não um servidor ou
  banco remoto.
- “Caracteres” não são tokens; a economia de contexto depende do consumidor.
- Não há medição confiável de GPU integrada neste ambiente.
- Números de uma máquina não devem ser convertidos em SLA.
- A amostra de avaliação é pequena e mantida manualmente.

## Como reproduzir

```bash
python3 tools/atlas/atlas.py init --force
python3 tools/atlas/atlas.py update
python3 tools/atlas/atlas.py verify
python3 tools/atlas/atlas.py bench --iterations 5
python3 tools/atlas/atlas.py eval
python3 tools/atlas/atlas.py docs
```

Registre data, hardware, versão do Python/SQLite e desvios. Não edite os números
sem uma nova execução.

## ATLAS 2.0 — medições de 30 de outubro de 2026

Índice real reconstruído com **423 arquivos** (4.455 símbolos, 1.279 relações,
44 targets, 64 testes), banco de 7.503.872 bytes.

| Operação | Antes (auditoria) | Depois |
| --- | ---: | ---: |
| `update` sem alterações (cache quente) | ~930 ms | 98–101 ms |
| `locate` | 60–150 ms | 98–110 ms |
| `symbol` | ~70 ms | 65–69 ms |
| `tests-for` | ~70 ms | 66–67 ms |
| `impact` | ~90 ms | 68–74 ms |
| `diagnose` (novo) | inexistente | 115–227 ms |
| `doctor` | ~200 ms | 237 ms |
| `verify` | ~250 ms | 265–276 ms |

Cada linha é o mínimo/mediana de 3 execuções da CLI completa (processo novo,
cache de disco aquecido), medidas com `time.time()` em Python.

`bench --iterations 3` (cópia isolada em `.atlas/atlas-bench-*`):

| Métrica | Valor |
| --- | ---: |
| Indexação completa | 2.923,8 ms |
| Update de 1 arquivo | 310,6 ms |
| Update de 5 arquivos | 334,7 ms |
| Consulta — média (36) | 73,6 ms |
| Consulta — máxima | 106,6 ms |
| Pico de RSS | 35.908 KiB |
| Banco SQLite | 7.503.872 bytes |
| Caracteres ATLAS / arquivos candidatos | 15.576 / 114.710 (0,136) |

Saída de `diagnose` nos 8 sintomas da auditoria: 994–1.094 caracteres de texto
humano, contra 1.907 caracteres antes do limite `--max-chars` padrão.

O caminho rápido do `update` não lê arquivo inalterado (`bytes_read = 0`); um
arquivo só é lido quando `size`, `mtime_ns` ou `ctime_ns` mudam, e o SHA-256
decide se o conteúdo mudou de fato. A varredura de metadados dos 423 arquivos
continua sendo o custo dominante.
