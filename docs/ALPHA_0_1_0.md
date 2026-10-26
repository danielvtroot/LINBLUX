# LINBLUX Alpha 0.1.0 — candidato Linux

Estado: **BLOQUEADO para Release pública**. Nenhuma tag é criada pelo empacotador.
A versão de trabalho contém Studio/Player, Lua, Bullet, modelos/Attachments,
itens, E/Delete/hotbar, áudio local, decals, NPCs, UI Lua, HUD circular e menu
lateral nas interfaces padrão. Consulte os relatórios30–32 para evidência real.

## Gerar e executar um candidato interno

Após compilar e validar com ATLAS/TURBO:

```sh
python3 tools/package_linux.py --build-dir build-linblux
```

O `.tar.gz` e seu SHA-256 ficam em `.turbo/dist/` (ignorados). Extraia e execute:

```sh
./linblux-alpha-0.1.0/bin/linblux
./linblux-alpha-0.1.0/bin/linblux_player --project ./linblux-alpha-0.1.0/share/linblux/examples/rifledemo/world.loblox.json
```

Não é necessário abrir CMake para usar o arquivo extraído. O candidato inclui
raylib com seu SONAME e avisos; as outras bibliotecas são dependências do sistema
listadas por `ldd` no manifesto. Este build usa glibc/libstdc++, Bullet3.24,
Lua5.4, FFmpeg61/59, FreeType/OpenGL/X11 e fontes DejaVu/Liberation do ambiente.
Não foi comprovado suporte universal a outras distribuições nem a uma instalação
nova do sistema operacional. Cada arquivo tem SHA-256 no manifesto.

A Home de desenvolvimento preserva o vídeo original. O instalador e o pacote
público o omitem por padrão: `LINBLUX_INSTALL_BACKGROUND_VIDEO=ON` exige direitos
confirmados. O candidato mostra o fallback próprio quando o vídeo não existe.
Experiências pessoais, imagens ignoradas, configurações, credenciais e logs não
entram no pacote.

## Portas de lançamento ainda abertas

- Revisar obrigações da build FFmpeg GPL efetivamente usada, além dos componentes
  embutidos da raylib redistribuída; avisos técnicos não constituem liberação jurídica.
- A migração da regressão RmlUi excedeu180s após maximizar em1366×768, sem
  aprovação. Diagnosticar maximizar/retomar/F10/F11 e concluir a migração
  antes da regressão abrangente. O patch recuperável está no relatório34.
- O modo de depuração `--ui legacy` conserva sua apresentação antiga. A interface
  Alpha suportada é RmlUi; o Player independente possui o novo menu.
- Validar o candidato extraído e dependências em outra instalação Linux.
- Áudio físico só pode ser aprovado em ambiente com dispositivo real disponível;
  não confundir a voz headless com reprodução audível.

Fora do Alpha: multiplayer, rigs/malhas arbitrários, mixagem/atenuação3D,
inventário de partida salvo automaticamente entre sessões, navegação em qualquer
terreno e widgets UI complexos. Não são anunciados como prontos.
