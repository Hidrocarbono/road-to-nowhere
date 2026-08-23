#!/usr/bin/env python3
"""
gen_titlefont.py - gera um atlas de fonte bitmap (RTN) a partir de um .ttf

POR QUE ISTO EXISTE
--------------------
titles.txt (o sistema de falas/mensagens do RTN) e desenhado pelo engine
inteiramente com uma fonte fixa embutida nele (gEngfuncs.pfnDrawCharacter, sem
parametro de tamanho nem de fonte - client/enginecallback.h: TextMessageDrawChar).
Nao ha como pedir ao engine "desenha maior" ou "desenha com outra fonte": essa
API literalmente nao tem esses parametros.

Para dar tamanho e fonte customizados por mensagem, o RTN passou a ter seu
PROPRIO renderizador de texto (client/hud_titlefont.cpp) que desenha cada letra
como um quad texturizado via DrawSpriteAsPoly() (client/render/tri.cpp - ja
existia no motor, so nunca tinha sido usado para isto). Esse renderizador le um
ATLAS - uma unica textura com todas as letras da fonte, coladas lado a lado - e
uma lista de METRICAS (onde cada letra esta no atlas, e quanto espaco ela ocupa
na linha). Este script gera as duas coisas a partir de um .ttf comum, porque o
engine nao consegue rasterizar TTF em tempo real (isso e coisa de biblioteca de
fonte - FreeType/stb_truetype -, e nada disso esta disponivel no lado do
client.dll deste mod).

USO
---
    pip install Pillow      (uma vez so, so precisa pra RODAR este script)
    python3 utils/gen_titlefont.py <fonte.ttf> <nome_curto>

Gera:
    game_dir/sprites/fonts/<nome_curto>.spr        (atlas, TGA/SPR v32 RGBA)
    game_dir/fonts/<nome_curto>.rtnfont             (metricas, texto simples)

<nome_curto> e o que se usa no titles.txt: "$font <nome_curto>".

FORMATO DO .rtnfont (proprio do RTN, nao e o .fnt binario do engine)
---------------------------------------------------------------------
Texto simples, um comando por linha, "#" comenta a linha inteira:
    sprite <caminho do .spr, relativo a game_dir>
    size <tamanho em pixels em que o atlas foi rasterizado>
    lineheight <altura de uma linha, no mesmo tamanho de bake>
    glyph <codigo> <atlasX> <atlasY> <atlasW> <atlasH> <avanco>

Todas as unidades de glyph/lineheight sao em PIXELS DO ATLAS (tamanho de bake).
client/hud_titlefont.cpp escala tudo por (fontsize_pedido / size) na hora de
desenhar - ou seja, um bake so serve qualquer $fontsize, so perde nitidez se
pedido muito maior que o tamanho de bake.

CONJUNTO DE CARACTERES
-----------------------
ASCII imprimivel (32-126) + Latin-1 (160-255, cobre acentos do portugues:
a a a a e e i i o o o o u u c C, mesma faixa que game_dir/*_cp1252.fnt ja usa
neste projeto). Codepoint Unicode == byte cp1252 nessas duas faixas, entao nao
precisa de tabela de conversao.
"""

import struct
import sys
import os

try:
    from PIL import Image, ImageFont, ImageDraw
except ImportError:
    print("Precisa do Pillow: pip install Pillow", file=sys.stderr)
    sys.exit(1)

