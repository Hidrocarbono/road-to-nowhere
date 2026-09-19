# Road to Nowhere — Contexto do Projeto

Este é um mod baseado no **PrimeXT** (engine Xash3D). Estamos portando/implementando
diversas funcionalidades que foram originalmente desenvolvidas para o **Paranoia 2**,
outro mod para Xash3D.

## Fluxo de trabalho para novas features

Antes de planejar qualquer modificação que pareça algo que o Paranoia 2 já resolveu:

1. **Checar primeiro o repositório original do Paranoia 2**:
   https://github.com/a1batross/Paranoia2_original
   — ver se algo semelhante já foi implementado lá e como o código foi feito.
2. **Só depois** analisar o port dessa solução para o PrimeXT (adaptação) ou, se não
   existir equivalente, seguir para desenvolvimento próprio do zero.

Ou seja: Paranoia2_original é referência/fonte de padrões antes de reinventar a roda
no PrimeXT.

## Colaboração multi-agente

Este repositório é trabalhado em conjunto por múltiplos agentes/colaboradores
(ex.: "Hermes" além do Claude). Branch protection está deliberadamente desativada.
Ver `NOTICE.md` e `CHANGELOG_AGENT.md` no repo para histórico e convenções combinadas.

---

# Arquitetura: onde termina o mod e começa o engine

O jogo roda como **DLLs do mod (PrimeXT) sobre o engine Xash3D-FWGS**. Saber de que
lado da fronteira um problema está economiza horas — vários sintomas parecem iguais
dos dois lados.

- **Mod (este repo):** `server/` (server.dll), `client/` (client.dll),
  `game_shared/` (compilado nos dois). `engine/` aqui contém **apenas headers de
  interface**, não o engine.
- **Engine:** fork do usuário em https://github.com/Hidrocarbono/xash3d-fwgs
  (é um repo separado; precisa ser clonado à parte para consulta).
- **`game_dir/`** é o conteúdo do jogo — inclusive `delta.lst`, que é **dado do mod**
  e define o protocolo de rede (ver abaixo).

## Tetos do protocolo (`game_dir/delta.lst`) — CRÍTICO

`delta.lst` define quantos bits cada campo usa na rede. O engine só tem fallback
embutido para `movevars_t`; `clientdata_t` e `weapon_data_t` vêm **inteiramente
deste arquivo**. Ou seja: dá para ampliar o protocolo editando dado do mod, sem
recompilar o Xash — mas cliente e servidor precisam carregar o mesmo arquivo.

Limites atuais e o que eles significam:

| Campo | Bits | Alcance | Observação |
|---|---|---|---|
| `clientdata_t.m_iId` | **5** | 0..31 | id da arma ativa |
| `weapon_data_t.m_iId` | **5** | 0..31 | id por slot |
| `clientdata_t.viewmodel` | 10 | 0..1023 | índice de modelo |
| `clientdata_t.weapons` | 32 | 32 armas | **já contornado** pelo PrimeXT via `gmsgWeapons` (8 bytes = 64 bits) |

**O engine é mais largo que o `delta.lst`:** em `engine/common/protocol.h` do fork,
`MAX_WEAPON_BITS 6` → 64 armas previsíveis (e `MSG_WriteWeaponData` escreve o índice
do slot com 6 bits), e `MAX_MODEL_BITS 12` → 4096 modelos. Quem estrangula é o
`delta.lst` do mod, não o engine.

**Consequência:** o sistema de armas por script atribui ids dinâmicos a partir de 31
(`WEAPON_SCRIPT_ID_BASE`), e 31 é o **último** que cabe em 5 bits. Por isso
`WEAPON_SCRIPT_ID_MAX` está fixado em **31** — ou seja, **uma arma de script por vez**;
pedir uma segunda gera erro no log em vez de corromper estado em silêncio. Para ter
mais: **primeiro** subir os dois `m_iId` para 6 bits no `delta.lst` (mudança de
protocolo, merece rodada de teste própria), **depois** aumentar o `MAX` nos dois
headers que o definem (`server/weaponscript.h` e `game_shared/weapons/mp5.h`).

