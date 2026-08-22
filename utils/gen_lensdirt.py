#!/usr/bin/env python3
"""
gen_lensdirt.py - gera game_dir/textures/lensdirt.tga (sujeira de lente, RTN)

POR QUE ESTE SCRIPT EXISTE
--------------------------
A textura anterior era praticamente preta: luminancia media 0.022 e MAXIMA
0.094. Combinada com a mascara do shader (brilho da cena acima de 0.7, escala
0.5), a contribuicao maxima possivel na tela era

    0.094 * 0.30 * 0.50 = 0.014

ou seja ~3.6 de 255 num pixel que ja estava estourado de luz. O efeito existia
e funcionava - so era invisivel por construcao. Dai "nunca vi o efeito".

O que muda aqui: a textura passa a ter faixa dinamica de verdade (bokehs
chegando perto de 1.0), que e o que uma textura de lens dirt do ReShade tem.

USO
---
    python3 utils/gen_lensdirt.py

Gera game_dir/textures/lensdirt.tga (512x512, 32bpp, sem compressao) - o mesmo
formato/caminho que CBasePostEffects::InitLensDirt() carrega, entao e so rodar e
testar. Sem dependencias externas (nem numpy nem PIL): TGA sem compressao e um
cabecalho de 18 bytes seguido dos pixels em BGRA.

SUBSTITUIR POR UMA TEXTURA PROPRIA
----------------------------------
Qualquer textura de lens dirt (ReShade, Battlefield, feita a mao) serve: basta
salvar como TGA 32 bits sem compressao RLE em game_dir/textures/lensdirt.tga.
Nao precisa ser 512x512. O que importa e ter pontos BRILHANTES sobre fundo
escuro - uma textura escura demais volta a ficar invisivel, que foi o problema
original.
"""

import math
import os
import random
import struct

WIDTH = 512
HEIGHT = 512
SEED = 20260822  # fixo: a mesma textura toda vez que se rodar o script

OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "game_dir", "textures", "lensdirt.tga",
)


def main():
    random.seed(SEED)

    # acumulador em float por canal; satura so na hora de escrever
    buf = [[0.0, 0.0, 0.0] for _ in range(WIDTH * HEIGHT)]

    def add(x, y, r, g, b):
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            p = buf[y * WIDTH + x]
            p[0] += r
            p[1] += g
            p[2] += b

    def blob(cx, cy, radius, intensity, tint, falloff):
        """Mancha radial suave. `falloff` alto = borda mais dura (poeira),
        baixo = halo difuso (bokeh desfocado)."""
        r0 = int(radius) + 1
        for dy in range(-r0, r0 + 1):
            yy = int(cy) + dy
            if yy < 0 or yy >= HEIGHT:
                continue
            for dx in range(-r0, r0 + 1):
                d = math.hypot(dx, dy) / radius
                if d >= 1.0:
                    continue
                a = (1.0 - d) ** falloff * intensity
                add(int(cx) + dx, yy, a * tint[0], a * tint[1], a * tint[2])

    # Densidade nao uniforme: a sujeira se concentra numa faixa diagonal, como
    # numa lente real (e como na referencia do ReShade). Uniforme demais parece
    # ruido de tela, nao sujeira.
    def density(x, y):
        u = x / WIDTH
        v = y / HEIGHT
        band = math.exp(-((u * 0.8 + v * 0.6 - 0.7) ** 2) / 0.12)
        return 0.25 + 0.75 * band

    def place():
        """Sorteia uma posicao respeitando a densidade da faixa."""
        for _ in range(40):
            x = random.uniform(0, WIDTH)
            y = random.uniform(0, HEIGHT)
            if random.random() < density(x, y):
                return x, y
        return random.uniform(0, WIDTH), random.uniform(0, HEIGHT)

    def tint():
        """Leve variacao cromatica - sujeira real difrata a luz, e um cinza puro
        parece sobreposicao digital."""
        base = random.uniform(0.80, 1.0)
        return (
            base * random.uniform(0.88, 1.0),
            base * random.uniform(0.90, 1.0),
            base * random.uniform(0.88, 1.05),
        )

    # --- bokehs grandes e difusos: as manchas de gordura/agua na lente -------
    for _ in range(90):
        x, y = place()
        blob(x, y, random.uniform(14, 46), random.uniform(0.10, 0.34), tint(), 2.2)

    # --- bokehs medios, mais definidos --------------------------------------
    for _ in range(260):
        x, y = place()
        blob(x, y, random.uniform(5, 16), random.uniform(0.22, 0.62), tint(), 1.6)

    # --- poeira fina: os pontos que realmente "acendem" contra a luz ---------
    for _ in range(1400):
        x, y = place()
        blob(x, y, random.uniform(1.2, 3.6), random.uniform(0.45, 1.0), tint(), 1.1)

    # --- riscos: arcos finos, o que mais denuncia "lente" e nao "tela" -------
    for _ in range(14):
        x, y = place()
        ang = random.uniform(0, math.tau)
        curve = random.uniform(-0.02, 0.02)
        length = random.uniform(30, 150)
        width = random.uniform(0.8, 1.8)
        bright = random.uniform(0.35, 0.9)
        t = tint()
        steps = int(length * 3)
        for i in range(steps):
            f = i / steps
            ang += curve
            x += math.cos(ang) * (length / steps)
            y += math.sin(ang) * (length / steps)
            # some nas pontas, para o risco nao comecar e terminar em corte seco
            fade = math.sin(f * math.pi) ** 0.6
            blob(x, y, width, bright * fade * 0.5, t, 1.0)

    # --- grao muito sutil: evita que as areas limpas fiquem preto chapado ----
    for i in range(len(buf)):
        n = random.uniform(0.0, 0.012)
        buf[i][0] += n
        buf[i][1] += n
        buf[i][2] += n

    # --- escrita do TGA (18 bytes de cabecalho + pixels BGRA) ---------------
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
    peak = 0.0
    total = 0.0
    for p in buf:
        r = min(1.0, p[0])
        g = min(1.0, p[1])
        b = min(1.0, p[2])
        lum = r * 0.299 + g * 0.587 + b * 0.114
        peak = max(peak, lum)
        total += lum
        out += bytes((int(b * 255), int(g * 255), int(r * 255), 255))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(out)

    print(f"{OUT}: {WIDTH}x{HEIGHT} 32bpp")
    print(f"luminancia media={total / len(buf):.3f} pico={peak:.3f}")


if __name__ == "__main__":
    main()
