> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Decisões técnicas

Registro cronológico das decisões que moldam o projeto. Alterar uma decisão
importante exige justificativa registrada aqui.

## D001 — Aplicação nativa em C++17 com raylib

**Data:** 2026-10-19

**Decisão:** construir o editor como aplicação nativa C++17 usando raylib para
janela, entrada e renderização 3D.

**Motivo:** o alvo é um notebook com 3 GB de RAM, HD mecânico e Linux/Sway.
Electron, navegador embutido ou engines grandes inviabilizariam o uso. raylib é
leve, de código aberto, funciona com OpenGL e tem build simples.

**Alternativas descartadas:** Godot (engine completa, mais memória e
complexidade), aplicação web/Electron (peso e dependência de navegador),
OpenGL puro (mais código de plataforma para o mesmo resultado).

## D002 — Dear ImGui com docking

**Decisão:** usar Dear ImGui (branch docking) + rlImGui para todos os painéis,
com layout montado por `DockBuilder` na primeira frame.

**Motivo:** permite painéis redimensionáveis, abas e reorganização pelo
usuário sem escrever um sistema de UI próprio. O layout é determinístico
(`io.IniFilename = nullptr`), então a interface abre sempre igual, mesmo sem
arquivos de configuração. O backend rlImGui é pequeno e integra com o loop da
raylib.

**Consequência:** a UI é imediata (sem retenção de estado) e lê o modelo real a
cada frame; não existe cópia da árvore para o Explorer.

## D003 — Bullet Physics 3D

**Decisão:** Bullet 3.24 como motor de física.

**Motivo:** requisito explícito de física 3D; Bullet é maduro, livre, empacotado
no Debian/Ubuntu (`libbullet-dev`) e cobre estático/dinâmico, colisão, raycast e
CCD.

**Alternativas descartadas:** física própria (instável e custosa de manter),
bibliotecas 2D (não atendem).

## D004 — Um corpo por Part e sincronização por flags

**Decisão:** `PhysicsWorld` mantém exatamente um `btRigidBody` por `Part`. A UI
nunca chama Bullet; ela altera a Part, que marca flags (`transform`, `shape`,
`anchored`, `collide`). O passo de simulação consome as flags.

**Motivo:** evita corpos duplicados, vazamentos e reconstruções desnecessárias.
Também mantém a física independente da UI e o renderizador independente da
física.

**Consequência:** existe um pequeno atraso de um frame entre editar e o corpo
refletir a mudança — aceitável e consistente, pois a física roda antes do
desenho.

## D005 — Redimensionar o shape existente em vez de recriar o corpo

**Decisão:** ao mudar `size`, atualizar dimensões do `btBoxShape`/raio da
`btSphereShape`, recalcular massa/inércia e AABB no mesmo corpo.

**Motivo:** o arrasto do gizmo de Scale altera o tamanho a cada frame; recriar
corpo/collider por frame violaria o requisito e causaria instabilidade.
Alternar `Anchored` continua exigindo reconstrução, mas é uma ação discreta.

## D006 — Passo fixo de física independente da renderização

**Decisão:** `stepSimulation(dt, 5, 1/60)`, com `dt` limitado a 0.1 s.

**Motivo:** consistência em máquinas fracas e em quedas de FPS, sem “explosão”
da simulação. Limitar subpassos evita espiral de tempo.

## D007 — Gravidade de −98.1 unidades/s²

**Decisão:** adotar 1 unidade de mundo ≈ 10 cm e usar a gravidade real
(−9.81 m/s² → −98.1 u/s²).

**Motivo:** escala coerente com peças de 4 unidades (~40 cm) e Baseplate de
160 (~16 m), mantendo quedas visualmente plausíveis sem números arbitrários.

## D008 — JSON versionado com escrita atômica

**Decisão:** formato texto `loblox-project` versão 1, com ids estáveis e
propriedades explícitas; validar primeiro, escrever para temporário único no
mesmo diretório e renomear atomicamente em POSIX.

