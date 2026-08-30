# Relatório de Transição de Agente

**Agente atual:** Claude
**Período de atuação:** 2026-08-19 - 2026-08-23
**Status geral do mod:** Funcionando com ressalvas (ver seção 5 e 6 - disparo em si de armas de script ainda hardcoded na MP5)

---

## 1. Resumo Geral das Mudanças
Ciclo grande, todo em cima do branch `claude/chat-pc-code-k6p2z1` (agora mesclado na `master`). Portei o sistema de **armas por script** do Paranoia 2 (`weaponscript.cpp`, ids dinâmicos 31-62, ironsight/recuo/muzzle por script, com a Parafal como arma de teste completa), corrigi uma série de bugs de build e runtime que isso expôs (corrupção de memória, contextos fantasma, tokenizer quebrado, colisão de macro `MAX_AMMO_TYPES`), ampliei o protocolo de rede (`m_iId` de 5→6 bits no `delta.lst`, verificado contra o fork real do engine) pra caber até 32 armas de script em vez de 1, portei o efeito de sujeira de lente (lensdirt) e vinheta/DOF do estilo Paranoia 2, adicionei 9 entidades que faltavam no `primext.fgd`, resolvi 5 bugs de feedback de teste (rodinha do mouse, mira, bloody HUD, ícone de "usar", aspecto do lensdirt), e por fim implementei um **sistema de fonte bitmap customizada para o `titles.txt`** (`$font`/`$fontsize`), do zero, porque o engine não tem parâmetro de tamanho nem de fonte na função que desenha texto.

---

## 2. Arquivos e Pastas Modificados / Criados
- **Criados:**
  - `client/hud_titlefont.h` / `client/hud_titlefont.cpp` - fonte custom do titles.txt (ver seção 3)
  - `utils/gen_titlefont.py` - gera atlas `.spr` + métricas `.rtnfont` a partir de um `.ttf` (Pillow)
  - `game_dir/sprites/fonts/kirkwood.spr` e `game_dir/fonts/kirkwood.rtnfont` - fonte "kirkwood" já gerada (cplkirkwood.ttf, usada em títulos de "episódio")
  - `server/weaponscript.cpp`/`.h` - parser dos scripts de arma (`game_dir/scripts/weapons/*.txt`, `ammodesc.txt`), server-only
  - `game_shared/weapons/weapon_activity.cpp`/`.h` - mapeamento de ACTIVITY pra animação de viewmodel por script
  - `utils/gen_lensdirt.py`, `utils/gen_hudsprites.py` - geradores de asset (textura de sujeira de lente, sprites de HUD)
  - `game_dir/scripts/weapons/weapon_parafal.txt`, `ammodesc.txt`, `game_dir/models/{v,w,p}_parafal.mdl`, `game_dir/sprites/weapon_parafal.*` - assets de teste da arma de script (FN FAL/Parafal)
  - `CLAUDE.md` - contexto do projeto (fluxo Paranoia2 → PrimeXT, fronteira mod/engine, tetos do protocolo)
- **Modificados (destaque - lista completa no `git log`/diff do merge):**
  - `game_dir/delta.lst` - `clientdata_t.m_iId` e `weapon_data_t.m_iId` de 5 para 6 bits (protocolo de rede - client E server precisam do mesmo arquivo)
  - `game_dir/titles.txt` - reescrito (cp1252) com documentação de `$font`/`$fontsize` + exemplo de bloco `Episodio_01`
  - `client/message.cpp`, `client/hud.h` - `CHudMessage` agora consulta `$font` do bloco e desvia pro renderizador custom quando presente; sem `$font`, comportamento idêntico ao anterior
  - `client/hud_weaponbox.cpp`/`.h`, `client/ammo.cpp` - HUD de arma passou a funcionar para armas de script e independe do traje
  - `server/weapons/weapon_scripted.cpp`/`.h`, `game_shared/weapons/mp5.cpp`/`.h` (armas de script reusam `CMP5WeaponContext`), `game_shared/weapon_context.cpp`/`.h`, `game_shared/weapon_layer.h`, `client/weapon_predicting_context.cpp`, `client/client_weapon_layer_impl.cpp`/`.h`, `server/server_weapon_layer_impl.cpp`/`.h` - fiação do sistema de armas por script
  - `client/render/gl_postprocess.cpp`/`.h`, `client/render/gl_cvars.cpp`/`.h`, `game_dir/glsl/postfx/lensdirt_fp.glsl` - efeito de sujeira de lente (mascarado por glow aproximado, não mais por luminância do pixel)
  - `game_dir/glsl/fog.h`, `game_dir/glsl/forward/skybox_fp.glsl`, `client/render/gl_sky.cpp` - fog não cobre mais o céu
  - `game_dir/primext.fgd` - 9 entidades do mod que faltavam (hand-verificadas contra `KeyValue()`/`Use()` reais)
