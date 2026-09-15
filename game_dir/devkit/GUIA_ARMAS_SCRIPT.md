# Guia do sistema de armas por script — RTN

Referência pra quem vai criar ou editar uma arma via `scripts/weapons/weapon_*.txt`
e `scripts/weapons/ammodesc.txt`. É um port do formato do Uncle Mike (Paranoia 2),
parseado por `server/weaponscript.cpp` — **server-only** (o cliente não tem o
parser, por isso o servidor é autoritativo sobre modelo/comportamento e alguns
campos precisam viajar pela rede pra predição bater; ver seção de `item_flags`).

Escrito depois de portar todos os 13 `item_flags` do Paranoia2 original — o
resto do documento assume que você já sabe o básico de como um `.txt` é
estruturado (blocos `{ }`, comentários com `//`).

## Os dois arquivos

- **`ammodesc.txt`** — dois tipos de bloco:
  - `ammoinfo { name, MaxCarry, PlayerDamage, MonsterDamage, Damage, Distance,
    NumShots, ShellModel, Missile, count }` — define um **tipo** de munição
    (ex: `"5.56"`). `Damage` substitui `PlayerDamage`/`MonsterDamage` quando os
    dois são iguais (granadas, foguetes). `Missile`/`Distance`/`count` só fazem
    sentido pra granadas (nome da entidade a spawnar).
  - `<classname_da_entidade> { model, sound, type, count }` — uma entidade de
    **pickup** de munição no mapa (ex: `ammo_m16`), apontando pra um `type` que
    precisa bater com algum `name` de um bloco `ammoinfo` acima.
  - **Cuidado com classname que já existe em C++.** `ammo_9mmclip`,
    `ammo_buckshot` e `ammo_rpgclip` já são entidades fixas
    (`LINK_ENTITY_TO_CLASS`, `server/entities/ammo_*.cpp`) — o bloco
    correspondente aqui nunca roda, é decorativo. Só edite o `.cpp` pra mudar
    esses três.
- **`weapon_<nome>.txt`** — um arquivo por arma, quatro blocos:
  `WeaponData`, `PrimaryAttack`, `SecondaryAttack`, `SoundData`, mais um ou
  mais `hudsprite` (ícone da barra de seleção — ver o cabeçalho de qualquer
  `weapon_*.txt` existente pro pipeline de conversão do ícone, não repito aqui).

## `WeaponData` — campo por campo

| Campo | O que faz |
|---|---|
| `viewmodel`/`playermodel`/`worldmodel` | modelo em primeira pessoa / nas mãos de outro jogador / largado no chão |
| `anim_prefix` | **campo morto hoje — não faz nada.** Ver seção dedicada abaixo antes de perder tempo com ele. |
| `bucket`/`bucket_position` | slot/posição na barra de seleção. `MAX_WEAPON_SLOTS` é **5** aqui (P2 usa 10) — `bucket_position` acima de 4 é clampado no parser, mas melhor nem chegar perto do limite |
| `clip_size` | tamanho do pente |
| `defaultammo` | munição dada no primeiro pickup |
| `primary_ammo`/`secondary_ammo` | nome de um `ammoinfo` do `ammodesc.txt`. `"none"` = sem secundária |
| `weight` | peso pra autoseleção (arma "melhor" ganha o troca-automática) |
| `item_flags` | ver a tabela grande abaixo |
| `MaxSpeed`/`MaxSpeedIS` | velocidade do jogador andando / mirando |
| `zoom_fov` | FOV da mira de ferro. `0` = arma sem mira (mesmo sem `IronSight` no `item_flags`, ambos precisam bater) |

`PrimaryAttack`/`SecondaryAttack`: `action`, `nextattack` (cadência),
`PunchAngle`/`PunchAngleIS` (recuo, como **faixa** `"min..max"` por eixo — o
meio da faixa sozinho apaga a variação por tiro, que é o que dá a sensação de
recuo), `SpreadRange`/`SpreadRangeIS` (dispersão em graus, quadril/mira).

`SoundData`: `shootsound1`/`shootsound2` (alterna entre os dois se declarar os
dois), `emptysound`. Caminho relativo a `sound/`. Se o `.wav` não existir no
disco, a arma cai pro som padrão hardcoded — não trava o carregamento do mapa.

## `anim_prefix` — por que não faz nada

Parseado e guardado (`weaponinfo_t::anim_prefix`), mas **nenhum código lê esse
campo**. Toda arma de script que reusa `CMP5WeaponContext` (que é toda arma de
script hoje) chama `DefaultDeploy(..., "mp5")` com a string **hardcoded** em
`mp5.cpp` — não `m_pScriptInfo->anim_prefix`. Escrever qualquer coisa nesse
campo não muda a animação de terceira pessoa que outros jogadores veem.

