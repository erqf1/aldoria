"""Erzeugt kachelbare Texturen für Gegner: Schuppenhaut, raue Haut, Leinengewebe, Knochen.

Alles wird prozedural berechnet (kein Download): periodisches Voronoi für Schuppen, Rauschen für Haut.
Die Farbtexturen sind neutral hell, weil der Spielcode sie mit der Farbe des Gegners einfärbt.
Aufruf: python scripts/gen_creature_textures.py   (schreibt nach data/textures)
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "textures")
N = 512
rng = np.random.default_rng(20260926)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def periodic_noise(size, cells, octaves=4, persistence=0.5):
    """Kachelbares Wertrauschen (mehrere Oktaven)."""
    total = np.zeros((size, size))
    amp = 1.0
    norm = 0.0
    for o in range(octaves):
        c = cells * (2 ** o)
        grid = rng.random((c, c))
        # bikubisch-glatt interpolieren
        xs = np.linspace(0, c, size, endpoint=False)
        x0 = np.floor(xs).astype(int)
        f = xs - x0
        f = f * f * (3 - 2 * f)
        x1 = (x0 + 1) % c
        row = grid[x0][:, x0] * (1 - f)[None, :] * (1 - f)[:, None] + grid[x0][:, x1] * f[None, :] * (1 - f)[:, None]
        row2 = grid[x1][:, x0] * (1 - f)[None, :] * f[:, None] + grid[x1][:, x1] * f[None, :] * f[:, None]
        total += (row + row2) * amp
        norm += amp
        amp *= persistence
    return total / norm


def voronoi(size, cells, jitter=0.85):
    """Periodisches Voronoi: Abstand zum nächsten und zweitnächsten Punkt und Nummer der Zelle."""
    pts = rng.random((cells, cells, 2))
    ys, xs = np.mgrid[0:size, 0:size]
    u = xs * cells / size
    v = ys * cells / size
    ci = np.floor(u).astype(int)
    cj = np.floor(v).astype(int)
    d1 = np.full((size, size), 9.0)
    d2 = np.full((size, size), 9.0)
    idx = np.zeros((size, size), dtype=int)
    for dj in range(-2, 3):
        for di in range(-2, 3):
            gi = (ci + di) % cells
            gj = (cj + dj) % cells
            px = ci + di + 0.5 + (pts[gj, gi, 0] - 0.5) * jitter
            py = cj + dj + 0.5 + (pts[gj, gi, 1] - 0.5) * jitter
            d = np.sqrt((u - px) ** 2 + (v - py) ** 2)
            closer = d < d1
            d2 = np.where(closer, d1, np.minimum(d2, d))
            idx = np.where(closer, gj * cells + gi, idx)
            d1 = np.where(closer, d, d1)
    return d1, d2, idx


def normal_from_height(h, strength):
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5   # entlang der Bildzeilen (nach unten)
    nx = -dx * strength
    ny = dy * strength
    nz = np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / ln, ny / ln, nz / ln], axis=-1)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def save(name, color, height, strength):
    c = np.clip(color, 0, 1)
    if c.ndim == 2:
        c = np.stack([c, c, c], axis=-1)
    Image.fromarray((c * 255).astype(np.uint8)).save(os.path.join(OUT, name + "_color.jpg"), quality=90)
    Image.fromarray(normal_from_height(height, strength)).save(os.path.join(OUT, name + "_normal.jpg"), quality=90)
    print("geschrieben:", name)


# ---- Schuppenhaut: gewölbte Schuppen mit dunklen Fugen, jede etwas anders getönt
d1, d2, idx = voronoi(N, 16)
cell_shade = rng.random(16 * 16)[idx]
edge = smoothstep(0.02, 0.16, d2 - d1)
dome = 1.0 - np.clip(d1 / 0.62, 0, 1) ** 1.6
fine = periodic_noise(N, 24, 3)
shade = edge * (0.42 + 0.46 * dome) * (0.80 + 0.30 * cell_shade) + 0.08 * fine
shade = 0.10 + 0.90 * shade
height = edge * (0.35 + 0.65 * dome) + 0.05 * fine
save("scaly_hide", 0.06 + shade * 0.62, height * 3.0, 5.0)

# ---- Raue Haut: Falten und Poren (Brecher)
low = periodic_noise(N, 5, 5, 0.55)
ridge = 1.0 - np.abs(2.0 * periodic_noise(N, 9, 3, 0.5) - 1.0)
pores = periodic_noise(N, 64, 2, 0.5)
h = 0.55 * low + 0.35 * ridge ** 3 + 0.10 * pores
col = 0.42 + 0.55 * h + 0.10 * (pores - 0.5)
save("rough_skin", col * 0.82, h * 2.5, 4.0)

# ---- Leinengewebe: sich kreuzende Fäden, jeder leicht unterschiedlich dick und hell
threads = 56
ys, xs = np.mgrid[0:N, 0:N]
tx = (xs * threads / N)
ty = (ys * threads / N)
fx = np.abs(np.sin(tx * np.pi)) ** 0.6
fy = np.abs(np.sin(ty * np.pi)) ** 0.6
over = ((np.floor(tx) + np.floor(ty)) % 2) == 0   # welcher Faden liegt oben
thick_x = 0.85 + 0.30 * rng.random(threads)[np.floor(tx).astype(int) % threads]
thick_y = 0.85 + 0.30 * rng.random(threads)[np.floor(ty).astype(int) % threads]
hx = fx * thick_x
hy = fy * thick_y
h = np.where(over, hx + 0.25 * hy, hy + 0.25 * hx)
slub = periodic_noise(N, 12, 3)
col = (0.50 + 0.36 * h) * (0.90 + 0.20 * slub)
save("weave_linen", col * 0.78, h * 1.4, 3.0)

# ---- Knochen: helles Elfenbein mit feinen Längsrissen
low = periodic_noise(N, 6, 4)
streak = periodic_noise(N, 3, 2)
cracks = np.abs(np.sin((ys / N * 9.0 + 2.5 * low) * np.pi)) ** 8
col = 0.72 + 0.20 * low - 0.16 * cracks + 0.05 * streak
save("bone_ivory", col * 0.92, low * 0.6 - cracks * 0.5, 2.5)
