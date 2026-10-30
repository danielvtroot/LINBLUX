# LINBLUX Alpha 0.1.0

**Código e documentação publicados em `main`; distribuição binária pública bloqueada.**
O candidato interno verifica o empacotamento. Não é uma Release aprovada nem um
pacote Linux universal. Veja o [relatório desta missão](releases/ALPHA_0_1_0_LANCAMENTO.md)
para commits, resultados e SHA-256.

## Experimentar agora

Compile seguindo [BUILD.md](BUILD.md), abra `build-linblux/linblux` ou execute:

```sh
./build-linblux/linblux_player --project assets/examples/rifledemo/world.loblox.json
```

A distribuição usa **LINBLUX Alpha 0.1.0** e CMake `0.1.0`. A tag sugerida é
`v0.1.0-alpha.1`; não se afirma que ela exista ou que uma Release tenha sido
publicada. A interface Alpha suportada é RmlUi; `--ui legacy` é depuração.

## Candidato Linux interno

Após compilar e validar:

```sh
python3 tools/package_linux.py --build-dir build-linblux
```

O `.tar.gz` e seu `.sha256` ficam em `.turbo/dist/` (ignorados pelo Git). Extraia,
entre em `linblux-alpha-0.1.0/` e execute:

```sh
./bin/linblux
./bin/linblux_player --project ./share/linblux/examples/rifledemo/world.loblox.json
```

O pacote contém Studio, Player, recursos oficiais selecionados, exemplos, avisos
e raylib com seu SONAME. O manifesto lista bibliotecas do sistema e hashes.
O ambiente atual usa Linux x86_64, glibc/libstdc++, Bullet 3.24, Lua 5.4,
FFmpeg 61/59, FreeType, OpenGL/X11 e fontes DejaVu/Liberation instaladas.
Não há confirmação em outra instalação Linux.

O vídeo do Início fica fora do pacote e a Home usa fallback estático.
Experiências pessoais, caches, logs e imagens importadas privadas não são instalados.
`source_dirty` no manifesto indica alterações locais preservadas: o candidato não
deve ser confundido com um binário reproduzível de uma tag pública limpa.

## Evidência desta missão

- Build incremental completo em `build-linblux/` aprovado.
- Explorador: seleção, filtro, expansão e rolagem em 1366×768 e 1920×1080 aprovados.
- Construção: prévia, contato, ESC, undo/redo e salvar/reabrir aprovados; sem grade visual.
- Áudio: widgets RmlUi e menu nativo aprovados com sink null, incluindo valores,
  arraste, teclado e mudo/desmudo. Isso não comprova reprodução em alto-falantes.
- Decal e imagens: regressões aprovadas com fixtures próprias; UVs não foram invertidas.
- Física e persistência: testes de núcleo e integração de autoria/Play/Stop aprovados.
- RifleDemo no Player: inventário, tiro, recarga, dano e recoleta aprovados.

A regressão do Explorador precisou aguardar o layout após filtrar e clicar no
botão de expansão real. A fixture RifleDemo passou a esperar o inventário equipar
em vez de depender de uma duração fixa de tecla em renderização por software.
As tentativas com falha constam no relatório; não foram contadas como aprovação.

## Bloqueios e limites

- FFmpeg vinculado declara GPL v2 ou posterior. Falta concluir as obrigações de
  redistribuição e registrar fontes/configuração correspondentes.
- Os componentes embutidos da raylib redistribuída ainda exigem inventário e avisos
  completos; copiar só o comentário do header não conclui essa revisão.
- Não houve validação em outra distribuição, GPU física ou dispositivo de áudio real.
- Nenhuma suíte FULL nem repetição abrangente de maximizar/F10/F11 nesta missão.

[Licenças e recursos](ASSETS_E_LICENCAS.md) detalha a proveniência. Não se declara
liberação jurídica do binário. A publicação do código e dos manuais não depende
de anunciar um pacote aprovado.

Fora desta Alpha: multiplayer, contas, publicação online, malhas/rigs arbitrários,
terreno avançado, mixagem/atenuação 3D, inventário salvo automaticamente entre
partidas e widgets Lua complexos. Fit proporcional ampliado fica no roadmap.
