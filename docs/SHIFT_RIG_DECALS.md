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

Primeira etapa: 312 checks headless de gameplay e 382 checks RML, zero falhas;
capturas em `/tmp/loblox-rml-ui-I29VxP/`. Pendentes decals por face do R6.
