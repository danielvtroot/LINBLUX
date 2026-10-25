# Recursos, créditos e licenças

Esta auditoria identifica proveniência técnica e pendências; não elimina riscos
jurídicos nem substitui uma revisão de licenças/marcas para distribuição.

| Recurso | Classificação e destino |
| --- | --- |
| midia/logo.png | fornecido pelo responsável como logotipo oficial LINBLUX; original preservado, sem alteração do arquivo |
| midia/careditfullhd.mp4 | fornecido como vídeo oficial; direitos de gravação, marcas visíveis e trilha precisam de documentação antes de redistribuição comercial; áudio não é aberto pelo menu |
| midia/lock.png | símbolo decorativo fornecido, finalidade/licença não documentadas; não utilizado nos controles nem instalado |
| assets/builtin/character-face.png | visor geométrico original criado no código desta migração; uso sob a licença do projeto |
| assets/defaults e ícones procedurais | recursos de cena/código próprios; tipo serializado legado preservado |
| assets/*.png de nomes hash | imagens pessoais, licença desconhecida; uma contém marca de terceiros. Preservadas localmente, removidas do índice Git atual e da distribuição instalada |
| tests/fixtures/surface-test.png | padrão geométrico original para testes de filtragem, substitui dependência de imagem de terceiros |
| tests/fixtures/image-test.jpg | padrão geométrico original regenerado nesta migração, somente para teste do decodificador JPEG |

O inventário dos PNGs pessoais está em ASSETS_INVENTARIO.json com SHA-256.
O backup inicial e os commits antigos ainda os contêm; o histórico não foi
reescrito. Isso preserva recuperação, mas não equivale a uma autorização de uso.
Experiências pessoais que usam esses caminhos locais continuam encontrando os
arquivos neste computador. Um clone novo não inclui essas imagens ignoradas.

## Componentes

| Componente | Licença / registro |
| --- | --- |
| código próprio | MIT, LICENSE; copyright anterior preservado como aviso legal |
| Dear ImGui | MIT, third_party/imgui/LICENSE.txt |
| rlImGui | licença zlib, third_party/rlimgui/LICENSE |
| stb_image (JPEG) | MIT ou domínio público, third_party/stb/LICENSE e ORIGIN.txt |
| nlohmann/json 3.11.3 | MIT, identificação no header e licença upstream preservada |
| RmlUi 6.3 | MIT; download de commit e digest fixados, LICENSE.txt upstream instalado |
| raylib | zlib/libpng; biblioteca do sistema, versão efetiva em pkg-config |
| Bullet | zlib; dependência do sistema |
| Lua 5.4 | MIT; dependência do sistema |
| FreeType, OpenGL, Zlib | dependências do sistema; manter avisos aplicáveis ao empacotamento |
| FFmpeg | LGPL/GPL conforme configuração; ligação dinâmica, consulte VIDEO.md |

A libavcodec efetivamente instalada nesta validação declara **GPL version 2 or later**.
Não se deve tratar essa build como LGPL pura. Distribuição de binários ligados
a ela exige revisar as obrigações GPL e das bibliotecas opcionais. O repositório
publica fontes e não inclui esses binários do sistema. A referência primária é
[FFmpeg legal](https://ffmpeg.org/legal.html). HEVC também pode envolver patentes
conforme a jurisdição; não se promete liberdade jurídica universal do codec.

Fontes são carregadas do sistema (DejaVu/Liberation/Noto) sem copiar seus arquivos
para o projeto. Builds e instalações futuras devem registrar versões, origens,
configuração e licenças de todas as bibliotecas que efetivamente redistribuírem.
