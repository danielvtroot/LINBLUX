# Imagens e associações no LINBLUX

A aba Recursos mostra miniaturas e nomes amigáveis. Renomear altera apenas
`assets/image-names.json` (version 1, objeto names: arquivo → nome), mantendo
identificador e caminho. Novas importações recebem o nome do arquivo original;
recursos antigos sem metadado aparecem como Imagem + sufixo do identificador e
podem ser renomeados. PNG/JPEG continuam sendo cópias gerenciadas; o original
externo não é apagado. Metadados acompanham snapshots e exportação da experiência.

O seletor nas Propriedades pesquisa nome/ID e apresenta miniaturas. Adesivos e
texturas independentes são filhos selecionáveis da peça. Um membro do rig com a
antiga propriedade image aparece no Explorador como Imagem do membro: selecione
a referência ou o membro para trocar ou remover sua imagem, sem criar outro decal.
Adesivos do rig continuam sendo filhos próprios, com face e transparência.

Excluir imagem abre a lista de usos. Localizar seleciona o objeto/membro/adesivo;
Remover associação limpa somente aquele vínculo, mantendo arquivo, geometria,
rig e animações. Remover adesivo/componente é uma operação distinta da exclusão
do asset. Os campos de apresentação legados (incluindo faceImage) também são
listados e podem ser desvinculados. Exclusão é bloqueada enquanto houver usos.

Após resolver os vínculos, Excluir imagem salva a cena antes de apagar o arquivo
gerenciado. Se salvar falhar, o arquivo permanece. O gerenciador verifica tanto
o manifesto quanto a cena persistida, além das referências em memória: chamadas
nativas não podem ignorar a cena salva apenas passando usedByScene=false. Revisões
antigas podem depender de assets excluídos permanentemente; mantenha backup se
precisar recuperar essas revisões.

A consulta central DescribeImageUsage identifica objeto, rig, membro, decal,
propriedade e referência. ImageInUse e ReplaceImageReferences usam essa mesma
consulta; SetImageUsage recusa descritores obsoletos. Somente propriedades de
imagem conhecidas são interpretadas: textos livres não são reescritos. Caminhos
externos com o mesmo nome de arquivo não são confundidos com assets internos.
Strings dinâmicas dentro de scripts não permitem inferência completa de uso;
esta análise não promete resolver dependências arbitrárias de código Lua.

Personagens novos não recebem face ou olhos impostos pelo renderizador. Imagens
configuradas explicitamente em experiências antigas continuam sendo renderizadas
até o criador remover o vínculo. O recurso de fábrica já sem imagem permanece
sem alteração de rig, juntas, animações, física ou controlador.

Superfícies independentes são persistidas em objetos Decal/Texture do formato
v3, com campo surface. Imagem, face, transparência, rotação, cor RGBA, repetição
e deslocamentos UV usam o mesmo codec dos adesivos de rig. Arquivos v1/v2/v3
anteriores continuam legíveis; campos novos opcionais usam os valores anteriores.
Pais Part são restaurados antes das superfícies, sem depender da ordem do JSON.
No renderizador, os componentes usam a mesma orientação de faces e rotação UV
contínua, com composição na ordem dos filhos.

## Orientação e limites da Alpha

Decals usam a orientação corrigida das faces; esta missão não inverteu UVs.
Os testes utilizam padrões assimétricos próprios, sem publicar imagens pessoais.
PNG/JPEG e associações persistidas são cobertos pelas regressões de imagem.
Texturas com repetição conservam seu caminho separado. Um modo proporcional Fit
mais completo é melhoria futura e não faz parte da promessa desta Alpha.
