#!/usr/bin/env python3
"""
gen_hudsprites.py - gera game_dir/sprites/<arma>.txt a partir dos blocos
                    "hudsprite" dos scripts em game_dir/scripts/weapons/

POR QUE ISTO EXISTE
-------------------
Os scripts do Paranoia 2 declaram os sprites de HUD da arma dentro do proprio
weapon_<nome>.txt:

    "hudsprite" { "name" "weapon"  "file" "sprites/weapon_parafal.spr"
                  "x" "0" "y" "64" "width" "256" "height" "64" }

O parser do RTN (server/weaponscript.cpp) le isso e guarda em gWeaponInfo. Mas
quem desenha o HUD de selecao de arma e o CLIENTE, e o cliente do PrimeXT/HL usa
outra convencao: WeaponsResource::LoadWeaponSprites (client/ammo.cpp:66) procura

    sprites/<classname da arma>.txt

e, se o arquivo nao existir, SPR_GetList devolve NULL e a funcao retorna sem
carregar nada - silenciosamente. Era esse o motivo de o weapon_parafal.spr estar
na pasta certa, declarado no script, e mesmo assim nao aparecer: ninguem nunca
ligou os dois formatos.

Levar os dados do script ate o cliente exigiria um parser no cliente (nao existe)
ou mais um canal de rede. Como sao dados ESTATICOS - nao mudam em runtime -, o
caminho certo e gerar o arquivo na convencao que o cliente ja sabe ler. O script
continua sendo a fonte da verdade; este utilitario so traduz.

USO
---
    python3 utils/gen_hudsprites.py

Le todos os game_dir/scripts/weapons/weapon_*.txt e escreve um
game_dir/sprites/<arma>.txt para cada um que tenha bloco hudsprite. Rode de novo
sempre que mexer nos hudsprite de um script.

FORMATO DE SAIDA (convencao do Half-Life)
-----------------------------------------
    <numero de linhas>
    <nome> <resolucao> <arquivo spr> <x> <y> <largura> <altura>

Nomes que o cliente procura: weapon (icone apagado), weapon_s (icone aceso),
ammo, ammo2, crosshair, autoaim, zoom, zoom_autoaim.

.tga NO SCRIPT E O PADRAO - NAO REESCREVA O SCRIPT
--------------------------------------------------
Os scripts do Paranoia 2 apontam o hudsprite para o .tga do VGUI, e isso
continua valendo aqui: mantenha

    "file"  "gfx/vgui/ammo/640_<arma>.tga"

O SPR_Load do cliente so abre .spr, entao esta ferramenta resolve sozinha,
pela mesma convencao do tools/convert_vgui.py:

    gfx/vgui/ammo/640_X.tga  ->  sprites/rtn_hud_ammo_X.spr
    gfx/vgui/640_X.tga       ->  sprites/rtn_hud_X.spr

e GERA o .spr a partir do .tga se ele ainda nao existir (usa tools/tga2spr.py,
precisa de Pillow). Ou seja: o script do P2 entra como esta, sem edicao.

O rect (x/y/width/height) do script foi escrito para o atlas ORIGINAL do P2,
que quase nunca e a arte que esta aqui. Se o rect nao couber na imagem real,
a ferramenta avisa e usa o quadro inteiro em vez de emitir um recorte
invalido - foi o que aconteceu com a Parafal (rect 0 72 24 24 num .tga de
200x62).
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, "game_dir", "scripts", "weapons")
SPRITES = os.path.join(ROOT, "game_dir", "sprites")
GAME_DIR = os.path.join(ROOT, "game_dir")

# tga2spr.py mora em tools/ - reusa o MESMO conversor que o convert_vgui.py
sys.path.insert(0, os.path.join(ROOT, "tools"))
try:
    from tga2spr import tga_to_spr
except ImportError:
    tga_to_spr = None

# O cliente escolhe a lista por resolucao: <640 usa 320, senao 640
# (client/ammo.cpp, variavel iRes). Emitimos as duas com os mesmos valores -
# nao ha arte separada para 320, e sem a linha o HUD fica vazio nessa faixa.
RESOLUTIONS = (320, 640)

TOKEN = re.compile(r'"([^"]*)"|(\S+)')


def tokenize(text):
    """Mesma ideia do tokenizer do parser: remove comentarios e devolve tokens,
    tratando trecho entre aspas como um token unico."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    out = []
    for m in TOKEN.finditer(text):
        out.append(m.group(1) if m.group(1) is not None else m.group(2))
    return out


