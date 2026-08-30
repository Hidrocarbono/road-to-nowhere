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

## Diferenças estruturais Paranoia 2 → PrimeXT já descobertas

- **`MAX_WEAPON_SLOTS`: P2 = 10, PrimeXT = 5** (e o cliente define
  `MAX_WEAPON_POSITIONS` como `MAX_WEAPON_SLOTS`). Scripts do P2 trazem
  `bucket_position` até 9; acima de 4 aqui é escrita fora dos limites de
  `rgSlots[6][6]` e a arma some do HUD. Há clamp no parser.
- **Ids de arma:** o P2 gera id único por arma (`GenerateID()`/`FindWeaponID()`,
  contador incremental) — não existe classe C++ por arma lá. Aqui cada arma clássica
  tem `WEAPON_*` fixo, e as de script recebem id dinâmico na faixa 31..62.
- **`item_flags` do script são `WIF_*`** (IronSight/AutoAim/AutoFire = 1|2|4), que
  colidem em valor com `ITEM_FLAG_*` (SELECTONEMPTY/NOAUTORELOAD/NOAUTOSWITCHEMPTY
  = 1|2|4) mas **não têm nada a ver**. Nunca atribuir um no outro.

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
   lightmap 0 × qualquer coisa = 0. Por isso há uma `CDynLight` `LIGHT_OMNI`
   presa ao jogador local (`RTN_NVG_SetupPlayerLight`, chamada do `R_AddEntity`).

**Custo do iluminador:** `R_RenderDynLightList` (`gl_world_new.cpp`) faz um passe
aditivo por luz sobre a geometria dentro do volume dela. Por isso o raio default é
curto (300) e `DLF_NOSHADOWS` é **obrigatório** — sem ele um `LIGHT_OMNI` aloca as
6 faces do `depthCubemap` por frame (`gl_dlight.cpp:348`). Não aumente
`rtn_nvg_ir_radius` sem medir.

**Dependência de pipeline:** o passe de exposição só roda com `gl_hdr` **e**
`r_tonemap` ligados (`gl_backend.cpp:477/493`). Com qualquer um desligado o NVG cai
num caminho degradado (só IR + ganho modesto em LDR pelos color levels) e avisa uma
vez no console.

**Cvars:** `rtn_nvg_gain`, `rtn_nvg_ir`, `rtn_nvg_ir_radius`, `rtn_nvg_ir_intensity`,
`rtn_nvg_tint`, `rtn_nvg_debug`.

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

## Ainda não portado

Disparo em si (`PrimaryAttack`, som, animação, muzzle flash) continua hardcoded da
MP5 (`events/mp5.sc`). Armas de script reusam `CMP5WeaponContext`.