BAKE_SIZE = 128        # px - alto o bastante pra titulo de "episodio" grande
PADDING = 4             # px de margem em volta de cada glifo no atlas (evita sangramento)
CHARSET = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100))

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) != 3:
        print(f"uso: {sys.argv[0]} <fonte.ttf> <nome_curto>", file=sys.stderr)
        return 1

    ttf_path, name = sys.argv[1], sys.argv[2]
    font = ImageFont.truetype(ttf_path, BAKE_SIZE)

    # mede cada glifo primeiro, pra saber o tamanho da celula do atlas
    metrics = {}
    max_w = max_h = 0
    ascent, descent = font.getmetrics()
    lineheight = ascent + descent

    for code in CHARSET:
        ch = chr(code)
        bbox = font.getbbox(ch)
        if bbox is None:
            # espaco/caractere sem tinta: sem retangulo, so avanco
            advance = font.getlength(ch)
            metrics[code] = (0, 0, 0, 0, advance)
            continue
        x0, y0, x1, y1 = bbox
        w, h = x1 - x0, y1 - y0
        advance = font.getlength(ch)
        metrics[code] = (x0, y0, w, h, advance)
        max_w = max(max_w, w)
        max_h = max(max_h, h)

    # grade quadrada o bastante pra caber todos os glifos
    cell_w = max_w + PADDING * 2
    cell_h = max_h + PADDING * 2
    cols = 16
    rows = (len(CHARSET) + cols - 1) // cols
    atlas_w = cols * cell_w
    atlas_h = rows * cell_h

    # RGBA, comeca transparente; glifos desenhados em BRANCO (a cor real vem
    # de $color/$color2 no titles.txt, aplicada como tint no draw - o mesmo
    # esquema que $color ja usa pra fonte classica).
    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)

    glyphs = []  # (code, atlasX, atlasY, atlasW, atlasH, advance)
    for i, code in enumerate(CHARSET):
        col, row = i % cols, i // cols
        cellX, cellY = col * cell_w, row * cell_h
        x0, y0, w, h, advance = metrics[code]

        if w > 0 and h > 0:
            ch = chr(code)
            # desenha deslocando pela bbox (x0,y0) pra origem da celula, com
            # a margem de padding - garante que a tinta caia dentro da celula
            # mesmo pra glifos com overshoot negativo (itaicos, cedilha etc.)
            draw.text((cellX + PADDING - x0, cellY + PADDING - y0), ch, font=font, fill=(255, 255, 255, 255))
            glyphs.append((code, cellX + PADDING, cellY + PADDING, w, h, advance))
        else:
            glyphs.append((code, 0, 0, 0, 0, advance))

    # ---- escreve o atlas como .spr v32 (mesmo formato de bloodyhud.spr) ----
    out_spr_dir = os.path.join(ROOT, "game_dir", "sprites", "fonts")
    os.makedirs(out_spr_dir, exist_ok=True)
    spr_path = os.path.join(out_spr_dir, f"{name}.spr")
    _write_spr_v32(atlas, spr_path)

    # ---- escreve as metricas ----
    out_fonts_dir = os.path.join(ROOT, "game_dir", "fonts")
    os.makedirs(out_fonts_dir, exist_ok=True)
    rtnfont_path = os.path.join(out_fonts_dir, f"{name}.rtnfont")
    with open(rtnfont_path, "w", encoding="ascii", newline="\n") as f:
        f.write(f"# gerado por utils/gen_titlefont.py a partir de {os.path.basename(ttf_path)}\n")
        f.write("# NAO EDITAR A MAO - gere de novo a partir do .ttf se precisar mudar algo\n")
        f.write(f"sprite sprites/fonts/{name}.spr\n")
        f.write(f"size {BAKE_SIZE}\n")
        f.write(f"lineheight {lineheight}\n")
        for code, gx, gy, gw, gh, advance in glyphs:
            f.write(f"glyph {code} {gx} {gy} {gw} {gh} {advance:.2f}\n")

    print(f"{spr_path}: atlas {atlas_w}x{atlas_h}, {len(glyphs)} glifos")
    print(f"{rtnfont_path}: metricas ({BAKE_SIZE}px de bake)")
    print(f'\nUse no titles.txt: $font {name}')
    return 0


def _write_spr_v32(img, dst):
    """Mesmo formato validado em tools/spr2_to_v32.py e usado por
    game_dir/sprites/bloodyhud.spr - conferido contra o codigo-fonte real do
    engine (SPRITE_VERSION_32 usa o layout de dsprite_q1_t, 36 bytes de
    cabecalho, RGBA cru em seguida). Ver o comentario grande em
    tools/spr2_to_v32.py para o porque desse formato especifico."""
    w, h = img.size
    body = img.tobytes()  # RGBA ja na ordem certa

    boundingradius = int(((w * w + h * h) ** 0.5) / 2)
    header = struct.pack(
        "<iiifiiifi",
        0x50534449, 32, 0, float(boundingradius),
        int(-(w / 2) * 65536), int((w / 2) * 65536), 1, 0.0, 0)
    frametype = struct.pack("<i", 0)  # FRAME_SINGLE
    frame = struct.pack("<iiii", 0, 0, w, h)

    with open(dst, "wb") as f:
        f.write(header)
        f.write(frametype)
        f.write(frame)
        f.write(body)


if __name__ == "__main__":
    sys.exit(main())
