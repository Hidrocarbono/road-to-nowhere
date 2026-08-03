# RTN — Plano MOD × MOTOR

> Documento de referência para decisões de arquitetura.
> Regra de ouro: **quase tudo vive no MOD. O MOTOR só é tocado quando ele não expõe o que precisamos.**

---

## 1. Os dois mundos (e a confusão histórica)

| | 🟦 **MOD** | 🟥 **MOTOR** |
|---|---|---|
| **Repo** | `Hidrocarbono/road-to-nowhere` | `Hidrocarbono/xash3d-fwgs` (fork do FWGS) |
| **O que é** | O JOGO (client.dll + server.dll + assets) | A ENGENHARIA (engine + renderer) |
| **Compila** | CMake (preset `windows-x86-debug`) | waf / CMake próprio do Xash |
| **Nossas edições** | Diárias — todo o gameplay | Raras — só quando necessário |
| **Pasta `engine/` no mod** | ⚠️ SÓ headers de interface (NÃO é o motor!) | — |

**A confusão que tivemos:** o sistema de armas por script foi implementado **no motor** (Fase 1-3, `engine/common/weaponscript.c`). Quando migramos pro PrimeXT, esse parser **não veio junto** (outro motor base) — recriamos no mod (`server/weaponscript.cpp`), mas o formato `.txt` e o conceito **nasceram no motor e foram reaproveitados**.

**Veredito (não foi barriga):** o trabalho no motor foi a **prova de conceito** que definiu o formato dos `.txt` (weaponinfo/ammodesc) que usamos como referência até hoje. O que sobrou no motor é o esqueleto; a lógica real migrou pro `weapon_context` do mod.

---

## 2. Regra de decisão — onde implementar

```
Pergunta: "essa feature é regra do jogo, arma, item, HUD ou efeito de gameplay?"
  → SIM  → 🟦 MOD (client/ ou server/)
  → NÃO  → é renderização, física, rede ou algo que o engine não expõe?
      → SIM → 🟥 MOTOR (com justificativa documentada)
```

| Tipo de mudança | Onde | Exemplos RTN |
|---|---|---|
| Regras do jogo, armas, itens | 🟦 MOD `server/` | MP5, estimulante, painkiller, lean (server), NVG controller |
| HUD, viewmodel, efeitos de tiro | 🟦 MOD `client/` | contadores laterais, eventos 5001, ironsight viewmodel |
| Lógica compartilhada de armas | 🟦 MOD `game_shared/` | CMP5WeaponContext, CBaseWeaponContext |
| Shaders de pós-processamento | 🟦 MOD `game_dir/glsl/` | NVG fósforo verde (arquivo do mod, lido pelo motor) |
| Assets (modelos, sprites, som) | 🟦 MOD `game_dir/` | v_mp5.mdl, painkiller.tga, kb_act.lst |
| Interfaces mod↔motor | 🟦 MOD `engine/*.h` | headers que o mod usa pra falar com o motor |
| Renderização, física, rede, audio core | 🟥 MOTOR | — (não tocamos) |
| Feature que o motor NÃO expõe | 🟥 MOTOR | só com justificativa + plano de contorno |

---

## 3. Status das features (onde vive cada uma)

| Feature | Onde está | Status |
|---|---|---|
| Sistema de armas script (motor) | 🟥 motor `engine/common/weaponscript.c` | ⚠️ Legado — conceito migrou pro mod |
| Parser de scripts no mod | 🟦 mod `server/weaponscript.cpp` | ⚠️ Incompleto — referência de dados apenas |
| Armas hardcoded (MP5 etc.) | 🟦 mod `game_shared/weapons/` | ✅ Ativo (caminho oficial) |
| Ironsight + FOV lerp | 🟦 mod `game_shared/weapons/mp5.cpp` | ✅ Ativo — a GENERALIZAR p/ base |
| Lean (server + client) | 🟦 mod `server/player.cpp` + `client/r_view.cpp` | ✅ Ativo |
| NVG (postfx) | 🟦 mod `server/nvg_controller.cpp` + `game_dir/glsl/` | ✅ Ativo |
| Estimulante / Painkiller | 🟦 mod `server/weapons/` + `game_shared/` | ✅ Ativo |
| HUD lateral contadores | 🟦 mod `client/hud_rtn_items.cpp` | ✅ Ativo |
| Menu de teclas | 🟦 mod `game_dir/gfx/shell/kb_*.lst` | ✅ Ativo |

---

## 4. O que NÃO fazer (anti-retrabalho)

1. ❌ **NÃO reimplementar armas via script no motor** — caminho abandonado (Fase 1-3). O motor não ganha mais parser de armas.
2. ❌ **NÃO mexer no motor pra resolver gameplay** — 95% das features são resolvíveis no mod. Antes de tocar o motor, perguntar: "dá pra contornar pelo mod?" (ex: NVG via `gmsgPostFxSettings` contornou sem tocar o motor).
3. ❌ **NÃO duplicar lógica** — ironsight/lean/efeitos que são genéricos devem viver na **classe base** (`CBaseWeaponContext`) e ser herdados, não copiados por arma.
4. ✅ **FAZER:** arquivo `.txt` = referência de dados que o usuário passa; código C++ = a verdade (hardcode).
5. ✅ **FAZER:** ao surgir feature nova, registrar aqui antes de implementar (MOD ou MOTOR?).

---

## 5. Próximas decisões pendentes

| Feature | Decisão pendente |
|---|---|
| Ironsight genérico | Mover da MP5 pra `CBaseWeaponContext` (base) — TODO no mod |
| Port de armas de CS 1.6 | Modelos + QC com eventos 5001 + classe C++ por arma (hardcode) |
| NVG alternativo (sem granulado) | ScreenFade nativo via mod (server) — sem tocar motor |
| Ícones TGA no HUD | Converter `.tga` → `.spr` (mod `game_dir/`) — HUD clássico só desenha .spr |

---

*Última atualização: sessão RTN F10 (decisões MOD×MOTOR). Mantido por Brother Hermes & Hidrocarboneto.*
