#!/usr/bin/env python3
"""
gen_clouds.py - gera game_dir/textures/clouds.tga (mapa de cobertura de nuvem,
tileavel, RTN)

POR QUE ESTE SCRIPT EXISTE
--------------------------
O skybox do RTN e o de 6 faces pintadas (TGA em gfx/env/), estatico - a cena
pintada (montanha, predio, ceu) nao se move. O engine (fork Xash3D-FWGS) tem um
sistema NATIVO de nuvem animada (R_CloudTexCoord/R_CloudDrawPoly, gl_warp.c),
mas ele e mutuamente exclusivo com esse skybox de 6 faces (so roda quando o
mapa usa uma textura "SKY" antiga num brush, sem sv_skyname) - ligar ele
significaria abandonar as texturas pintadas que ja existem.

A alternativa de baixo custo: uma SEGUNDA camada, amostrada dentro do proprio
skybox_fp.glsl por cima do sky_color existente, com UV que escorrega com o
tempo (u_CloudParams, calculado em client/render/gl_sky.cpp a partir de
tr.time - sem rede, sem passe novo). Essa camada precisa ser uma textura de
COBERTURA (alpha = quanto de nuvem cobre aquele ponto, RGB neutro) tileavel
nas boas - se ela tiver costura, o scroll deixa a costura viajando pela tela
o tempo todo, muito mais visivel que numa textura estatica.

TILING
------
Cada "blob" e desenhado com coordenada ENVOLVIDA (wrap por modulo em WIDTH/
HEIGHT) usando so o deslocamento LOCAL (dx, dy) do kernel do blob - como o
kernel e pequeno perto do canvas inteiro, isso tileia perfeitamente nas 4
bordas sem precisar desenhar copia nenhuma nas bordas.

USO
---
    python3 utils/gen_clouds.py

Gera game_dir/textures/clouds.tga (512x512, 32bpp BGRA, sem compressao) - TGA
cru, sem dependencia nenhuma (nem PIL nem numpy), no mesmo formato que
utils/gen_lensdirt.py ja usa e que o loader do engine (LOAD_TEXTURE) aceita.

SUBSTITUIR POR UMA TEXTURA PROPRIA
-----------------------------------
Qualquer textura de nuvem tileavel serve, desde que: (a) TGA 32 bits sem RLE,
(b) o ALPHA carregue a cobertura (0 = ceu limpo, 1 = nuvem cheia) - e o canal
que o skybox_fp.glsl le -, (c) tileavel nas 4 bordas (senao o scroll expoe a
costura).
"""

import math
import os
import random
import struct

WIDTH = 512
HEIGHT = 512
SEED = 20260919  # fixo: a mesma textura toda vez que se rodar o script

OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "game_dir", "textures", "clouds.tga",
)


def main():
    random.seed(SEED)

    cov = [0.0] * (WIDTH * HEIGHT)  # cobertura acumulada, 0..~vários antes de saturar

    def add(x, y, a):
        # wrap por modulo: garante tiling perfeito sem desenhar copias na borda
        xi = int(x) % WIDTH
        yi = int(y) % HEIGHT
        cov[yi * WIDTH + xi] += a

    def blob(cx, cy, radius, intensity, falloff):
        """Mancha radial suave, desenhada em coordenada local (wrap so na
        hora de gravar) - falloff alto = borda mais definida, baixo = mais
        difusa/felpuda."""
        r0 = int(radius) + 1
        for dy in range(-r0, r0 + 1):
            for dx in range(-r0, r0 + 1):
                d = math.hypot(dx, dy) / radius
                if d >= 1.0:
                    continue
                a = (1.0 - d) ** falloff * intensity
                add(cx + dx, cy + dy, a)

    def cluster(cx, cy, scale):
        """Um aglomerado de nuvem (cumulus): varios blobs sobrepostos, nao
        um circulo perfeito - senao parece mancha de sombra, nao nuvem."""
        n = random.randint(6, 11)
        for _ in range(n):
            ox = random.uniform(-0.55, 0.55) * scale
            oy = random.uniform(-0.35, 0.35) * scale
            r = random.uniform(0.35, 0.75) * scale
            inten = random.uniform(0.35, 0.65)
            blob(cx + ox, cy + oy, r, inten, random.uniform(1.4, 2.4))

    # --- aglomerados grandes: a forma principal das nuvens -------------------
    for _ in range(22):
        cx = random.uniform(0, WIDTH)
        cy = random.uniform(0, HEIGHT)
        cluster(cx, cy, random.uniform(45, 95))

    # --- aglomerados pequenos: quebram a regularidade, dao variedade de escala
    for _ in range(30):
        cx = random.uniform(0, WIDTH)
        cy = random.uniform(0, HEIGHT)
        cluster(cx, cy, random.uniform(15, 35))

    # --- ruido fino por cima: borda felpuda em vez de silhueta lisa ----------
    # Soma de senos com frequencia INTEIRA em WIDTH/HEIGHT - isso e
    # automaticamente periodico no tile, sem precisar de wrap.
    phases = [random.uniform(0, math.tau) for _ in range(6)]
    freqs = [3, 5, 7, 11, 13, 17]
    amps = [0.10, 0.08, 0.06, 0.05, 0.04, 0.03]
    for y in range(HEIGHT):
        v = y / HEIGHT
        for x in range(WIDTH):
            u = x / WIDTH
            n = 0.0
            for f, a, p in zip(freqs, amps, phases):
                n += a * math.sin((u * f + v * f * 0.7) * math.tau + p)
            idx = y * WIDTH + x
            if cov[idx] > 0.05:  # so texturiza onde ja ha nuvem, senao vira ruido no ceu limpo
                cov[idx] += n * min(1.0, cov[idx])

    # --- normaliza pra 0..1 com joelho suave (evita platos saturados feios) --
    header = struct.pack(
        "<BBBHHBHHHHBB",
        0,      # id length
        0,      # sem palette
        2,      # tipo 2 = RGB sem compressao
        0, 0, 0,  # palette (nao usada)
        0, 0,   # origem x/y
        WIDTH, HEIGHT,
        32,     # bits por pixel
        8,      # 8 bits de alpha, origem embaixo (padrao TGA)
    )

    out = bytearray(header)
    total = 0.0
    for c in cov:
        # smoothstep-like: realca contraste no miolo, mantem bordas macias
        v = max(0.0, min(1.0, c))
        v = v * v * (3.0 - 2.0 * v)
        total += v
        px = int(v * 255)
        # RGB neutro (branco): so o alpha carrega informacao real; o shader
        # decide a cor/tint da nuvem, essa textura e so mascara de cobertura
        out += bytes((255, 255, 255, px))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(out)

    print(f"{OUT}: {WIDTH}x{HEIGHT} 32bpp")
    print(f"cobertura media={total / len(cov):.3f}")


if __name__ == "__main__":
    main()