# Sistema de armas por script — estado e armadilhas

Port do formato do Uncle Mike (Paranoia 2): `game_dir/scripts/weapons/ammodesc.txt`
e `weapon_*.txt`, parseados por `server/weaponscript.cpp` (**server-only** — o cliente
não tem parser, o que obriga o servidor a ser autoritativo sobre modelos).

Referência completa (todo campo do `.txt`, os 13 `item_flags` — quais funcionam de
verdade e quais são gap conhecido — e por que `anim_prefix` não faz nada):
`game_dir/devkit/GUIA_ARMAS_SCRIPT.md`.

## Diferenças estruturais Paranoia 2 → PrimeXT já descobertas

- **`MAX_WEAPON_SLOTS`: P2 = 10, PrimeXT = 5** (e o cliente define
  `MAX_WEAPON_POSITIONS` como `MAX_WEAPON_SLOTS`). Scripts do P2 trazem
  `bucket_position` até 9; acima de 4 aqui é escrita fora dos limites de
  `rgSlots[6][6]` e a arma some do HUD. Há clamp no parser.
- **Ids de arma:** o P2 gera id único por arma (`GenerateID()`/`FindWeaponID()`,
  contador incremental) — não existe classe C++ por arma lá. Aqui cada arma clássica
  tem `WEAPON_*` fixo, e as de script recebem id dinâmico na faixa 31..62.
- **`item_flags` do script viram DOIS grupos de bits, não um.** `WIF_*`
  (`weaponscript.h`, precisam de predição idêntica nos dois lados — viajam em
  `weapon_data_t.iuser2`) e `ITEM_FLAG_*` (`item_info.h`, só servidor —
  drop/respawn/duplicata). Os dois **colidem em valor de propósito**
  (`WIF_IRONSIGHT` = 1 = `ITEM_FLAG_SELECTONEMPTY`) — nunca atribuir um campo
  no outro. Os 13 valores do Paranoia2 original (`IronSight` até `NoDrop`) já
  estão todos portados — tabela completa com o que cada um faz de verdade em
  `game_dir/devkit/GUIA_ARMAS_SCRIPT.md`.

## Armadilhas do PrimeXT que já custaram caro

- **`CBaseWeaponContext::ItemInfoArray[m_iId]` é a fonte de verdade em runtime**, não
  o script. `CanDeploy()`, `pszAmmo1()`, `pszAmmo2()`, `iMaxClip()` leem essa tabela
  direto. Linha vazia ou com dado velho → todos retornam zero/NULL → a arma reprova
  no `CanDeploy()` e o `SwitchWeapon()` a descarta **em silêncio** (pega, nunca equipa).
- **`UTIL_PrecacheOtherWeapon()` é inseguro para classname que pode não existir:**
  faz cast estilo C para `CBasePlayerItem*`, chama virtual e grava
  `ItemInfoArray[II.iId]` sem checar limites. Com entidade que não é arma, corrompe
  globais vizinhos. Usar `UTIL_PrecacheScriptWeapon()` (dynamic_cast + bounds check).
- **`weaponscript.h` não pode usar nomes de macro genéricos.** `MAX_AMMO_TYPES` já
  existe em `game_shared/cdll_dll.h` como 32; o `#ifndef/#define 64` resolvia para um
  valor ou outro conforme a ordem de include (ODR). Usar prefixo `WS_`.
- **`Read/WriteWeaponsState` chamam `GetWeaponContext(i)` para os 64 ids, todo frame.**
  Criar contexto por faixa de id gera dezenas de armas fantasma que corrompem
  `ItemInfoArray` e `weapondata`. Só criar para arma que o servidor anunciou
  (`gWR.rgWeapons[id].iId == id`), e com `m_iId` igual à chave.
- **O cliente prediz `Deploy()`** e escreve na clientdata predita, que
  `HUD_TxferLocalOverrides()` copia para `gHUD.m_iViewModelIndex` — o índice que o
  renderer desenha. Modelo hardcoded no cliente sobrepõe o do servidor.

---

# Visão noturna (NVG) — arquitetura e armadilhas