**Motivo:** legibilidade, depuração e compatibilidade futura. A troca atômica
evita substituir um projeto válido por um arquivo parcialmente escrito. Nada de
ponteiros/handles é serializado.

**Alternativas descartadas:** binário proprietário (difícil de inspecionar) e
serialização direta de structs (frágil e insegura).

**Atualização 2026-10-20:** o temporário é criado por `mkstemp`, o conteúdo é
verificado e sincronizado com `fsync`, e o arquivo anterior é preservado nas
falhas anteriores ao rename. O destino conserva suas permissões existentes.

## D009 — Rotação persistida em graus Euler

**Decisão:** o JSON guarda `rotation: [x, y, z]` em graus; internamente a Part
usa quaternion.

**Motivo:** o painel Properties edita graus Euler, e o arquivo fica legível.
Quaternions são convertidos na carga e normalizados; a orientação é preservada
(com possível diferença de representação em rotações equivalentes).

## D010 — Baseplate protegida e sempre ancorada

**Decisão:** a Baseplate é uma Part real no Workspace, `deletable = false`; é
forçada a `anchored = true` e `canCollide = true` na carga e os setters ignoram
tentativas de desancorar ou desligar a colisão. A UI desabilita esses controles
para a Baseplate.

**Motivo:** é o piso físico e visual do projeto; apagá-la, deixá-la cair ou
permitir que as peças a atravessem quebraria a cena inicial. Ainda pode ser
movida/redimensionada manualmente pelo editor.

## D011 — Um único selecionado nesta etapa

**Decisão:** `Scene::selection()` guarda um ponteiro; Explorer e Properties
refletem essa seleção.

**Motivo:** reduz complexidade da fundação. Multi-seleção, undo/redo e
duplicação ficam para a próxima etapa, sem exigir reescrita (basta evoluir a
seleção para um conjunto e os comandos para transações).

## D012 — Materiais como presets de cor/tonalidade

**Decisão:** os seis materiais (SmoothPlastic, Plastic, Wood, Metal, Concrete,
Neon) alteram a cor/tonalidade e o brilho no renderizador, sem shaders PBR.

**Motivo:** a raylib padrão não oferece PBR pronta; escrever shaders de
iluminação fisicamente correta aumentaria custo e risco nesta etapa. O
resultado é visível imediatamente e o enum está preparado para um renderizador
futuro.

**Limitação registrada:** não há rugosidade, reflexo ou textura por material.

## D013 — Studs em malha única cacheada

**Decisão:** gerar uma `Mesh` com todos os studs da Baseplate, reconstruída
apenas quando o tamanho muda, e desenhá-la com `DrawMesh`.

**Motivo:** atende ao requisito de não desenhar cada stud como objeto
independente. O custo é ~25 mil vértices uma vez, não por frame.

## D014 — Interface em inglês, documentação em português

**Decisão:** rótulos das ferramentas seguem a referência (Select, Move, Scale,
Rotate, Part, Sphere, Color, Material, Anchor, Save); documentação e mensagens
de erro internas estão em português.

**Motivo:** familiaridade com o Roblox Studio e clareza para o usuário do
projeto.

## D015 — Sem modo Play/Test nesta etapa

**Decisão:** a física roda continuamente no editor; não existe botão de
execução, personagem ou ciclo de jogo.

**Motivo:** o escopo é a fundação do editor. A simulação contínua já permite
soltar peças e observar queda/colisão, que é o comportamento exigido.

## D016 — Sem backend Wayland nativo

**Decisão:** usar a janela X11 da raylib, funcionando em Sway via XWayland.

**Motivo:** é o caminho suportado pela raylib 6.2 e amplamente disponível em
Loc-OS. Um backend Wayland nativo exigiria outra camada de plataforma e não é
necessário para esta fundação.

## D017 — Repositório com dependências vendorizadas

