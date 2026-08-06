#!/usr/bin/env python3
"""Converte TGA (RGBA 32bpp) em .spr do GoldSrc (Half-Life sprite, version 2).

Formato .spr HL:
  dsprite_hl_t (40 bytes):
    ident  int32 'IDSP' (little-endian)
    version int32 2
    type   uint32 0 (SPR_FWD_PARALLEL_UPRIGHT)
    texFormat uint32 2 (SPR_INDEXALPHA) -> usa paleta+alpha indexado
    boundingradius int32
    bounds[2] int32 (mins/maxs em 16.16)
    numframes int32 1
    facetype uint32 0
    synctype uint32 0
  dspriteframe_t (16 bytes):
    origin[2] int32 (0,0)
    width int32
    height int32
  pixels: width*height bytes (paleta indexada) + width*height bytes (alpha)
  paleta: 256*3 bytes RGB
"""
import struct
import sys
from PIL import Image

def tga_to_spr(tga_path, spr_path):
    im = Image.open(tga_path).convert('RGBA')
    w, h = im.size
    pixels = list(im.getdata())

    # quantiza RGB para 256 cores (sem dithering p/ nao sujar o alpha)
    rgb = Image.new('RGB', (w, h))
    rgb.putdata([(r, g, b) for r, g, b, a in pixels])
    q = rgb.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()  # 768 bytes
    idx = list(q.getdata())  # indices

    alpha = [a for r, g, b, a in pixels]

    # boundingradius ~ metade da diagonal
    boundingradius = int(((w*w + h*h) ** 0.5) / 2)
    # bounds[2] = mins, maxs (2 ints em 16.16)
    mins_b = int(-(w/2) * 65536)
    maxs_b = int((w/2) * 65536)

    header = struct.pack(
        '<iiIIiiiiII',
        0x50534449,  # 'IDSP' little-endian
        2,           # version HL
        0,           # type: SPR_FWD_PARALLEL_UPRIGHT
        2,           # texFormat: SPR_INDEXALPHA
        boundingradius,
        mins_b, maxs_b,  # bounds[2]
        1,           # numframes
        0,           # facetype
        0,           # synctype
    )
    frame = struct.pack(
        '<iiii',
        0, 0,       # origin[2]
        w, h,       # width, height
    )

    body = bytes(idx) + bytes(alpha) + bytes(pal)

    with open(spr_path, 'wb') as f:
        f.write(header)
        f.write(frame)
        f.write(body)

    print(f'{tga_path} -> {spr_path}  ({w}x{h}, {len(header)+len(frame)+len(body)} bytes)')

if __name__ == '__main__':
    tga_to_spr(sys.argv[1], sys.argv[2])