Item `item_nvgoggles` (server) + efeito 100% client-side. Liga/desliga pelo comando
de cliente `nvg` (bind `n` no `rtn.cfg`) → `pfnServerCmd("nvg_toggle")` →
`CBasePlayer::NVGToggle()`.

**Divisão de responsabilidade:** o servidor é dono só do estado (tem o item /
ligado / bateria 0..100, tudo em `CBasePlayer` com `DEFINE_FIELD`, drenagem no
`UpdateClientData()` junto com a lanterna) e publica por `gmsgNVG` (2 bytes, só
quando muda). O render decide sozinho como aquilo aparece — `client/render/gl_nvg.cpp`.

## Por que NÃO é postfx (erro que já custou um ciclo)

A primeira tentativa fazia tudo em `postfx/postprocessing`, que roda **por último**
(`gl_backend.cpp`: `RenderTonemap` → … → `RenderPostprocessing`). Ali a cena já é
LDR e a exposição já foi limitada em 1.0 pelo `generate_exposure` — a informação do
escuro **já foi descartada**. Somado a isso, `u_Brightness` naquele shader é
**aditivo**: em pixel preto ele só levanta o piso, uniforme, e o tint verde por cima
vira mancha chapada sem contraste. Não existe valor de `brightness`/`levels` que
conserte isso. Se alguém pedir "mais verde" ou "mais brilho" no NVG, a resposta
quase sempre é mexer no ganho de exposição, não no postfx.

## As duas partes que fazem o efeito

1. **Ganho de exposição (o que faz enxergar).** `postfx/generate_exposure_fp.glsl`
   teve `exposureMax`/`exposureScale`/`adaptRate*` promovidos de `const` para o
   uniform `u_NVGParams` (vec4). Sem NVG o cliente manda `(1.0, 1.0, 0.6, 1.6)` =
   exatamente os valores antigos, comportamento idêntico ao original. Com NVG, o
   teto sobe (`rtn_nvg_gain`, default 12) → amplificação **em HDR, antes** do
   tonemap comprimir. Custo: nenhum passe novo, só uniforms.
2. **Iluminador IR (o que resolve o preto absoluto).** Ganho é multiplicativo:
   lightmap 0 × qualquer coisa = 0. Por isso há uma `CDynLight` `LIGHT_SPOT`
   presa ao jogador local, apontada para frente (`RTN_NVG_SetupPlayerLight`,
   chamada do `R_AddEntity`), no mesmo molde de origem/ângulo de
   `R_SetupPlayerFlashlight`.

**Por que spot e não omni:** uma omni de raio R tem caixa de cull cúbica de lado
2R (volume ∝ R³) — a luz é espalhada em todas as direções, a maioria das quais a
câmera nem olha. Uma spot de raio R e FOV F tem a caixa limitada pelo cone: a
seção a distância R tem raio ≈ R·tan(F/2). Com F=50° isso dá ≈0.47R, o que faz a
caixa de uma spot de raio 550 ficar **menor** que a caixa da omni de raio 300 que
ela substituiu — mais alcance pelo mesmo orçamento de `R_RenderDynLightList`, e
de quebra fica igual a um iluminador IR de verdade (que em NVGs reais é
projetado, não omnidirecional). Reusa `tr.flashlightTexture` como cookie —
nenhum asset novo.

**Combo com a lanterna nativa.** O cone da lanterna (`EF_DIMLIGHT`) já é
amplificado pelo ganho de exposição como qualquer luz realtime da cena — é por
isso que "NVG + lanterna" já resolvia o escuro distante mesmo antes de qualquer
código dedicado. `RTN_NVG_SetupPlayerLight` detecta `EF_DIMLIGHT` no jogador
local e reforça `radius`/`intensity` do próprio iluminador do NVG
(`rtn_nvg_ir_flashlight_boost`, default ×1.4) — não é luz nova, é reescala de
uma luz que já existe, custo extra zero.

