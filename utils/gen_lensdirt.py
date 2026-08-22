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

    def bokeh(cx, cy, radius, intensity, tint, rim=0.55, fringe=0.25):
        """Disco de bokeh: ponto de luz FORA DE FOCO.

        E a forma dominante numa textura de lens dirt de verdade, e o que a
        primeira versao deste script errou - ela so tinha borroes gaussianos, que
        parecem fumaca, nao lente suja.

        Um ponto desfocado nao vira um borrao com centro brilhante: vira um DISCO
        de brilho quase uniforme, com a BORDA mais clara que o meio (a luz se
        acumula no anel de confusao) e franja cromatica na beirada (aberracao
        da lente). `rim` controla o realce do anel, `fringe` o quanto R e B se
        separam nele.
        """
        r0 = int(radius) + 2
        for dy in range(-r0, r0 + 1):
            yy = int(cy) + dy
            if yy < 0 or yy >= HEIGHT:
                continue
            for dx in range(-r0, r0 + 1):
                d = math.hypot(dx, dy) / radius
                if d >= 1.12:
                    continue

                # interior quase chapado; cai rapido logo depois de d = 1
                if d <= 1.0:
                    a = 1.0
                else:
                    a = max(0.0, 1.0 - (d - 1.0) / 0.12)

                # anel de confusao: pico de brilho colado na borda
                a *= 1.0 + rim * math.exp(-((d - 0.93) ** 2) / 0.006)
                a *= intensity

                # franja cromatica: vermelho puxa para fora, azul para dentro
                fr = 1.0 + fringe * (d - 0.6)
                fb = 1.0 - fringe * (d - 0.6)
                add(int(cx) + dx, yy,
                    a * tint[0] * fr,
                    a * tint[1],
                    a * tint[2] * fb)

    # Densidade nao uniforme: a sujeira se concentra numa faixa diagonal, como
    # numa lente real (e como na referencia do ReShade). Uniforme demais parece
    # ruido de tela, nao sujeira.
    def density(x, y):
        u = x / WIDTH
        v = y / HEIGHT
        band = math.exp(-((u * 0.8 + v * 0.6 - 0.7) ** 2) / 0.09)
        return 0.12 + 0.88 * band

    def place():
        """Sorteia uma posicao respeitando a densidade da faixa."""
        for _ in range(40):
            x = random.uniform(0, WIDTH)
            y = random.uniform(0, HEIGHT)
            if random.random() < density(x, y):
                return x, y
        return random.uniform(0, WIDTH), random.uniform(0, HEIGHT)

    # Paleta fria com desvios roxo/rosa/ambar, como na referencia do ReShade.
    # A primeira versao usava um cinza levemente ruidoso, e o resultado parecia
    # sobreposicao digital em vez de vidro sujo - a cor e o que vende o efeito.
    TINTS = (
        (0.82, 0.88, 1.00),   # azul frio (o dominante)
        (0.90, 0.86, 1.00),   # lilas
        (1.00, 0.86, 0.96),   # rosa
        (1.00, 0.95, 0.82),   # ambar
        (0.86, 1.00, 0.95),   # verde-agua
        (0.95, 0.95, 0.95),   # neutro
    )
    TINT_WEIGHTS = (34, 20, 14, 10, 8, 14)

    def tint():
        base = random.uniform(0.78, 1.0)
        c = random.choices(TINTS, weights=TINT_WEIGHTS, k=1)[0]
        j = lambda v: base * v * random.uniform(0.94, 1.06)
        return (j(c[0]), j(c[1]), j(c[2]))

    # --- halos difusos de fundo: gordura/umidade espalhada na lente ----------
    # Ficam por baixo de tudo e dao a "sujeira geral"; sozinhos parecem fumaca,
    # por isso sao poucos e fracos.
    for _ in range(55):
        x, y = place()
        blob(x, y, random.uniform(18, 52), random.uniform(0.03, 0.09), tint(), 2.4)

    # --- BOKEHS: a forma dominante da textura --------------------------------
    # Grandes e translucidos ao fundo...
    for _ in range(75):
        x, y = place()
        bokeh(x, y, random.uniform(16, 44), random.uniform(0.04, 0.11), tint(),
              rim=random.uniform(0.4, 0.9), fringe=random.uniform(0.15, 0.4))

    # ...medios, o corpo do efeito...
    for _ in range(150):
        x, y = place()
        bokeh(x, y, random.uniform(6, 18), random.uniform(0.08, 0.26), tint(),
              rim=random.uniform(0.3, 0.8), fringe=random.uniform(0.1, 0.35))

    # ...e pequenos e brilhantes, que sao os que de fato acendem contra a luz.
    for _ in range(300):
        x, y = place()
        bokeh(x, y, random.uniform(2.5, 7), random.uniform(0.22, 0.75), tint(),
              rim=random.uniform(0.2, 0.6), fringe=0.12)

    # --- poeira fina: pontos duros, quebram a regularidade dos discos --------
    for _ in range(900):
        x, y = place()
        blob(x, y, random.uniform(1.0, 2.6), random.uniform(0.4, 1.0), tint(), 1.1)

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
