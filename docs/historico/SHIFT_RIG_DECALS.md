> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Shift Lock e decals de rig — continuidade

Shift Lock: alvo instantâneo vindo da cabeça animada + acompanhamento antes
da física + correção de yaw do Follow durante lock causavam os trancos.
Mouse agora é lido antes da física; acompanhamento ocorre depois, com altura
neutra do rig e offset lateral em espaço da câmera. Mouse não é suavizado;
oclusão e captura existentes foram preservadas. EditorCamera não foi alterada.

`lock.png` original integrado pelo cache RmlUi, com corte de margens alfa,
proporção preservada e caixa de 28 px no centro da viewport. A imagem tem RGB
azul nos pixels transparentes; o alfa real remove esse fundo corretamente.
Também removido o marcador antigo do fallback legacy.

Checkpoint Shift Lock: `111249a`. Primeira etapa: 312 checks de gameplay e
382 checks RML, zero falhas. Capturas reais do binário principal, incluindo
pulo, pausa, fullscreen e Stop: `build/shift-rig-fixes/` (não versionadas).

R6: o renderer antigo repetia a imagem do membro nas seis faces. A imagem
legada agora usa apenas Front. Cada Body possui filhos RigDecal independentes,
com ID local persistente qualificado por `rig://<rig-id>/<decal-id>`, nome,
imagem, face e transparência. São assets do rig, sem criar Parts/corpos físicos
duplicados. A representação JSON acrescenta campos opcionais aos rigs atuais;
projetos sem decals continuam válidos. O rosto padrão não foi alterado.

Uso: Explorer > selecionar R6 > Torso/braço/perna > Properties > Add Decal.
Selecionar o filho e editar Name, Face, Image Asset e Transparency. Import
image usa o importador/cache existente. Repetir Add Decal para outras imagens.
F2 renomeia o filho; Delete ou o botão remove somente esse filho. Front=-Z,
Back=+Z, Left=-X, Right=+X, Top=+Y, Bottom=-Y, sempre no espaço local do membro.
Camadas da mesma face compõem na ordem dos filhos, sem escrever profundidade;
a geometria mantém teste de profundidade e oclusão. O último filho fica acima.

A integração gráfica encontrou mais duas causas: selectvalue interno não era
reconhecido como campo focado, e o Properties reconstruía o botão entre down/up
após um commit. Agora teclado pertence ao form ancestral e a árvore visual
permanece durante o clique. Add/Import ficam acessíveis no topo do inspector.

Validações focadas: gameplay 318 checks; R6 imagem 74 checks (seis faces,
animação, camadas, transparência, IDs, delete e round-trip); RML decals 154
checks com mouse/teclado reais, importação, F2/Delete e save/load. Zero falhas
nas execuções finais desses testes. A regressão RML completa passou com 1050
checks. Os outros 18 testes CTest também passaram; os 20 testes foram cobertos
em execuções por grupos. `git diff --check` e build principal aprovados.

Comandos: `cmake --build build --target loblox -j2`,
`ctest --test-dir build --output-on-failure`, ou testes focados
`ctest --test-dir build -R 'loblox_(game|rig_image|rig_decals_ui)$' --output-on-failure`.
Pendente: notebook real Sway/Vega 8, DPI e avaliação tátil de suavidade. As
capturas e automação desta etapa usam Xvfb/llvmpipe. Próximo comando recomendado:
`git log -2 --oneline` para localizar os dois checkpoints funcionais.