- **Deletados:** Nenhum.

---

## 3. Novas Funções, Variáveis Globais e Dependências (CRUCIAL)

### Sistema de fonte custom do titles.txt (mais recente)
- `CRTNTitleFont *RTN_GetTitleFont(const char *pszName)` (`client/hud_titlefont.h`) -> carrega (com cache) `game_dir/fonts/<nome>.rtnfont` + `game_dir/sprites/fonts/<nome>.spr`. Retorna `NULL` se não achar/carregar (loga no console, não trava).
- `bool RTN_GetTitleFontOverride(const char *pszMessageName, char *szFontOut, size_t fontOutSize, int *piFontSize)` -> lê `game_dir/titles.txt` (parser próprio e simples, separado do parser do engine) e devolve se o bloco `pszMessageName` tem `$font`. **Não é persistente entre blocos** - cada bloco tem que repetir `$font`/`$fontsize`, igual `$position`.
- `RTN_TitleFont_CharWidth/LineHeight/DrawChar(...)` -> medida e desenho de glifo via `DrawSpriteAsPoly` (já existia em `client/render/tri.cpp`, nunca tinha sido usada pra texto).
- `CHudMessage::m_pCustomFont` / `m_flCustomFontScale` (membros novos em `client/hud.h`) -> setados no início de `MessageDrawScan()` a cada mensagem; `NULL` = comportamento 100% igual ao antigo (fonte fixa do engine via `TextMessageDrawChar`).
- **Como gerar fonte nova:** `pip install Pillow && python3 utils/gen_titlefont.py <fonte.ttf> <nome_curto>`. Documentação completa também está no topo do `game_dir/titles.txt`.
- **IMPORTANTE:** `client_textmessage_t` (engine/cdll_int.h) é struct fixa do engine, sem campo de fonte/tamanho - por isso o parser de `$font`/`$fontsize` é **próprio do RTN**, relendo `titles.txt` separadamente. Se o próximo agente mudar o FORMATO do `titles.txt` (ex: mudar como blocos são delimitados), tem que atualizar `RTN_ParseTitleFontDirectives()` em `client/hud_titlefont.cpp` também, ou os overrides param de bater.

### Sistema de armas por script (ciclo anterior, já estável)
- `server/weaponscript.cpp` - único parser dos scripts (`server-only`, cliente não tem). Ids dinâmicos de arma de script: `WEAPON_SCRIPT_ID_BASE` = 31, `WEAPON_SCRIPT_ID_MAX` = 31 (**uma arma de script de cada vez**, é o teto de 5 bits do protocolo antigo - o `delta.lst` já foi ampliado pra 6 bits neste ciclo, mas os `#define WEAPON_SCRIPT_ID_MAX` em `server/weaponscript.h` e `game_shared/weapons/mp5.h` **ainda não foram subidos** - ver pendência na seção 5).
- `UTIL_PrecacheScriptWeapon()` (substitui `UTIL_PrecacheOtherWeapon()` para classnames que podem não ser arma - faz `dynamic_cast` + bounds check; a versão antiga corrompia memória vizinha).
- `item_flags` de script são `WIF_*` (não confundir com `ITEM_FLAG_*`, mesmo valor numérico, semânticas diferentes).
- `g_TitleFonts[8]` / `g_TitleFontOverrides[256]` (limites fixos, `static` em `hud_titlefont.cpp`) - se o próximo agente precisar de mais de 8 fontes custom carregadas ao mesmo tempo ou mais de 256 overrides no `titles.txt`, tem que subir `MAX_RTN_TITLEFONTS`/`MAX_RTN_TITLEFONT_OVERRIDES`.

### Dependências externas
- `RTN_SubstituteLocalPlayerName()` e `RTN_Utf8ToCp1252()` (`extern`, definidas em outro `.cpp` do client, ciclos anteriores) - `MessageDrawScan()` depende delas rodarem **antes** do desenho (substituição de `%player_name%` e correção de acentuação). Se mexer na ordem dessas chamadas, os acentos/nome do jogador quebram de novo.
- `game_dir/delta.lst` **precisa estar sincronizado entre client e server** - é dado do mod, não do engine, mas se um lado carregar versão diferente do outro, o protocolo desalinha.