No Paranoia2 original ele alimentava `ItemInfoArray[].szAnimExt` →
`CBasePlayer::m_szAnimExtention`, escolhendo o sufixo de sequência do
`player.mdl` (`"onehanded"`, `"mp5"`, `"shotgun"`, `"crowbar"`...). Pra portar
de verdade, precisaria trocar o `"mp5"` fixo pelos pontos que chamam
`DefaultDeploy()` em `mp5.cpp` — não foi feito ainda.

## `item_flags` — os 13 valores (port completo do Paranoia2 original)

Combináveis com `|`, ex: `"IronSight|AutoAim|UnderWater"`. Internamente eles
se dividem em **dois grupos com destinos diferentes** — isso importa se você
for mexer no C++, não pro autor de script (a sintaxe do `.txt` é a mesma pros
13):

- **Bits `WIF_*`** (`server/weaponscript.h`) — precisam ser **idênticos nos
  dois lados** (cliente/servidor), porque o cliente prediz a arma sem ter o
  parser do script. Viajam pela rede dentro de `weapon_data_t.iuser2`
  (`m_iScriptFlags`), que é `DT_SIGNED` de 10 bits no `delta.lst` — sobra
  espaço (faixa seguro 0..511), mas é finito.
- **Bits `ITEM_FLAG_*`** (`game_shared/item_info.h`) — só importam pro
  **servidor** (drop, respawn no mundo, duplicata). Não são networked.

**Nunca misturar os dois grupos manualmente no C++** — eles colidem em valor
de propósito (`WIF_IRONSIGHT` = 1 = `ITEM_FLAG_SELECTONEMPTY`), e essa
colisão já causou um bug real aqui (`item_flags` do script vazando pra dentro
de `ITEM_FLAG_*` por engano, arma perdendo `NoAutoSwitch`/`NoAutoReload` sem
aviso nenhum). Pro autor de `.txt`, isso é invisível — só escreva o nome do
flag, o parser (`WS_FlagsFromString`, `weaponscript.cpp`) roteia sozinho.

| Valor no `.txt` | O que faz | Status |
|---|---|---|
| `IronSight` | habilita a mira de ferro (zoom, `SecondaryAttack`) | ✅ funciona |
| `AutoAim` | auto-mira **cosmética** (só o snap do crosshair, `SET_CROSSHAIRANGLE`) | ✅ funciona — **nunca** afeta a direção real do tiro (ver aviso abaixo) |
| `AutoFire` | no Paranoia2, distingue disparo automático de semi-automático | ⚠️ parseado, **sem efeito**. RTN não tem noção de "semi-auto" pra nenhuma arma (nem a Glock) — implementar exigiria detecção de borda de botão nova, não uma leitura de flag. Ver "Gaps conhecidos" |
| `SelectOnEmpty` | pode selecionar a arma sem munição | ⚠️ parseado, sem efeito próprio — **sempre ligado** pra arma de script (comportamento herdado da MP5 clássica), declarar é opcional |
| `NoAutoReload` | desliga o recarregamento automático | ✅ funciona |
| `NoAutoSwitch` | não troca de arma sozinho ao zerar munição | ✅ funciona |
| `LimitInWorld` | limita quantas cópias existem no mapa (respawn) | ✅ funciona |
| `Exhaustible` | jogador pode esgotar a munição e perder a arma | ✅ funciona |
| `NoDuplicate` | não pode receber essa arma de novo (ex: faca) | ✅ funciona (`AddDuplicate()` recusa) |
| `AllowFireMode` | trocar modo de disparo (rajada/semi/auto) | ⚠️ **reservado** — RTN não tem sistema de troca de modo de disparo. Parseado sem erro, sem consumidor |
| `UnderWater` | pode atirar debaixo d'água | ✅ funciona — sem a flag, tiro é bloqueado (`PlayEmptySound`) a `waterlevel == 3` |
| `IronSight`+`Scope` juntos, ou só `Scope` | mira telescópica em vez de mira de ferro | ⚠️ **informativo apenas** — hoje usa exatamente o mesmo código do `IronSight` (reaproveita `zoom_fov`); não existe efeito visual próprio de "luneta" ainda. Pra simular, use um `zoom_fov` bem baixo |
| `NoDrop` | jogador não pode largar a arma | ✅ funciona (`DropPlayerItem()` recusa) |