**Decisão:** versionar ImGui, rlImGui e nlohmann/json em `third_party/`.

**Motivo:** o programa precisa compilar e rodar offline; fixar versões evita
quebras por atualizações externas. O custo é ~4 MB no repositório.

## D018 — Estados de movimento e captura de câmera separados

**Decisão:** movimento livre de câmera exige foco/hover da viewport, não RMB;
RMB captura o cursor somente enquanto a órbita está ativa. A UI libera captura
em soltura, perda de foco, saída/cancelamento e destruição.

**Motivo:** antes movimento WASD e rotação compartilhavam o gate de RMB, e a
rotação usava posição absoluta do cursor sem captura, falhando nas bordas.

## D019 — Transformações de gizmo calculadas de snapshots

**Decisão:** cada arraste fixa manipulador, espaço local/global e transformação
inicial. Uma camada `GizmoMath` pura calcula eixo/plano, face de escala e ângulo;
os incrementos de snap são aplicados a partir dos valores iniciais.

**Motivo:** evita deriva por acumulação e permite testar a matemática sem
janela. Para Sphere, o tamanho permanece uniforme, compatível com o collider
esférico existente.

## D020 — Lua declarativo separado de script/runtime

**Decisão:** `SceneLua` lê/escreve somente um subconjunto literal seguro de
Lua (`loblox-scene` v1), com allowlist e validação via esquema JSON existente.
Não há execução Lua/Luau; JSON continua o formato nativo.

**Motivo:** preparar interoperabilidade textual e futura integração sem
executar código ao abrir projetos, sem runtime novo e sem quebrar arquivos
existentes.

## D021 — Suspender corpo dinâmico enquanto o gizmo edita

**Decisão:** o corpo Bullet associado é temporariamente removido da simulação
durante o arraste, preservado no mapa de corpos e reativado ao terminar. O shape
e o AABB são atualizados in-place.

**Motivo:** gravidade e integração não devem competir com o cursor, e recriar
corpos por frame causaria custo e risco de duplicação.

## D022 — Caminho inicial de projeto independente do diretório de execução

**Decisão:** o projeto inicial usa `$XDG_DATA_HOME/loblox/projects`, com fallback
para `~/.local/share/loblox/projects`; caminhos escolhidos em Save As passam a
ser absolutos na sessão.

**Motivo:** atalhos de desktop podem iniciar o processo com qualquer cwd; um
caminho relativo fazia o salvamento parecer perdido em outra pasta.

## D023 — Alças de escala acessíveis após projeção

**Decisão:** as seis alças derivam dos limites geométricos atuais, mas seus
ícones de tela são separados quando a projeção os sobrepõe. A seleção usa os
mesmos centros visuais e linhas-guia apontam para a âncora geométrica.

**Motivo:** alças em lados opostos frequentemente se sobrepunham na câmera
diagonal mesmo estando em pontos 3D distintos.

**Limitação:** escalar uma caixa rotacionada em eixos globais exatos requer
shear; `Part` guarda somente rotação e dimensões locais. O editor mantém OBB e
aplica o eixo mundial à dimensão local mais alinhada, compensando a projeção.

## D024 — Identidade inequívoca e projeção para escala unilateral

**Decisão:** cada controle tem `HandleId`; a identidade retornada pelo hit test
é preservada no arraste. Um algoritmo snapshot-relative serve aos seis lados.
Em Global, o delta mundial é projetado multiplicando pela componente local
alinhada; em Local, usa-se o eixo local diretamente. O overlay 2D usa retângulos
idênticos no desenho e no hit test.

**Motivo:** uma face deve mudar somente sua dimensão e manter a face oposta
fixa. Dividir o deslocamento pela projeção amplificava escala em Parts
rotacionadas; descartar o sinal do alinhamento também fazia algumas faces
diminuírem quando puxadas para fora. Reconstruir eixo e lado em campos
independentes dificultava confirmar que o controle selecionado era o aplicado.
