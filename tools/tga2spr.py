#!/usr/bin/env python3
"""Converte TGA (RGBA 32bpp) em .spr do GoldSrc — versao 32 (TRUE COLOR RGBA).

Por que versao 32 em vez de versao 2 (Half-Life)?
  - .spr v2 com texFormat=SPR_INDEXALPHA usa a paleta '#gradient.pal' do engine:
    cor = pal[765..767] (UMA cor so) e alpha = INDICE do pixel. Ou seja, o
    sprite fica MONOCROMATICO (perde as cores do TGA).
  - .spr v32 (truecolor): pixels RGBA direto. O Mod_SpriteLoadFrame passa
    w*h*4 bytes p/ GL_LoadTexture e o Image_LoadSPR detecta filesize == w*h*4
    -> truecolor -> PF_RGBA_32. Cores originais preservadas + alpha real.

Estrutura .spr v32:
  dsprite_q1_t (36 bytes):
    ident  int32 'IDSP'
    version int32 32
    type   int32 0 (SPR_FWD_PARALLEL_UPRIGHT)
    boundingradius float
    bounds[2] int32 (mins/maxs 16.16)
    numframes int32 1
    beamlength float 0
    synctype uint32 0
  dspriteframe_t (16 bytes): origin[2] (0,0) + width + height
  pixels: w*h*4 bytes RGBA

O engine forca texFormat=SPR_ADDITIVE p/ v32 (cl_sprite.c: psprite->texFormat
e Image_LoadSPR). Desenhar com SPR_DrawAdditive no HUD.
"""
import struct
import sys
from PIL import Image

def tga_to_spr(tga_path, spr_path, white=False):
    im = Image.open(tga_path).convert('RGBA')
    w, h = im.size
    pixels = list(im.getdata())

    # boundingradius ~ metade da diagonal
    boundingradius = int(((w*w + h*h) ** 0.5) / 2)

    # RTN FIX: estes dois campos do dsprite_q1_t sao WIDTH e HEIGHT do sprite,
    # nao um "bounds[2]" - a versao anterior gravava aqui um par de valores em
    # ponto-fixo (-(w/2)*65536, +(w/2)*65536), que sao a ORIGEM do frame, no
    # slot errado. Resultado: todo .spr gerado por estas ferramentas declarava
    # largura/altura absurdas no header (ex.: -60293120 x 60293120 num atlas
    # 1840x1776).
    #
    # Na pratica isso nunca quebrou o jogo porque o engine usa a largura/altura
    # do FRAME (escrito logo abaixo, sempre correto) pra desenhar e pra
    # SPR_Width/SPR_Height - prova disso e que todos os .spr atuais do mod tem
    # esse header errado e funcionam. Mas o header ficava mentindo, e a
    # etiqueta "bounds[2]" ja levou o erro a ser copiado pra outros dois
    # scripts. Os .spr existentes NAO precisam ser regerados.
    header = struct.pack(
        '<iiifiiifi',
        0x50534449,  # 'IDSP' little-endian
        32,          # version = SPRITE_VERSION_32 (truecolor)
        0,           # type: SPR_FWD_PARALLEL_UPRIGHT
        float(boundingradius),
        w, h,        # width, height
        1,           # numframes
        0.0,         # beamlength
        0,           # synctype
    )
    # dframetype_t (4 bytes) OBRIGATORIO entre header e frame:
    # Mod_SwapSprite (mod_sprite.c:173) le o frametype ANTES de cada frame.
    # Sem ele, o engine le origin[0] como frametype e desloca tudo 4 bytes ->
    # sprite nao carrega (fallback letra). type=0 = FRAME_SINGLE.
    frametype = struct.pack('<i', 0)
    frame = struct.pack('<iiii', 0, 0, w, h)

    # pixels RGBA direto. white=True: forca RGB branco (mantem alpha) p/ o
    # SPR_Set(r,g,b) do client tingir na cor do HUD (gHUD.m_color).
    body = b''
    for r, g, b, a in pixels:
        if white:
            r = g = b = 255
        body += struct.pack('<BBBB', r, g, b, a)

    with open(spr_path, 'wb') as f:
        f.write(header)
        f.write(frametype)
        f.write(frame)
        f.write(body)

    total = len(header) + len(frametype) + len(frame) + len(body)
    print(f'{tga_path} -> {spr_path}  ({w}x{h} truecolor RGBA, {total} bytes)')

if __name__ == '__main__':
    white = '--white' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--white']
    tga_to_spr(args[0], args[1], white)