**Aviso sobre `AutoAim`:** já existiu um bug real aqui (comentário "RTN F10
fix" em `mp5.cpp`) onde `GetAutoaimVector()` era usado pra puxar a direção do
tiro de verdade, e a arma acertava alvos até 25° fora da mira — inclusive
aliados. **Nunca reintroduza autoaim dentro de `PrimaryAttack()`/
`FireBullets()`.** O flag hoje só controla o snap de crosshair em
`WeaponIdle()`, que é puramente visual/feedback, não decide o que a bala
acerta.

## Gaps conhecidos (não implementados nesta rodada)

- **`AutoFire` sem efeito real.** Toda arma no RTN hoje dispara enquanto o
  botão fica pressionado, limitada só pela cadência (`nextattack`) — não
  existe "clique por tiro" em lugar nenhum do código, nem pras pistolas
  clássicas. Fazer a flag funcionar de verdade (bloquear tiro contínuo sem
  ela) exige detectar borda de botão dentro do loop de disparo — uma
  peça de infraestrutura nova, não uma leitura de flag a mais. Ainda não
  fizemos por risco de regressão de cadência de tiro (área que já custou
  várias rodadas de build pra acertar).
- **`AllowFireMode` sem sistema nenhum por trás.** Trocar modo de disparo
  (semi/rajada/auto) exigiria um comando novo de cliente, uma tabela de
  modos por arma no script, e feedback de HUD — um recurso novo do zero,
  não um flag a mais.
- **`anim_prefix` continua morto** — ver seção própria acima.

## Limite de armas de script simultâneas

`WEAPON_SCRIPT_ID_MAX` está fixado em **31** (`server/weaponscript.h`,
`game_shared/weapons/mp5.h`) — só cabe **uma** arma de script carregada por
vez. Pedir uma segunda gera erro no log, não corrompe estado. O teto vem do
`clientdata_t.m_iId`/`weapon_data_t.m_iId` do `game_dir/delta.lst`, que hoje
tem só 5 bits (0..31). Pra ter mais de uma, é preciso primeiro subir esses
dois campos pra 6 bits no `delta.lst` (mudança de protocolo — merece rodada
de teste própria) e só depois aumentar os `MAX` nos dois headers acima.

## Armadilhas de C++ (pra quem for mexer no parser/contexto, não no `.txt`)

- **`CBaseWeaponContext::ItemInfoArray[m_iId]` é a fonte de verdade em
  runtime, não o script.** `CanDeploy()`, `pszAmmo1()`, `pszAmmo2()`,
  `iMaxClip()` leem essa tabela direto. Linha vazia ou desatualizada → tudo
  retorna zero/NULL → a arma reprova no `CanDeploy()` e some do inventário em
  silêncio (`SwitchWeapon()` a descarta sem avisar).
- **`UTIL_PrecacheOtherWeapon()` não é seguro pra classname que pode não
  existir.** Faz cast estilo C pra `CBasePlayerItem*` e grava
  `ItemInfoArray[II.iId]` sem checar limites — entidade que não é arma
  corrompe globais vizinhos. Use `UTIL_PrecacheScriptWeapon()`
  (`dynamic_cast` + bounds check).
- **Existem DUAS fontes de `ItemInfoArray[m_iId].iFlags` pra arma de
  script, uma por lado:** o servidor usa `CWeaponScripted::GetItemInfo()`
  (`server/weapons/weapon_scripted.cpp`, tem acesso direto ao `.txt`); o
  cliente usa `CMP5WeaponContext::GetItemInfo()` (`game_shared/weapons/mp5.cpp`,
  roda também no servidor pra arma clássica) porque `CWeaponScripted` é
  server-only e o cliente prediz com um `CMP5WeaponContext` avulso. As duas
  **têm que concordar** bit a bit — é por isso que qualquer novo `item_flags`
  com efeito em código compartilhado (`game_shared/weapon_context.cpp`) tem
  que ir pelos bits `WIF_*` (rede), nunca só pelo lado servidor.
- **`Read/WriteWeaponsState` chamam `GetWeaponContext(i)` pros 64 ids, todo
  frame.** Criar contexto por faixa de id gera armas fantasma que corrompem
  `ItemInfoArray`/`weapondata`. Só criar pra arma que o servidor anunciou
  (`gWR.rgWeapons[id].iId == id`), com `m_iId` igual à chave.
- **O cliente prediz `Deploy()`** e escreve na clientdata predita
  (`gHUD.m_iViewModelIndex`) — modelo hardcoded no cliente sobrepõe o que o
  servidor mandou.

## Como testar

- `weaponscript_reload` — recarrega todo `.txt` sem reiniciar o mapa.
- `weaponscript_list` — lista as armas carregadas e seus parâmetros.
- `ws_give <nome>` — dá a arma direto (`sv_cheats 1`), com diagnóstico verboso
  no console do servidor (modelo precacheado, activity resolvida, etc.).
- `impulse 101` lê `scripts/weapons/impulse101.txt` (mesma pasta) — editar
  esse arquivo pra incluir/tirar arma do cheat de "dar tudo" não precisa
  recompilar nada.
