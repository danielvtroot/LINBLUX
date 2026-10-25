> Registro histórico anterior à migração LINBLUX. Nomes, capturas, comandos e
> status antigos são preservados como evidência; consulte o [README atual](../../README.md).

# Reprodução determinística de animações R6 — 2026-10-23

Base: `0e5262c`. Escopo: interpolação de keyframes, reprodução no runtime,
edição numérica de pose no Animation Editor. Shift Lock, câmeras, Bullet,
HUD, menu Esc, RmlUi, céu, rosto, imagens/decals, spawn e Lua preservados.

## Causas confirmadas no código

- `AnimationClip::sample` guardava apenas a orientação de cada key
  (quaternion). O caminho entre duas keys vinha do SLERP de arco curto, então
  qualquer trecho autoral maior que 180° era reproduzido no sentido oposto:
  os braços subiam pela frente no editor e voltavam pelas costas no jogo.
  Medido antes da correção: key de 250° reproduzia -110°.
- O sinal de cada quaternion (`q` ou `-q`, mesma orientação) era gravado pelo
  gizmo/Euler sem normalização de hemisfério. Em 180° exatos os dois arcos são
  igualmente curtos e o sentido ficava a cargo do sinal armazenado e do ruído
  numérico — a mesma animação podia inverter ao salvar/reabrir.
- `RuntimeSession::animate` fazia `pose_ = BlendPose(pose_, target, ...)` a
  cada frame. Era um seguidor de primeira ordem com estado: a pose visível
  dependia do pulo anterior e da taxa de quadros, atrasava em relação às keys
  (120° autorais apareciam como 92°) e, ao perseguir um alvo com salto grande,
  podia escolher o arco curto no sentido contrário.
- `airTime_` só era zerado quando o clip ativo mudava. Um pulo enfileirado que
  sai do chão e decola no mesmo passo fixo mantinha o tempo do clip anterior,
  então cada pulo encadeado começava no meio da animação.
- Os campos numéricos do editor usavam `QuaternionToEuler` (ordem ZYX) e
  `QuaternionFromEuler` (ordem XYZ). Não são inversos: redigitar um valor de
  uma junta com rotação combinada alterava a orientação salva.

## Contrato atual

- `AnimationClip::interpolate` normaliza as duas pontas, alinha o destino ao
  hemisfério da origem e resolve 180° por um desempate fixo do eixo relativo.
  O resultado é o mesmo para qualquer sinal armazenado.
- `Keyframe::arcDegrees` (opcional, JSON `arc`) guarda o giro autoral com
  sinal em graus. Só é usado quando termina exatamente na orientação da
  próxima key; caso contrário vale o arco curto. Clips antigos, sem o campo,
  continuam idênticos e com o comportamento anterior.
- O Animation Editor grava esse arco quando o gizmo arrasta uma junta 180° ou
  mais a partir de uma pose com key e a próxima key é inserida. O par Euler
  do editor é ZYX nas duas direções, então exibir/redigitar não muda a pose.
- O runtime reconstrói a pose a partir de uma pose de entrada fixa e do sample
  do clip, com crossfade de 0,12 s. Depois disso a pose é exatamente a dos
  keyframes; não há incremento sobre o frame anterior. O tempo do clip é
  zerado em toda decolagem, inclusive no pulo enfileirado que reinicia no
  mesmo passo físico.

## Validação

`loblox_game_tests` cobre: caminho de 250° com `arc`, queda para arco curto
sem `arc`, 180° determinístico, wrap ±180°, round-trip JSON do `arc`,
round-trip Euler do editor, 30 pulos comparados key a key (pose local das
seis juntas), braços sempre à frente, pose neutra após aterrissagem,
personagem girado no ar, 30/60/120 Hz e save/load antes de repetir o pulo.
Resultado: 4612 checks, 0 falhas. CTest completo: 21/21 aprovados.
