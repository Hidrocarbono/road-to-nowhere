# Corpo do jogador em primeira pessoa (pernas + tronco)

Ao olhar para baixo o jogador vê o próprio corpo (botas, pernas, cintura) **junto com os
braços e a arma**. Implementação: `client/rtn_body.cpp` (estado) e
`CStudioModelRenderer::DrawBodyModel()` em `client/render/gl_studio_draw.cpp` (desenho).

## Como funciona

- O corpo é uma **entidade própria** (`models/player_legs.mdl`, o modelo de pernas do
  Paranoia 2: partes `Player_Top` + `Player_Bottom`), desenhada **antes** da arma. O viewent
  é sempre a arma — antes, ao olhar para baixo ele era *trocado* pelo corpo e a arma sumia.
- Desenhada só na passada normal do jogador local, vivo e em primeira pessoa. Nunca em
  espelho, portal, sombra ou cubemap.
- FOV do mundo e profundidade normal (sem o depth hack da arma): as pernas têm a mesma
  perspectiva do chão e são ocultadas por paredes/caixas. A arma, com o depth hack, fica por cima.
- Esta entidade **não projeta sombra** (a sombra do jogador continua sendo a do `player.mdl`)
  e **não recebe luz dinâmica** (lanterna / NVG) — a mesma situação da arma.
- Esconde-se com janela de documento aberta, morto, noclip, escada, na água (≥ cintura),
  spectator e câmera de terceira pessoa.

## Comportamento

| Aspecto | Regra |
|---|---|
| Posição | pés no chão (origem do modelo está nos pés), suavizados em degraus |
| Recuo | `rtn_body_back` (olhando em frente) + `rtn_body_back_look` (olhando 100% para baixo): sem isso a câmera fica dentro do peito e o plano próximo recorta o tronco |
| Yaw | segue o olhar. Parado: deadzone de 50° antes de girar. Andando: segue rápido, com o quadril girando até 35° para o lado do movimento (strafe) |
| Para trás | o corpo continua olhando em frente e a animação roda **ao contrário** |
| Animação | escolhida pelo **estado** (idle / walk2handed / run2 / crawl / crouch_idle / jump2), pelo label da sequência |
| Velocidade da animação | presa à velocidade real, para os pés não "deslizarem" (ver cvars de referência) |
| Pulo | só após 0,15 s no ar (degrau não dispara o pulo) |
| Lean | o corpo acompanha `rtn_body_lean` (40%) do deslocamento e do roll da câmera |
| Troca de animação | blend automático do renderer (sem "pop") |

## Cvars (todas `FCVAR_ARCHIVE`, ajustáveis no console)

| Cvar | Padrão | O que faz |
|---|---|---|
| `rtn_body` | 1 | liga/desliga o corpo |
| `rtn_body_back` | 8 | recuo (unidades) olhando em frente |
| `rtn_body_back_look` | 8 | recuo extra olhando para baixo |
| `rtn_body_zoffset` | 0 | ajuste vertical dos pés (+ sobe) |
| `rtn_body_walk_ref` | 150 | velocidade (u/s) em que o ciclo de caminhada roda a 1.0× |
| `rtn_body_run_ref` | 320 | idem, ciclo de corrida |
| `rtn_body_crouch_ref` | 100 | idem, ciclo agachado andando |
| `rtn_body_lean` | 0.4 | quanto do lean o corpo acompanha; **negativo inverte o roll** |

O `player_legs.mdl` **não tem `linearmovement`** (velocidade de chão) gravado nas sequências,
por isso as três referências são cvars: se os pés deslizam, ajuste-as (maior = ciclo mais
lento para a mesma velocidade).

## Calibrar em jogo (primeira vez)

1. Olhe reto para baixo parado: as botas devem aparecer à frente e o tronco embaixo, sem
   "buraco" (interior da malha). Se o tronco é cortado, aumente `rtn_body_back_look`.
2. Ande reto: os pés não devem deslizar. Ajuste `rtn_body_walk_ref` / `rtn_body_run_ref`.
3. Incline (Q/E) olhando para baixo: o corpo deve acompanhar sem se separar da câmera.
   Se o roll do corpo vai para o lado errado, use `rtn_body_lean` negativo.
4. Suba uma escada de degraus: sem "pop" vertical dos pés.

## Limites conhecidos

- `player_legs.mdl` tem só 8 sequências: **não há** animação de queda, escada, nado nem giro
  parado (o corpo gira inteiro); por isso o corpo é escondido em escada e na água.
- Sem luz dinâmica: em local escuro com lanterna/NVG, as pernas não são iluminadas por elas.
- Não há sincronia entre a pose de sprint da arma e o ciclo de corrida das pernas além de a
  velocidade escolher `run2`.
- A cintura/tronco aparecem como o `Player_Top` do modelo permitir; se parecer pouco ou
  estranho, é o modelo, não o código.
