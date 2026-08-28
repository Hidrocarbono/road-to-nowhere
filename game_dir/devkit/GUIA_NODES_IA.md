# Guia de node graph para IA — RTN

Nota de desenvolvimento sobre como o PrimeXT (herdado do GoldSrc/HL1) navega
os inimigos pelo mapa, e por que **não é preciso encher o mapa de `info_node`**
pra IA funcionar bem. Escrita pra quem está mapeando pro Road to Nowhere.

## O node graph é o fallback, não o método principal de movimento

Todo deslocamento de monstro passa por `CBaseMonster::BuildRoute()`
(`server/monsters.cpp`), que tenta, nessa ordem:

1. **`CheckLocalMove()`** — traça uma linha reta (com o hull do tamanho do
   monstro) do ponto A ao B. Se não bate em nada, o monstro anda direto.
   **Não usa node nenhum.**
2. **`FTriangulate()`** — se o caminho reto esbarra num obstáculo isolado
   (uma quina, uma caixa, um degrau), calcula um único ponto de desvio e
   contorna. **Ainda sem node.**
3. **Só se as duas falharem**, o monstro recorre ao node graph
   (`WorldGraph.FindPath`, os `info_node`/`info_node_air` do mapa) pra
   calcular uma rota mais complexa.

Na prática: **em qualquer sala aberta, corredor simples ou área com poucos
obstáculos isolados, o inimigo nunca consulta um node.** Ele navega por
raycast direto, igual qualquer pathing simples.

## Onde os nodes realmente importam

Node graph só é necessário nos **gargalos de conectividade** — pontos onde
não existe linha reta nem contorno de obstáculo único que resolva:

- Vãos de porta entre duas áreas fechadas
- Topo/base de escadas
- Passagens estreitas ou em L que exigem mais de uma mudança de direção
- Qualquer lugar onde a única rota entre dois pontos não é "óbvia" por
  raycast (ex.: contornar uma parede inteira, não só uma caixa)

Fora desses pontos, não é necessário — e nem recomendado, por custo de
processamento do build do grafo — espalhar `info_node` pela sala toda.

## Você não precisa ligar os nodes manualmente

O processo de build (`CTestHull::BuildNodeGraph()`, `server/nodes.cpp`,
disparado automaticamente pelo primeiro `info_node` que spawna no mapa) faz
isso sozinho:

1. Liga automaticamente todo par de nodes que se enxergam entre si
   (`LinkVisibleNodes`).
2. Elimina links redundantes (quando um terceiro node no meio já cobre a
   ligação).
3. Caminha entre cada node e seus links usando o hull de cada tamanho de
   monstro, pra confirmar que o bicho realmente cabe passando por ali.
4. Gera `maps/graphs/<nome_do_mapa>.nod`, que é o que o servidor carrega
   em runtime — e um relatório `.nrp` com o resultado.

O trabalho do mapper é só **onde** colocar o marcador. A topologia de
conexão (quem liga com quem) é calculada pelo build, não é decisão manual.

## Resumo prático

- **Não** cubra salas abertas com grade de nodes — desperdiça nodes e tempo
  de build sem ganhar nada (o monstro já navega essas áreas sem eles).
- **Sim**, coloque `info_node`/`info_node_air` nos gargalos: portas,
  escadas, curvas fechadas, qualquer transição entre áreas que não seja
  visível em linha reta de um lado pro outro.
- Rode o build do grafo (spawnar um `info_node` e deixar o mapa carregar
  uma vez) depois de qualquer mudança de geometria que afete esses
  gargalos, pra regenerar o `.nod`.
- Se um inimigo está "travando" ou recusando se mover em uma área
  específica, o primeiro suspeito é falta de node num gargalo daquela
  região — não falta de cobertura geral.

## Não existe alternativa de navmesh nesta stack

Vale registrar: o engine (Xash3D-FWGS, fork em
`Hidrocarbono/xash3d-fwgs`) não tem suporte a navmesh (Recast/Detour ou
equivalente) — o `CGraph`/`.nod` acima é o único sistema de pathing
disponível. Substituir isso exigiria formato de dado novo, trabalho de
engine (fora da fronteira mod/engine descrita no `CLAUDE.md` da raiz) e
nenhum suporte de editor pra desenhar navmesh no pipeline atual — está fora
de proporção pra qualquer ciclo de melhoria de IA que não seja, em si, um
projeto de reescrita do sistema de navegação. Recomendação: trabalhar
dentro do node graph existente, seguindo as regras acima.
