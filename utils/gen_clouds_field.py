#!/usr/bin/env python3
"""
gen_clouds_field.py - reconstroi game_dir/textures/clouds.tga como um CAMPO
continuo de nuvens, tileavel, a partir da arte original (RTN)

POR QUE ESTE SCRIPT EXISTE
--------------------------
A arte enviada em 04d1159 e um aglomerado unico, esmaecido a zero nas 4 bordas
(85% do alpha esta no quadrado central 256x256 de 512x512). Com a textura
repetindo (GL_REPEAT) e o scroll de gl_sky_clouds_speed, a nuvem passa e o ceu
fica limpo ate o proximo ladrilho chegar - o "ceu limpo depois que ela passa".

Este script PRESERVA a arte: recorta o aglomerado e o cola varias vezes, com
posicao, espelhamento, giro de 90 graus e escala variados, combinando o alpha
em "screen" (1-(1-a)(1-b), nunca passa de 1). A colagem usa indice modulo, entao
o resultado tileia nas 4 bordas por construcao, sem costura.

ENTRADA / SAIDA
---------------
    entrada: game_dir/textures/clouds_original.tga  (a arte aprovada, NAO e
             carregada pelo jogo - fica so como fonte e backup)
    saida:   game_dir/textures/clouds.tga           (o que o jogo carrega)

USO
---
    python3 utils/gen_clouds_field.py            # gera e imprime metricas
    python3 utils/gen_clouds_field.py --preview /caminho/preview.png

Requer numpy. --preview requer Pillow (opcional). Determinista (SEED fixo).
Para voltar a arte original: copie clouds_original.tga por cima de clouds.tga.

AJUSTE
------
GRID = quantas colagens por eixo (GRID*GRID no total) - mais = mais cobertura.
SCALE_MIN/SCALE_MAX = tamanho de cada colagem em relacao ao recorte original.
Se a cobertura subir, reduza gl_sky_clouds_opacity no jogo (era 0.35 com a
textura esparsa).
"""

import argparse
import os
import struct
import sys

import numpy as np

SEED = 20261018
GRID = 5              # 5x5 = 25 colagens por ladrilho
SCALE_MIN = 0.55
SCALE_MAX = 0.95
JITTER = 0.45         # fracao do tamanho da celula que o centro pode desviar
PIECE_MIN = 0.50      # cada colagem usa um PEDACO do aglomerado (fracao de cada
PIECE_MAX = 0.80      # eixo), nao ele inteiro - a silhueta inteira repetida
                      # 16x denuncia a copia, pedacos diferentes nao
EDGE = 0.55           # fracao do RAIO, de fora pra dentro, que esmaece suave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "game_dir", "textures", "clouds_original.tga")
OUT = os.path.join(ROOT, "game_dir", "textures", "clouds.tga")


def read_tga_alpha(path):
    d = open(path, "rb").read()
    idl, ctype = d[0], d[2]
    w, h = struct.unpack("<HH", d[12:16])
    bpp = d[16]
    if ctype != 2 or bpp != 32:
        sys.exit("clouds_original.tga precisa ser TGA 32 bits sem RLE")
    px = np.frombuffer(d, dtype=np.uint8, offset=18 + idl, count=w * h * 4)
    px = px.reshape(h, w, 4)
    return px[:, :, 3].astype(np.float32) / 255.0, w, h, d[:18 + idl]


def write_tga(path, header, alpha):
    h, w = alpha.shape
    px = np.empty((h, w, 4), dtype=np.uint8)
    px[:, :, 0:3] = 255                      # RGB neutro (so o alpha e lido)
    px[:, :, 3] = np.clip(alpha * 255.0 + 0.5, 0, 255).astype(np.uint8)
    with open(path, "wb") as f:
        f.write(header)
        f.write(px.tobytes())


def resize_bilinear(img, new_h, new_w):
    h, w = img.shape
    ys = (np.arange(new_h) + 0.5) * h / new_h - 0.5
    xs = (np.arange(new_w) + 0.5) * w / new_w - 0.5
    y0 = np.clip(np.floor(ys).astype(int), 0, h - 1)
    x0 = np.clip(np.floor(xs).astype(int), 0, w - 1)
    y1 = np.clip(y0 + 1, 0, h - 1)
    x1 = np.clip(x0 + 1, 0, w - 1)
    fy = np.clip(ys - y0, 0, 1)[:, None]
    fx = np.clip(xs - x0, 0, 1)[None, :]
    top = img[y0][:, x0] * (1 - fx) + img[y0][:, x1] * fx
    bot = img[y1][:, x0] * (1 - fx) + img[y1][:, x1] * fx
    return top * (1 - fy) + bot * fy


