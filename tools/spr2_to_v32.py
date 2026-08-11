#!/usr/bin/env python3
"""Converte .spr v2 (HL indexado) em .spr v32 (truecolor RGBA).

USO: spr2_to_v32.py <entrada.spr> <saida.spr>

Motivacao (RTN F10): o bloodyhud.spr do P2 e v2 indexado com fundo PRETO
opaco. No engine, texFormat=1 (SPR_ALPHTEST) usa #masked.pal: indice 255 =
transparente, resto = opaco. Resultado: o sangue escuro fica OPACO (parece
preto) e o SPR_Set fixa o alpha do vertice em 255 -> nunca translucido.

Conversao p/ v32 (truecolor RGBA):
  - pixel com cor ~preta  -> alpha 0 (transparente)
  - pixel de sangue       -> alpha = brilho (max(r,g,b)) -> translucido real
  - preserva a cor original do sangue
"""
import struct
import sys
from collections import Counter

def rgb565(v):
    r = (v >> 11) & 0x1F; g = (v >> 5) & 0x3F; b = v & 0x1F
    return ((r << 3) | (r >> 2)), ((g << 2) | (g >> 4)), ((b << 3) | (b >> 2))

def spr2_to_v32(src, dst):
    d = open(src, 'rb').read()
    ident, ver, typ, texf, br, b1, b2, nf, face, sync = struct.unpack('<iiIIiiiiII', d[:40])
    assert ident == 0x50534449 and ver == 2, 'nao e um .spr v2 HL'
    numcolors = struct.unpack('<H', d[40:42])[0]
    pal = d[42:42 + numcolors * 3]
    off = 42 + numcolors * 3
    ftype = struct.unpack('<I', d[off:off+4])[0]
    ox, oy, w, h = struct.unpack('<iiii', d[off+4:off+20])
    pix_start = off + 20
    pixels = d[pix_start:pix_start + w * h]
    print(f'origem: {w}x{h} texFormat={texf} ftype={ftype} numcolors={numcolors}')

    # ---- conta o que temos ----
    used = Counter(pixels)
    black_idx = [i for i in used if pal[i*3] < 24 and pal[i*3+1] < 24 and pal[i*3+2] < 24]
    print(f'indices usados: {len(used)}, indices escuros (<24): {len(black_idx)}')

    # ---- monta RGBA ----
    body = bytearray()
    n_alpha0 = 0
    for idx in pixels:
        i = idx
        r, g, b = pal[i*3], pal[i*3+1], pal[i*3+2]
        if i == 255 or (r < 24 and g < 24 and b < 24):
            # fundo preto ou indice 255 -> transparente
            body += b'\x00\x00\x00\x00'
            n_alpha0 += 1
        else:
            a = max(r, g, b)  # brilho = alpha (translucido)
            body += bytes((r, g, b, a))
    print(f'pixels transparentes: {n_alpha0} de {w*h}')

    # ---- header v32 ----
    boundingradius = int(((w*w + h*h) ** 0.5) / 2)
    header = struct.pack(
        '<iiifiiifi',
        0x50534449, 32, 0, float(boundingradius),
        int(-(w/2) * 65536), int((w/2) * 65536), 1, 0.0, 0)
    frametype = struct.pack('<i', 0)  # FRAME_SINGLE (obrigatorio)
    frame = struct.pack('<iiii', 0, 0, w, h)

    with open(dst, 'wb') as f:
        f.write(header)
        f.write(frametype)
        f.write(frame)
        f.write(body)

    total = len(header) + len(frametype) + len(frame) + len(body)
    expected = 56 + w * h * 4
    print(f'{src} -> {dst}  ({w}x{h} v32 RGBA, {total} bytes, esperado {expected})')
    assert total == expected, 'TAMANHO ERRADO'

if __name__ == '__main__':
    spr2_to_v32(sys.argv[1], sys.argv[2])