**Custo do iluminador:** `R_RenderDynLightList` (`gl_world_new.cpp`) faz um passe
aditivo por luz sobre a geometria dentro do volume dela. `DLF_NOSHADOWS` é
**obrigatório** — sem ele uma `LIGHT_SPOT` aloca `depthTexture` por frame
(`gl_dlight.cpp:348`). Meça FPS antes de subir `rtn_nvg_ir_radius`/`rtn_nvg_ir_fov`
além do default — o cálculo acima só vale para a razão raio/FOV atual.

**Dependência de pipeline:** o passe de exposição só roda com `gl_hdr` **e**
`r_tonemap` ligados (`gl_backend.cpp:477/493`). Com qualquer um desligado o NVG cai
num caminho degradado (só IR + ganho modesto em LDR pelos color levels) e avisa uma
vez no console.

**Cvars:** `rtn_nvg_gain`, `rtn_nvg_ir`, `rtn_nvg_ir_radius`, `rtn_nvg_ir_fov`,
`rtn_nvg_ir_intensity`, `rtn_nvg_ir_flashlight_boost`, `rtn_nvg_tint`, `rtn_nvg_debug`.

## Sobre a técnica de lightstyle (avaliada e não usada)

`tr.lightstyle[]` é calculado inteiramente no cliente (`R_AnimateLight`,
`gl_dlight.cpp:187`, chamada em `gl_backend.cpp:422`) e chega aos shaders como
uniform por frame — dá para sobrescrever localmente de graça, sem rede. Não foi o
caminho escolhido porque: (a) é multiplicativo, não cria luz onde o lightmap é 0;
(b) só afeta iluminação baked, não luzes realtime; (c) `gl_slight.cpp` usa `uint`
com `>>7` e clamp em 255, então **modelos estouram para branco antes do mundo**; e
(d) o light cache de studio atualiza a cada 0,1s, dando *pop* ao ligar/desligar.
Se um dia for usado, seria como complemento, com clamp em 550 (`gl_local.h:589`) —
e nunca pelo servidor (`pfnLightStyle` é broadcast e atropela o flicker do mapa).

---

## Disparo das armas de script — o que já é próprio de cada arma e o que é compartilhado

Armas de script reusam `CMP5WeaponContext` (`game_shared/weapons/mp5.h/.cpp`) como
implementação C++, mas isso **não** significa que elas soam/atiram como a MP5. Já
está portado, por arma, via `weapon_*.txt`:

- **Som de tiro** (`SoundData/shootsound1|2`, `emptysound`) — toca no **servidor**
  via `EMIT_SOUND_DYN` em `PrecacheScriptSounds()`/`PrimaryAttack()`, e o evento
  client-side (`events/mp5.sc`) recebe `iparam1 = m_iScriptHasSound` avisando pra
  **não** tocar o som hardcoded da MP5 por cima. O cliente não tem o parser do
  script (não conhece o nome do `.wav`), por isso o som fica um frame atrás da
  predição — assumido de propósito (ver comentário em `mp5.cpp` perto de
  `PlaybackWeaponEvent`); cadência/dispersão continuam 100% preditos.
- **Animação** — cada arma tem seu próprio `viewmodel`/`playermodel`/`worldmodel`
  no `.txt`. `SendWeaponAnimAct(WACT_*, MP5_ANIM_*)` manda um **índice** de
  sequência (0=idle, 1-3=tiro, 4=reload...); quem toca é o modelo carregado da
  arma, não a MP5 — funciona porque o artista segue a mesma convenção de ordem
  de sequência da MP5 ao modelar a viewmodel nova.
- **Recuo** (`PunchAngle`/`PunchAngleIS` do script, sorteado por tiro) e
  **dano/tipo de munição** (`primary_ammo` → `WeaponScript_FindAmmo`) — também
  por arma, lidos do `.txt`.

O que **é** genuinamente compartilhado entre todas as armas de script — e isso é
opção de design herdada da HL/`events/mp5.sc`, não pendência: o **evento de fire**
em si (`m_usEvent1`/`m_usEvent2`), ou seja, o sprite/luz de muzzle flash e a
ejeção de estojo são do mesmo evento pra todas. Só mexer nisso se um dia quiser
efeito visual de disparo diferente por arma — não é bug, é convenção.

---

# Munição de script (`ammo_*`) — registro dinâmico de entidades