---

## 4. Hacks e Contornos da Engine (IMPORTANTÍSSIMO)
- **Fonte custom do titles.txt não usa nenhuma API de fonte do engine.** Ela desenha cada letra como um sprite retangular (atlas `.spr` v32/RGBA) via `DrawSpriteAsPoly`, porque `pfnDrawCharacter` (a única função de texto do engine) não tem parâmetro de tamanho nem de fonte - **não existe** e não vai existir sem mexer no engine. Não tente "aumentar" a fonte padrão do engine chamando `TextMessageDrawChar` várias vezes ou coisa do tipo - isso não escala, redesenha o mesmo glifo bitmap fixo maior (serrilhado). Use `$font` no `titles.txt`.
- **`DrawSpriteAsPoly`** (client/render/tri.cpp) não tem header próprio - é declarada via `extern` local no `.cpp` que a usa (padrão já existente no projeto pra funções "irmãs" tipo `OrthoQuad`). Se vazar pra mais arquivos, vale dar um header próprio.
- **NÃO desenhe textura crua (`.tga` via `LOAD_TEXTURE` + `GL_Bind` + `OrthoQuad`) no HUD.** Neste engine esse caminho gera `GL_INVALID_ENUM` **todo frame**. Isso foi **medido, não deduzido**: com `rtn_hud_selectbar_gldebug 1` (instrumentação em `client/ammo.cpp`, um `GL_CheckForErrors()` por chamada, cada um numa linha diferente), o erro saía na checagem imediatamente após `GL_Bind( 0, <handle do LOAD_TEXTURE> )`, enquanto o `GL_Bind` de uma textura do próprio engine (`FIND_TEXTURE( "*white" )`), **na linha de cima**, vinha limpo. Como o bind falhava, o quad saía pintado com a textura que ficasse ligada antes - era essa a origem do "quadrado branco"/"quadrado com a arte do menu".
  - **O caminho que FUNCIONA é o sprite:** `.spr` v32 truecolor RGBA desenhado por `DrawSpriteAsPoly` (`client/render/tri.cpp`) com `kRenderTransTexture` - exatamente o que o sistema de fonte do `titles.txt` (`client/hud_titlefont.cpp`) já fazia e que é comprovadamente estável aqui. Gere o `.spr` a partir do `.tga` com `python3 tools/tga2spr.py <in.tga> <out.spr>`. **Atenção:** `.spr` v2/indexado (o que a maioria dos conversores gera) aparece como **caixa preta** - tem que ser o v32 do `tga2spr.py`.
  - `hud_radio.cpp` e `hud_textwindow.cpp` ainda usam o caminho de textura crua. Eles aparecem pouco em jogo, então o erro nunca foi notado ali - mas **provavelmente sujam o GL do mesmo jeito**. Se forem mexidos, migrar pra `.spr`.
  - **Nunca chame `FREE_TEXTURE` em textura carregada por nome.** O engine cacheia por nome, então dois donos do mesmo arquivo recebem o **mesmo índice**; se um libera, o handle do outro fica pendurado. (Isto foi corrigido no `CHudWeaponBox`, mas **não** era a causa do `GL_INVALID_ENUM` - a correção foi testada e o erro continuou.)
  - **Diagnóstico:** `R_RenderScene:976` (`gl_rmain.cpp`) é só um `GL_CheckForErrors()`. `glGetError` é *sticky*, então esse número de linha **não aponta a origem** - o erro nasceu antes, tipicamente no HUD 2D do frame anterior. Não persiga essa linha; instrumente o suspeito.
  - **Ícone de arma no script continua apontando para o `.tga`** (`"file" "gfx/vgui/ammo/640_<arma>.tga"`), como nos scripts do Paranoia 2 - **não reescreva o script**. O `SPR_Load` do cliente só abre `.spr`, então `utils/gen_hudsprites.py` resolve o caminho sozinho (`gfx/vgui/ammo/640_X.tga` -> `sprites/rtn_hud_ammo_X.spr`, mesma convenção do `tools/convert_vgui.py`) e **gera o `.spr` se faltar**. Ele também valida o `x/y/width/height` do script contra o tamanho real da imagem: os rects dos scripts do P2 referem-se ao atlas original, quase nunca à arte que está aqui, então quando não cabem ele avisa e usa o quadro inteiro.
  - **`wrect_t` é `{ left, right, top, bottom }`** (`common/wrect.h`), **não** `{left,top,right,bottom}`. Preencha campo a campo, nunca por lista posicional. Inicializar `{0,0,w,h}` dá `right = 0` -> largura zero em texcoord -> o `DrawSpriteAsPoly` desenha um quad degenerado e **nada aparece, sem erro nenhum no console**. Custou uma rodada de teste; o `hud_titlefont.cpp` já fazia certo (campo a campo) e serve de referência.
  - Cvars de apoio em `client/ammo.cpp`: `rtn_hud_selectbar_icons` (0 = barra só com texto, não encosta na TriAPI - isola se o problema é o desenho de ícone) e `rtn_hud_selectbar_gldebug` (1 = checa erro por chamada; tem cota e se autodesliga pra não inundar o console). **Nenhum dos dois pode ser `FCVAR_ARCHIVE`** - o `icons` foi registrado assim por engano e o engine gravou o `0` do teste no `config.cfg`, deixando a barra sem ícone em todas as builds seguintes; o "bug de desenho" que se investigou depois era só o cvar preso. Chave de diagnóstico não persiste.
