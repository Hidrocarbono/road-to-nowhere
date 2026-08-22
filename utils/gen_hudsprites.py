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
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, "game_dir", "scripts", "weapons")
SPRITES = os.path.join(ROOT, "game_dir", "sprites")

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

            # SPR_Load so abre .spr. Os scripts do Paranoia 2 as vezes apontam o
            # icone de municao para um .tga do VGUI (o HUD dele lia os dois
            # formatos); aqui esse caminho simplesmente nao existe, e emitir a
            # linha faria o cliente falhar ao carregar a lista INTEIRA.
            if not spr.lower().endswith(".spr"):
                skipped.append(f"{name} -> {spr}")
                continue

            x = e.get("x", "0")
            y = e.get("y", "0")
            w = e.get("width", "0")
            h = e.get("height", "0")
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
