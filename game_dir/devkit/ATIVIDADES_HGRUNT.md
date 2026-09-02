# Referência de `ACT_` do `monster_human_grunt` (CHGrunt) — RTN

Lista de toda atividade (`Activity`/`ACT_*`) que o `CHGrunt` (`server/monsters/hgrunt.cpp`)
usa ou depende, com a função de cada uma. Serve de checklist pra quem for
compilar/portar um modelo `.qc` pro hgrunt — cada linha aqui precisa existir
como `$sequence ... ACT_XXX <peso>` no modelo pra aquele comportamento
funcionar.

Lembrete de mecanismo (contexto completo na conversa que gerou este arquivo):
o `ACT_XXX` do `.qc` é resolvido pro **inteiro** que aquele símbolo vale em
`server/activity.h` no momento da compilação, e esse inteiro é gravado por
sequência dentro do `.mdl`. O código nunca pede "sequência tal" — ele pede
`LookupActivity(ACT_RANGE_ATTACK1)`, e o engine procura no modelo qual
sequência tem aquele inteiro batendo. Faltando a tag, a atividade cai pra
`ACTIVITY_NOT_AVAILABLE` e o `SetActivity()` do grunt escolhe outra coisa
como fallback (geralmente fica visualmente "travado" numa pose errada).

## 1. Usadas diretamente no código do `CHGrunt`

| `ACT_` | Onde é usada em `hgrunt.cpp` | Função |
|---|---|---|
| `ACT_IDLE` | `SetActivity`, schedules de espera, `SetYawSpeed` | Parado, sem alvo — animação padrão de patrulha/espera |
| `ACT_IDLE_ANGRY` | `SetActivity` (via `RunTask`/estado agitado) | Idle alternativo quando o grunt está em alerta mas sem linha de tiro — versão "tenso" do idle |
| `ACT_WALK` | `SetActivity`, `SetYawSpeed` | Andar em velocidade normal |
| `ACT_WALK_HURT` | `SetActivity`, sub de `ACT_WALK` via `LookupActivity` | Andar mancando — troca automática quando `pev->health <= HGRUNT_LIMP_HEALTH` |
| `ACT_RUN` | `SetActivity`, `SetYawSpeed` | Correr |
| `ACT_RUN_HURT` | `SetActivity`, sub de `ACT_RUN` via `LookupActivity` | Correr mancando — mesmo gatilho de saúde baixa que o `WALK_HURT` |
| `ACT_CROUCH` | Schedule de cobertura (`slGruntTakeCover`/duck) | Abaixar atrás de cobertura |
| `ACT_TURN_LEFT` / `ACT_TURN_RIGHT` | `SetYawSpeed` | Giro rápido parado (reposicionar sem andar) |
| `ACT_RANGE_ATTACK1` | `SetActivity`, `SetYawSpeed`, `CheckRangeAttack1` | Disparo da arma principal (rajada MP5) |
| `ACT_RANGE_ATTACK2` | `SetActivity`, `SetYawSpeed`, schedule de granada | Ataque secundário — lançar/atirar granada |
| `ACT_MELEE_ATTACK1` / `ACT_MELEE_ATTACK2` | `SetYawSpeed`, `Kick()` | Ataque corpo-a-corpo (chute) quando o inimigo está muito perto |
| `ACT_SPECIAL_ATTACK1` | Task de arremesso de granada de cobertura | Animação específica de preparar/arremessar granada em schedule de granada-cobertura |
| `ACT_RELOAD` | `CheckAmmo`/`m_IdealActivity`, task de recarga | Recarregar a arma — só entra em schedule protegido por `bits_CAP_CROUCH_COVER` |
| `ACT_SIGNAL1` / `ACT_SIGNAL2` | Tasks de sinal pro squad (`TASK_PLAY_SEQUENCE_FACE_ENEMY`) | Gestos de mão pro squad — "avançando", "acho vocês" (primeiro encontro do squad) |
| `ACT_VICTORY_DANCE` | Task de vitória, após matar o jogador | Comemoração quando não há mais ameaça |
| `ACT_GLIDE` | `RunTask` (queda de rapel), `SetYawSpeed` | Planando durante o rapel (`monster_grunt_repel`) antes de tocar o chão |
| `ACT_FLY` | `SetYawSpeed`, task de rapel | Voando/descendo durante o rapel |
| `ACT_LAND` | Task de rapel | Aterrissagem ao final da descida de rapel |