def crop_blob(alpha):
    ys, xs = np.where(alpha > 0.01)
    return alpha[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def edge_window(h, w, edge):
    """Janela 0..1 ELIPTICA com queda suave: o pedaco recortado nao deixa
    degrau de alpha nem borda reta onde o corte atravessou a nuvem. Em 'edge'
    = 0.45 o miolo (1 - edge do raio) fica intacto e o resto cai em cosseno."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot((yy - (h - 1) / 2.0) / (h / 2.0), (xx - (w - 1) / 2.0) / (w / 2.0))
    t = np.clip((r - (1.0 - edge)) / edge, 0.0, 1.0)
    return 0.5 + 0.5 * np.cos(np.pi * t)


def rotate_bilinear(img, angle):
    """Gira 'img' por 'angle' (rad) numa tela maior (cabe o giro), fundo 0."""
    h, w = img.shape
    n = int(np.ceil(np.hypot(h, w)))
    out = np.zeros((n, n), dtype=np.float32)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    yy -= (n - 1) / 2.0
    xx -= (n - 1) / 2.0
    c, s_ = np.cos(angle), np.sin(angle)
    sy = c * yy + s_ * xx + (h - 1) / 2.0
    sx = -s_ * yy + c * xx + (w - 1) / 2.0
    inside = (sy >= 0) & (sy <= h - 1) & (sx >= 0) & (sx <= w - 1)
    y0 = np.clip(np.floor(sy).astype(int), 0, h - 2)
    x0 = np.clip(np.floor(sx).astype(int), 0, w - 2)
    fy, fx = sy - y0, sx - x0
    v = (img[y0, x0] * (1 - fy) * (1 - fx) + img[y0, x0 + 1] * (1 - fy) * fx
         + img[y0 + 1, x0] * fy * (1 - fx) + img[y0 + 1, x0 + 1] * fy * fx)
    out[inside] = v[inside]
    return out


def paste_wrap(canvas, tile, cy, cx):
    """Combina 'tile' no canvas em 'screen', centrado em (cy, cx), com wrap."""
    th, tw = tile.shape
    H, W = canvas.shape
    yy = (np.arange(th) + cy - th // 2) % H
    xx = (np.arange(tw) + cx - tw // 2) % W
    region = canvas[np.ix_(yy, xx)]
    canvas[np.ix_(yy, xx)] = 1.0 - (1.0 - region) * (1.0 - tile)


def box_mean(a, k):
    """Media em janela k x k com wrap (tileavel)."""
    out = a.copy()
    acc = np.zeros_like(a)
    for s in range(-(k // 2), k // 2 + 1):
        acc += np.roll(a, s, axis=0)
    acc /= k
    out = np.zeros_like(a)
    for s in range(-(k // 2), k // 2 + 1):
        out += np.roll(acc, s, axis=1)
    return out / k


def metrics(alpha):
    """Mean e quanto do ladrilho e 'ceu vazio' vendo por uma janela pequena.

    A janela de 48 px imita o que se ve olhando pra cima (o UV amostrado perto
    do zenite cobre poucas dezenas de texels). 'vazio' = media local < 0.02."""
    local = box_mean(alpha, 48)
    return {
        "alpha medio": float(alpha.mean()),
        "pixels alpha>0.5 (%)": float((alpha > 0.5).mean() * 100),
        "area vazia (janela 48px < 0.02) (%)": float((local < 0.02).mean() * 100),
        "menor media local (janela 48px)": float(local.min()),
    }


def build(src_alpha, w, h):
    rng = np.random.default_rng(SEED)
    blob = crop_blob(src_alpha)
    canvas = np.zeros((h, w), dtype=np.float32)
    cell_h, cell_w = h / GRID, w / GRID

    cells = [(gy, gx) for gy in range(GRID) for gx in range(GRID)]
    rng.shuffle(cells)  # ordem aleatoria: quem sobrepoe quem nao segue a grade
    for gy, gx in cells:
        # pedaco aleatorio do aglomerado, com borda suave
        ph = int(blob.shape[0] * rng.uniform(PIECE_MIN, PIECE_MAX))
        pw = int(blob.shape[1] * rng.uniform(PIECE_MIN, PIECE_MAX))
        py = int(rng.integers(0, blob.shape[0] - ph + 1))
        px = int(rng.integers(0, blob.shape[1] - pw + 1))
        t = blob[py:py + ph, px:px + pw] * edge_window(ph, pw, EDGE)
        if rng.random() < 0.5:
            t = t[:, ::-1]
        t = rotate_bilinear(np.ascontiguousarray(t, dtype=np.float32),
                            rng.uniform(0, 2 * np.pi))
        s = rng.uniform(SCALE_MIN, SCALE_MAX)
        th = max(8, int(round(t.shape[0] * s)))
        tw = max(8, int(round(t.shape[1] * s)))
        t = resize_bilinear(np.ascontiguousarray(t), th, tw)
        cy = int((gy + 0.5) * cell_h + rng.uniform(-JITTER, JITTER) * cell_h)
        cx = int((gx + 0.5) * cell_w + rng.uniform(-JITTER, JITTER) * cell_w)
        paste_wrap(canvas, t, cy, cx)
    return canvas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="grava um PNG 2x2 ladrilhos (precisa de Pillow)")
    ap.add_argument("--dry-run", action="store_true", help="nao grava clouds.tga")
    args = ap.parse_args()

    src, w, h, header = read_tga_alpha(SRC)
    print("ORIGINAL"); [print("  %-42s %.4f" % kv) for kv in metrics(src).items()]
    out = build(src, w, h)
    print("NOVA");     [print("  %-42s %.4f" % kv) for kv in metrics(out).items()]

    if args.preview:
        from PIL import Image
        big = np.tile(out, (2, 2))
        Image.fromarray((np.clip(big, 0, 1) * 255).astype(np.uint8)).save(args.preview)
        print("preview:", args.preview)
    if not args.dry_run:
        write_tga(OUT, header, out)
        print("gravado:", OUT)


if __name__ == "__main__":
    main()