Assim como armas de script, munição de script (`ammo_aks`, `ammo_ak74`, `ammo_m16`,
`ammo_painkillers`, etc., definidas em `game_dir/scripts/weapons/ammodesc.txt`) só
existe de verdade a partir de `CEntityFactoryDictionary`/`IEntityFactory` —
`CAmmoScripted` (`server/entities/ammo_scripted.h`/`.cpp`) resolve o pickup certo via
`WeaponScript_FindAmmoPickup(STRING(pev->classname))` no `Spawn()`, e
`AmmoScript_RegisterEntities()` (`server/weaponscript.cpp`, chamada logo depois de
`WeaponScript_RegisterEntities()` em `WeaponScript_Init()`) registra a factory
compartilhada pra cada classname de `gAmmoPickups[]` que ainda não tem entidade C++
própria.

**Armadilha já paga:** ter o `classname` certo no `.txt`/`.fgd` **não basta** — sem
essa factory registrada, `CreateEntityByName()` falha em silêncio (loga "unknown
entity type" e nada spawna) pro classname que não tem `LINK_ENTITY_TO_CLASS` fixo.
Isso já mordeu uma vez: o primeiro fix (sincronizar nome de modelo entre `.fgd` e
`ammodesc.txt`) foi necessário mas **insuficiente** — a munição só passou a aparecer
no mapa depois de criar essa factory. Mesma lição do bug do atlas de fonte: uma
correção real pode mascarar uma causa-raiz mais funda; sempre confirmar que o
sintoma sumiu de fato, não só que o primeiro problema identificado foi corrigido.

---

# Nomes de modelo unificados (itens de consumo)

`item_painkiller`, `item_stimulant` e `item_antidote` vinham de um ciclo antigo que
usava nomes de asset emprestados/placeholder do Half-Life (`w_antidote.mdl` pros
três). Já corrigido: `item_painkiller` → `models/w_painkiller.mdl`, `item_stimulant`
→ `models/w_stimulant.mdl` (mantendo **`v_antidote.mdl`** como viewmodel do
stimulant — verificado contra a QC decompilada desse modelo especificamente, a
sequência de índice 3 = "draw"; não trocar esse viewmodel sem reconferir). Também
sincronizados em `game_dir/primext.fgd`. `item_antidote` continua em
`models/w_antidote.mdl` (esse é o nome real dele, não placeholder).

Mesmo padrão vale pra munição de script: `ammo_aks`/`aksbox`, `ammo_ak74`/`ak74box`,
`ammo_m16`/`m16box` e `ammo_painkillers` tiveram os `.mdl` sincronizados entre
`ammodesc.txt` e `primext.fgd`. **Cuidado ao editar `ammodesc.txt` por script:** ele
usa CRLF; reescrever em modo texto (Python `open(...).read()/write()`) corrompe os
finais de linha e polui o diff — editar em modo binário ou preservando `\r\n`.

---

# Loot dos hgrunt

Tabela de drop configurada em `server/monsters/hgrunt.cpp`
(`HGrunt_AmmoClassnameForKiller()` mapeia tipo de munição → classname de pickup).
Estendida pra cobrir todos os tipos de munição de arma de script, não só as armas
clássicas: `"5.56"→ammo_m16`, `"7.62"→ammo_ak74`, `"5.45"→ammo_aks`,
`"9x39"→ammo_vss`, `"aps"→ammo_aps`, `"mp5"→ammo_mp5`, `"rpk"→ammo_rpk`,
`"tt33"→ammo_tt33`. Antes disso, hgrunt morto com arma de script sempre dropava
munição genérica de 9mm por falta de mapeamento. Referência completa de loot em
`game_dir/devkit/ATIVIDADES_HGRUNT.md`.

---

# Fog volumétrico — start distance, height fog e horizonte de céu

Sistema estendido de um único parâmetro de densidade (`u_FogParams.w`) pra um
modelo com forma: distância inicial, decaimento por altura e blend do céu
ponderado por horizonte. Novo uniform `u_FogParams2` (`vec4`), registrado em
`client/render/gl_shader.h`/`.cpp` como `UT_FOGPARAMS2`, com **significado
diferente por shader** (mesmo padrão já usado em `u_FogParams.w` — densidade nos
shaders de mundo, peso de blend no shybox):

- Mundo/studio/grass/decal (`game_dir/glsl/fog.h`, `CalculateFog(...)`): `.x` =
  `fogStart` (distância antes da qual não há névoa), `.y` = densidade extra por
  altura, `.z` = altura onde a névoa de altura começa, `.w` = falloff.
- Skybox (`game_dir/glsl/forward/skybox_fp.glsl`): só `.x`, força do blend
  ponderado por horizonte (`1 - |skyDir.z|`) — céu no horizonte pega mais fog que
  o zênite.

5 cvars novas em `client/render/gl_cvars.h`/`.cpp` (`FCVAR_ARCHIVE`):
`gl_fog_start`, `gl_fog_height_density`, `gl_fog_height_start`,
`gl_fog_height_falloff`, `gl_fog_sky_horizon`. Todos com default que reproduz o
comportamento antigo (mudança 100% aditiva — cvars antigos de fog continuam
funcionando sem alteração). Referência completa de cvars e uso em
`game_dir/devkit/GUIA_FOG.md`.

---

# Sistemas nativos já confirmados (não reinventar)

Antes de propor um sistema novo, checar esta lista — várias ideias que parecem
gaps já são cobertas pelo engine ou por código existente:

- **Fadiga/stamina:** já existe, `server/player.cpp:~1779-1798`. `pev->fuser2`
  guarda stamina 0-100, drena `-0.25`/tick correndo (`IN_RUN` + velocidade > 100),
  regenera `+0.25` parado / `+0.1` andando; com stamina < 1 não corre (`maxspeed`
  cai pra 320) **e não pula**. Nasce cheia em 100 no spawn (`player.cpp:~3125`).
  Estilo Paranoia2 (`dlls/player.cpp:2139-2158` de lá foi a referência).
- **Autosave:** `server/entities/trigger_autosave.cpp` é só o wrapper de entidade
  (`SaveTouch()` chama `SERVER_COMMAND("autosave\n")`) — o comando `autosave` em
  si é **nativo do engine** (`engine/server/sv_save.c`/`sv_cmds.c`/`sv_main.c`,
  serialização completa de save state). Não precisa reimplementar nada aqui, só
  colocar o trigger padrão no mapa.
- **Diário/notas com imagem:** sistema já existe, exibe `.tga` na tela (não usar
  o caminho de textura crua pro HUD normal — ver armadilha de `GL_INVALID_ENUM`
  na seção de fonte custom do `CHANGELOG_AGENT.md` — mas o diário já tem seu
  próprio caminho funcional, não confundir os dois).
- **Geiger/radiação:** `CBasePlayer::UpdateGeigerCounter()` +
  `CTriggerHurt::RadiationThink()` (`server/entities/trigger_hurt.cpp`) já fazem
  detecção por proximidade ao trigger de radiação mais próximo, 100% funcional,
  nativo do HL1. `trigger_hurt` com `DMG_POISON`/`DMG_RADIATION` já aplica dano
  persistente a cada 0.5s (`HurtTouch()`) — serve como "parede" de área sem
  precisar de física/geometria.
- **Ciclo de dia/noite real: NÃO existe e não dá pra fingir bem.** Engine só tem
  `sv_skyname`/`svc_skybox` (troca de textura do skybox em runtime), sem ângulo
  de sol dinâmico — `light_environment`/lightmaps são **bakeados em tempo de
  compilação do mapa**, não recalculam em runtime. Um "ciclo" fake (trocar
  skybox + tint do `u_FogParams`/`u_FogParams2` num timer) muda cor ambiente e
  névoa, mas a sombra permanece fixa — não prometer "dia/noite" de verdade, só
  "variação de atmosfera por horário".
- **Antídoto:** `item_antidote`/`server/player.cpp:~2406` já cura automaticamente
  exposição a `Poison`/`NerveGas` (zera o contador de dano por tempo) alguns
  segundos após pegar o item. Não cura radiação.
