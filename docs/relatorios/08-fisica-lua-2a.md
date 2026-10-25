# Fase 2 — incremento 2A: física real por Lua

ATLAS localizou PhysicsWorld e RuntimeSession. Foram acrescentadas operações
validadas do runtime para velocidade, força, impulso, gravidade e raycast de
peças; nenhum ponteiro Bullet é entregue ao Lua. O namespace linblux.physics
oferece esses raios e busca por proximidade de centros. Todas as operações são
aplicadas fora do passo de Bullet, na cópia de execução.

Arquivos: Runtime.{h,cpp}, LuaRuntime.cpp, tests/studio_main.cpp e o guia Lua.
Nenhum formato salvo foi alterado. A consulta de raios ignora a cápsula do
jogador; os bindings legados permanecem intactos.

TURBO FAST: 12/12 selecionados aprovados, zero falhas/timeouts. Compilação em
95,6 s, testes em 10,3 s, total 106,3 s. Run: 20261025T070746Z-27267, orçamento
120 s de testes, timeout individual 60 s. O teste novo verifica movimento real,
velocidade após impulso, gravidade, ponto/normal/distância do raio, peça ignorada,
proximidade e rejeição de vetores inválidos e corpos ancorados. Testes de runtime,
spawn, persistência, padrões e compatibilidade também passaram nessa seleção.

Não é a conclusão de toda a Fase 2: contatos, atualização física por evento,
controle do personagem/câmera, áudio e UI seguem pendentes na API nova. Não se
rodou FULL nem se repetiram capturas da Home, que não foi alterada.
