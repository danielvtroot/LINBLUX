# Contribuir com LINBLUX

Use C++17 e mudanças pequenas e coerentes. Preserve o isolamento entre autoria
e execução; uma sessão de teste não pode regravar o mundo do Studio.

1. Configure e compile conforme o README, usando um build separado.
2. Execute os testes relacionados e depois a suíte completa para mudanças centrais.
3. Em alterações de interface, examine capturas reais em pelo menos 1280×720 e
   1920×1080 e teste os controles, não somente a compilação.
4. Adicione textos ao catálogo pt-BR e cores ao tema central. Evite marcas e
   conteúdo sem licença. Preserve os avisos de copyright dos componentes.
5. Revise arquivos novos e o diff, registre um commit identificável e envie por
   push normal. Nunca force o histórico nem descarte alterações locais.

`scripts/checkpoint.py` oferece build, testes, revisão de arquivos explicitamente
indicados, commit e push seguro. Uma falha de rede mantém o commit local.
O salvamento de padrões da engine modifica assets/defaults; esses recursos devem
entrar no mesmo fluxo de revisão e sincronização que o código.

A UI não deve criar regras de física ou outra implementação da serialização.
Use ProjectIO para cenas, ExperienceManager para bibliotecas e RuntimeSession
para execução. Testes novos devem verificar contratos observáveis e riscos reais.