def parse_hudsprites(path):
    """Devolve a lista de blocos hudsprite de um script de arma."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        toks = tokenize(f.read())

    sprites = []
    i = 0
    while i < len(toks):
        if toks[i].lower() != "hudsprite":
            i += 1
            continue
        # espera '{' logo em seguida
        i += 1
        while i < len(toks) and toks[i] != "{":
            i += 1
        if i >= len(toks):
            break
        i += 1
        entry = {}
        while i < len(toks) and toks[i] != "}":
            if i + 1 < len(toks):
                entry[toks[i].lower()] = toks[i + 1]
                i += 2
            else:
                i += 1
        sprites.append(entry)
        i += 1
    return sprites


def resolve_to_spr(path):
    """gfx/vgui/ammo/640_X.tga -> sprites/rtn_hud_ammo_X.spr
       gfx/vgui/640_X.tga      -> sprites/rtn_hud_X.spr
    Mesma convencao do tools/convert_vgui.py. Gera o .spr se faltar.
    Devolve None se o caminho nao casa com nenhuma regra."""
    norm = path.replace("\\", "/").lower()
    base = os.path.basename(norm)
    if not norm.endswith(".tga") or not base.startswith("640_"):
        return None

    stem = base[len("640_"):-len(".tga")]
    if "/vgui/ammo/" in norm:
        out_rel = f"sprites/rtn_hud_ammo_{stem}.spr"
    elif "/vgui/" in norm:
        out_rel = f"sprites/rtn_hud_{stem}.spr"
    else:
        return None

    out_abs = os.path.join(GAME_DIR, out_rel.replace("/", os.sep))
    if not os.path.exists(out_abs):
        src_abs = os.path.join(GAME_DIR, path.replace("\\", "/").replace("/", os.sep))
        if not os.path.exists(src_abs):
            print(f"    AVISO: {path} nao existe - nao da pra gerar {out_rel}")
            return None
        if tga_to_spr is None:
            print(f"    AVISO: falta Pillow/tga2spr - gere na mao: "
                  f"python3 tools/tga2spr.py {path} {out_rel}")
            return None
        os.makedirs(os.path.dirname(out_abs), exist_ok=True)
        tga_to_spr(src_abs, out_abs)
        print(f"    gerado {out_rel} (a partir de {path})")

    return out_rel


def fit_rect(spr_rel, x, y, w, h, name):
    """Valida o rect contra o tamanho real do .spr. Fora dos limites (ou zerado)
    -> quadro inteiro. Os rects dos scripts do P2 referem-se ao atlas original."""
    spr_abs = os.path.join(GAME_DIR, spr_rel.replace("/", os.sep))
    try:
        import struct
        with open(spr_abs, "rb") as f:
            d = f.read(56)
        sw, sh = struct.unpack("<ii", d[48:56])
    except Exception:
        return x, y, w, h   # sem como conferir: mantem o que o script disse

    try:
        xi, yi, wi, hi = int(x), int(y), int(w), int(h)
    except ValueError:
        return "0", "0", str(sw), str(sh)

    if wi <= 0 or hi <= 0 or xi < 0 or yi < 0 or xi + wi > sw or yi + hi > sh:
        print(f"    rect de '{name}' ({xi} {yi} {wi} {hi}) nao cabe em {sw}x{sh} "
              f"- usando o quadro inteiro")
        return "0", "0", str(sw), str(sh)

    return x, y, w, h


def main():
    if not os.path.isdir(SCRIPTS):
        print(f"nao encontrei {SCRIPTS}", file=sys.stderr)
        return 1

    os.makedirs(SPRITES, exist_ok=True)
    total = 0

    for fname in sorted(os.listdir(SCRIPTS)):
        if not fname.startswith("weapon_") or not fname.endswith(".txt"):
            continue

        weapon = fname[:-4]
        entries = parse_hudsprites(os.path.join(SCRIPTS, fname))
        if not entries:
            continue

        lines = []
        skipped = []
        for e in entries:
            name = e.get("name", "")
            spr = e.get("file", "")
            if not name or not spr:
                continue

            # Os scripts do Paranoia 2 apontam o icone para o .tga do VGUI (o
            # HUD dele lia os dois formatos). Aqui SPR_Load so abre .spr - mas
            # em vez de exigir que o script seja reescrito, resolvemos o .tga
            # para o .spr equivalente pela MESMA convencao do
            # tools/convert_vgui.py, e geramos o .spr se ele nao existir. Assim
            # o script do P2 continua importavel como esta, que e o ponto da
            # compatibilidade de formato (ver CLAUDE.md).
            if not spr.lower().endswith(".spr"):
                resolved = resolve_to_spr(spr)
                if resolved is None:
                    skipped.append(f"{name} -> {spr} (sem regra de conversao)")
                    continue
                spr = resolved

            x = e.get("x", "0")
            y = e.get("y", "0")
            w = e.get("width", "0")
            h = e.get("height", "0")

            # O rect do script foi escrito para a arte ORIGINAL do P2 (um atlas),
            # nao para a arte que esta aqui. Se ele cai fora da imagem, usa o
            # quadro inteiro em vez de emitir um recorte invalido.
            x, y, w, h = fit_rect(spr, x, y, w, h, name)

            for res in RESOLUTIONS:
                lines.append(f"{name}\t{res}\t{spr}\t{x} {y} {w} {h}")

        if not lines:
            continue

        out = os.path.join(SPRITES, f"{weapon}.txt")
        with open(out, "w", encoding="ascii", newline="\n") as f:
            f.write(f"{len(lines)}\n")
            for line in lines:
                f.write(line + "\n")

        total += 1
        print(f"{weapon}.txt: {len(lines)} linhas")
        for s in skipped:
            print(f"    ignorado (nao e .spr): {s}")

    print(f"\n{total} arquivo(s) gerado(s) em game_dir/sprites/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
