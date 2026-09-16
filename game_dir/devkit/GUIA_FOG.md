# Guia do fog — RTN

Referência dos cvars de fog do RTN: o que cada um faz, como combinar pra
disfarçar o fim do mapa/início do skybox, e o que é fixo (só recompilando o
mapa) versus o que dá pra ajustar ao vivo pelo console.

## Onde o fog vem de verdade

A cor e a densidade **base** do fog são a chave `fog` do `worldspawn` do
mapa (`R G B D`, definida no Hammer) — só muda **recompilando o mapa**. Tudo
que os cvars abaixo fazem é multiplicar/dar forma a esse valor base em tempo
real, sem precisar recompilar nada. Fórmula completa em
`game_dir/glsl/fog.h`; o fog é exponencial (`exp2(-density*dist)`), não
linear (não existe "start/end" fixo de OpenGL clássico aqui).

## Cvars originais (intensidade/cor)

| Cvar | Default | O que faz |
|---|---|---|
| `gl_fog_density_scale` | `2.5` | Multiplica a densidade `D` do worldspawn inteira. `1.0` = PrimeXT puro (quase sem fog); `10.0` = sufocante. Referência: com `D=10` do mapa, `2.5` dá 50% de fog a ~800u (20m). |
| `gl_fog_sky_blend` | `0.85` | Teto de quanto o fog cobre o skybox (0 = céu sempre limpo, 1 = céu some por completo). Escala junto com a densidade — fog fraco toca pouco no céu mesmo com este valor alto. |
| `gl_fog_debug` | `0` | `1` = ignora o worldspawn inteiro e usa `gl_fog_debug_color`/`gl_fog_debug_density` direto. Pra testar cor/densidade sem recompilar o mapa. Não é `FCVAR_ARCHIVE` (não sobrevive a restart) — é ferramenta de diagnóstico, não preferência. |
| `gl_fog_debug_color` | `"255 0 255"` | Cor de teste (R G B, 0-255) usada só quando `gl_fog_debug 1`. |
| `gl_fog_debug_density` | `0.01` | Densidade de teste, mesma unidade de `D` do worldspawn já multiplicado — usada só quando `gl_fog_debug 1`. |

## Cvars novos (forma da curva) — pra disfarçar borda de mapa/skybox

Os dois cvars acima (`density_scale`/`sky_blend`) só escalam a **mesma**
curva inteira, perto e longe juntos — não dá pra pedir "limpo por 500
unidades, só daí fecha" nem "mais denso rente ao chão". Estes cinco
resolvem isso:

| Cvar | Default | O que faz |
|---|---|---|
| `gl_fog_start` | `0` | Distância (unidades) **sem nenhum fog** antes de começar a curva exponencial de sempre. `0` = comportamento idêntico a antes (fog começa no olho da câmera). |
| `gl_fog_height_density` | `0` | Densidade **extra** de fog, que só existe perto do chão e cai com a altura. `0` = desligado — precisa ligar por mapa/área. |
| `gl_fog_height_start` | `0` | Altura Z do mundo (unidades do Hammer) onde o fog de altura está no **máximo** — normalmente o piso da área que quer disfarçar. Confira o Z real no editor antes de setar. |
| `gl_fog_height_falloff` | `128` | Quantas unidades de altura **acima** de `gl_fog_height_start` levam a densidade de altura a cair pra ~37% (1/e). Menor = camada de nevoa mais rasteira/fina; maior = camada mais alta/grossa. |
| `gl_fog_sky_horizon` | `0.6` | 0..1 — quanto o **horizonte** do céu recebe mais fog que o **zênite**. `0` = mistura igual em qualquer direção (comportamento antigo); `1` = só o horizonte pega fog, zênite fica sempre limpo. Já vem ligado por padrão porque o ganho visual é imediato. |

Todos são `FCVAR_ARCHIVE` (sobrevivem a restart, salvos no config).

## Como usar pra disfarçar borda de mapa e horizonte

1. **Ache o Z do chão** da área problemática no Hammer (a régua/grade mostra
   as coordenadas).
2. Comece só com o fog de altura:
   ```
   gl_fog_height_start <Z do chão>
   gl_fog_height_falloff 150
   gl_fog_height_density 0.01
   ```
   Suba `gl_fog_height_density` aos poucos (`0.005` a `0.02` costuma ser a
   faixa útil) até o horizonte se dissolver sem afogar o resto da cena —
   é o parâmetro mais sensível, porque some direto na densidade final.
3. Se quiser manter visibilidade boa perto do jogador e só fechar de verdade
   perto da borda:
   ```
   gl_fog_start 400
   ```
   Ajuste pra distância real que você quer ver limpo antes do fog começar a
   fechar.
4. `gl_fog_sky_horizon` já vem em `0.6` — se ainda notar uma linha entre o
   mundo e o céu, suba pra `0.8`~`1.0`; se o céu estiver ficando cinza demais
   perto do horizonte, desça.
5. `gl_fog_debug 1` continua funcionando pra testar cor/densidade **base**
   ao vivo — os cinco cvars novos se aplicam por cima, não interferem no
   debug.

## O que ainda não existe (avaliado, não implementado)

Fog por área/trigger (tipo `env_fog` do Paranoia2 original — liga/desliga
fog diferente por volume do mapa, com fade). Hoje o fog é **um valor só pro
mapa inteiro** (worldspawn) + os multiplicadores acima, também globais. Pra
ter fog mais forte só numa área específica sem fogar o mapa todo, seria
preciso portar esse conceito adaptado pro sistema de shader do RTN (o
`env_fog` do P2 usa `glFog` de função fixa, legado - não dá pra copiar
literal). Fica pra depois dos ajustes de forma acima estarem calibrados.