- **`.rtnfont` é formato texto próprio do RTN** (não é o `.fnt` binário do engine nem nada padrão) - unidades sempre em pixels do tamanho de "bake" (128px), escaladas em runtime por `fontsize_pedido / size`. Documentado no topo de `utils/gen_titlefont.py`.
- **Protocolo de rede:** o teto real de `m_iId` não é o engine (`MAX_WEAPON_BITS` do fork é 6, ou seja 64 armas) - é o `game_dir/delta.lst`, que agora está em 6 bits (64) também, mas o `WEAPON_SCRIPT_ID_MAX` em código ainda está travado em 31 por segurança. Não mexer nesse `#define` sem also confirmar que o `delta.lst` publicado bate.
- Detalhes de hacks do sistema de armas por script (contexto fantasma, ODR do `MAX_AMMO_TYPES`, `ItemInfoArray` como fonte de verdade em runtime) já estão documentados no `CLAUDE.md` da raiz - **leia esse arquivo antes de mexer em armas de script**, ele tem a lista completa de armadilhas já descobertas.

---

## 5. Pendências e O que Precisa Ser Feito na Próxima Semana
- **Fazer:** Disparo em si de armas de script (`PrimaryAttack`, som, animação, muzzle flash) continua **hardcoded da MP5** (`events/mp5.sc`) - armas de script reusam `CMP5WeaponContext` inteiro. Portar isso pro script de verdade é o maior item pendente.
- **Fazer (se precisar de mais de 1 arma de script simultânea):** subir `WEAPON_SCRIPT_ID_MAX` de 31 pra até 62 em `server/weaponscript.h` e `game_shared/weapons/mp5.h` - o `delta.lst` (6 bits) já suporta, só falta o código. Merece rodada de teste própria (mudança de protocolo).
- **Investigar:** O GitHub reportou vulnerabilidades de dependência via Dependabot (não recontei neste ciclo, ver `https://github.com/Hidrocarbono/road-to-nowhere/security/dependabot`) - ainda não tratado por nenhum agente.
- **Nada pendente no sistema de fonte custom do titles.txt** - está completo conforme pedido (tamanho + fonte configuráveis, opt-in, documentado no próprio `titles.txt`).

---

## 6. Estado Atual do Teste (Obrigatório)
- **Testei a compilação e o jogo rodou?** Parcial. Não consegui rodar o build local completo (CMake+vcpkg) neste sandbox - a política de rede do ambiente bloqueia (403) download direto de `github.com`/`codeload.github.com`, de onde o vcpkg busca `fmt`. Validei os arquivos novos/alterados do sistema de fonte (`hud_titlefont.cpp`, `message.cpp`, `hud.h` e os arquivos que dependem dele) com `g++ -fsyntax-only -std=c++17`, usando os mesmos include paths/defines do `client/CMakeLists.txt` - todos limpos, sem erro. **Também disparei a build oficial do GitHub Actions (`nightly-builds.yml`, preset `windows-x86-debug`) na branch antes de mesclar, e o usuário confirmou que baixou e testou com sucesso** antes de pedir a mesclagem final - portanto o build real (MSVC, link completo) passou.
- **Funcionalidades implementadas estão 100%?** Sim, pra tudo que foi testado pelo usuário. As pendências da seção 5 (disparo de arma de script não portado, `WEAPON_SCRIPT_ID_MAX`) são features **não implementadas ainda**, não bugs do que já existe.
- **O último commit pode ser puxado com segurança pelo próximo agente?** Sim - o usuário testou o build (via GitHub Actions) antes de autorizar esta mesclagem pra `master` com tag `v1.0.1`.