## 2. Herdadas da base (`CBaseMonster`/`CSquadMonster`) — não aparecem em `hgrunt.cpp`, mas o modelo precisa delas

O grunt não referencia essas diretamente porque a lógica mora em
`combat.cpp` (`GetDeathActivity`/seleção de flinch), compartilhada por
qualquer monstro humanoide. Sem a sequência correspondente no `.mdl`, o
grunt cai no fallback genérico (`ACT_DIESIMPLE`/`ACT_SMALL_FLINCH`) mesmo
levando um tiro na cabeça.

| `ACT_` | Gatilho | Função |
|---|---|---|
| `ACT_DIESIMPLE` | Morte sem hitgroup especial reconhecido (fallback) | Animação de morte genérica |
| `ACT_DIEFORWARD` | Morte com o dano vindo de trás | Cai pra frente |
| `ACT_DIEBACKWARD` | Morte com o dano vindo de frente | Cai pra trás |
| `ACT_DIE_HEADSHOT` | Morte com dano fatal no `HITGROUP_HEAD` | Morte específica por tiro na cabeça |
| `ACT_DIE_GUTSHOT` | Morte com dano fatal no `HITGROUP_STOMACH` | Morte específica por tiro no abdômen |
| `ACT_SMALL_FLINCH` | Dano leve, sem hitgroup específico mapeado | Reação curta a dano |
| `ACT_BIG_FLINCH` | Dano pesado | Reação mais forte a dano |
| `ACT_FLINCH_HEAD` | Dano no `HITGROUP_HEAD` (não fatal) | Flinch específico de cabeça |
| `ACT_FLINCH_STOMACH` | Dano no `HITGROUP_STOMACH` | Flinch específico de abdômen |
| `ACT_FLINCH_LEFTARM` / `ACT_FLINCH_RIGHTARM` | Dano no braço correspondente | Flinch de braço |
| `ACT_FLINCH_LEFTLEG` / `ACT_FLINCH_RIGHTLEG` | Dano na perna correspondente | Flinch de perna |

## 3. Existem no Paranoia 2, ainda **não existem** no `activity.h` do RTN

Achado da nossa comparação `activity.h` RTN × P2: estas ativações são
específicas do pacote de animações do P2 pros humanos militares
(`monster_human_military`/`monster_human_alpha`) e não têm entrada em
`server/activity.h` do RTN hoje. Ficam registradas aqui porque são
pré-requisito se decidirmos portar o modelo/animações completas do P2 (ver
discussão sobre o pacote "hgrunt tático" + lanterna):

| `ACT_` (só existe no P2) | Função pretendida |
|---|---|
| `ACT_FLASHLIGHT` | Ligar/desligar a lanterna anexada à arma |
| `ACT_WALKBACK_FIRE` | Andar de costas atirando (recuo tático) |
| `ACT_FIRINGWALK` | Andar atirando pra frente (gait-blend com `ACT_RANGE_ATTACK1`) |
| `ACT_FIRINGRUN` | Correr atirando |
| `ACT_DIERAGDOLL` | Morte em ragdoll completo (em vez de sequência de morte fixa) |
| `ACT_180_LEFT` / `ACT_180_RIGHT` | Giro rápido de 180° (reação a flanco) |
| `ACT_90_LEFT` / `ACT_90_RIGHT` | Giro rápido de 90° |

Pra usar qualquer uma dessas, é preciso **acrescentar a entrada correspondente
no final de `server/activity.h`** (mesmo padrão de "append, nunca inserir no
meio" que já seguimos pro `bits_COND_CROUCH_NOT_SAFE`) antes de compilar
qualquer `.mdl` que dependa delas — senão a tag na sequência não corresponde
a nada que o RTN reconheça.
