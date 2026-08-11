#!/usr/bin/env python3
"""RTN F10: pipeline automatizado TGA -> .spr p/ o HUD estilo Paranoia 2.

Converte TODOS os .tga de gfx/vgui/ (e gfx/vgui/ammo/) em .spr v32 RGBA
(sprites de HUD) em sprites/rtn_hud_*.spr — ninguem converte manualmente.

Regras:
  - gfx/vgui/640_X.tga          -> sprites/rtn_hud_X.spr   (cores originais)
  - gfx/vgui/ammo/640_Y.tga     -> sprites/rtn_hud_ammo_Y.spr (--white: silhueta
    de arma em BRANCO, o HUD tinge via SPR_Set se quiser)

Uso: python3 tools/convert_vgui.py [--game-dir game_dir]
"""
import os
import sys
import glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tga2spr import tga_to_spr  # noqa: E402

def main():
    game_dir = "game_dir"
    if "--game-dir" in sys.argv:
        game_dir = sys.argv[sys.argv.index("--game-dir") + 1]

    vgui_dir = os.path.join(game_dir, "gfx", "vgui")
    ammo_dir = os.path.join(vgui_dir, "ammo")
    spr_dir = os.path.join(game_dir, "sprites")
    os.makedirs(spr_dir, exist_ok=True)

    converted = 0
    for tga in sorted(glob.glob(os.path.join(vgui_dir, "640_*.tga"))):
        base = os.path.basename(tga)          # 640_armor_full.tga
        name = base[len("640_"):-len(".tga")] # armor_full
        spr = os.path.join(spr_dir, "rtn_hud_" + name + ".spr")
        tga_to_spr(tga, spr, white=False)
        converted += 1
        print(f"  {base} -> {os.path.basename(spr)}")

    for tga in sorted(glob.glob(os.path.join(ammo_dir, "640_*.tga"))):
        base = os.path.basename(tga)           # 640_weapon_mp5.tga
        name = base[len("640_"):-len(".tga")]  # weapon_mp5
        spr = os.path.join(spr_dir, "rtn_hud_ammo_" + name + ".spr")
        tga_to_spr(tga, spr, white=True)       # silhueta de arma em BRANCO
        converted += 1
        print(f"  ammo/{base} -> {os.path.basename(spr)} (branco)")

    print(f"convert_vgui: {converted} sprites gerados em {spr_dir}")

if __name__ == "__main__":
    main()
