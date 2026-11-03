#!/usr/bin/env python3
"""
LINBLUX Icon Pipeline — Lucide SVG → PNG 96x96 (branco, fundo transparente)
Atribui nomes internos LINBLUX → ícones Lucide
Licença Lucide: ISC  https://lucide.dev/license
"""
import subprocess
import urllib.request
import os
import sys
import tempfile
from pathlib import Path

LUCIDE_BASE = "https://unpkg.com/lucide-static@latest/icons/"

# Mapeamento: nome-interno-linblux → nome-lucide
ICON_MAP = {
    # Ferramentas de transformação
    "select":           "mouse-pointer-2",
    "move":             "move",
    "scale":            "scaling",
    "rotate":           "rotate-ccw",
    # Inserção de objetos
    "part":             "box",
    "sphere":           "circle",
    "spawn":            "map-pin",
    # Aparência / física
    "color":            "palette",
    "material":         "layers",
    "anchor":           "anchor",
    # Projeto
    "settings":         "settings",
    "audio":            "volume-2",
    "image":            "image",
    # Organização
    "folder":           "folder",
    "model":            "boxes",
    "duplicate":        "copy-plus",
    "copy":             "copy",
    "paste":            "clipboard-paste",
    "delete":           "trash-2",
    # Editor
    "script":           "code",
    "local-script":     "code-2",
    "module":           "package",
    "output":           "terminal",
    "rig":              "person-standing",
    "animation":        "film",
    # Execução
    "play":             "play",
    "stop":             "square",
    "save":             "save",
    # Painéis
    "explorer":         "layout-list",
    "properties":       "sliders-horizontal",
    "toolbox":          "wrench",
    # Viewport
    "focus":            "crosshair",
    "workspace":        "globe",
    # Player HUD
    "player-menu":      "menu",
    "player-close":     "x",
    "player-person":    "user",
    "player-settings":  "settings-2",
    "player-left":      "chevron-left",
    "player-right":     "chevron-right",
    "player-heart":     "heart",
    "player-controls":  "gamepad-2",
    "player-leave":     "log-out",
    # Navegação
    "chevron-right":    "chevron-right",
    "chevron-down":     "chevron-down",
    # Miscelânea
    "respawn":          "refresh-cw",
    "info":             "info",
    "home":             "house",
    "window":           "app-window",
    "lighting":         "sun",
    "item":             "package-open",
    "service":          "server",
    "music":            "music",
    "menu":             "menu",
    "heart":            "heart",
    # Studio header
    "open":             "folder-open",
}

def svg_to_white(svg_text: str) -> str:
    """Substitui currentColor por white e define stroke-width=1.5 se necessário."""
    svg_text = svg_text.replace('stroke="currentColor"', 'stroke="white"')
    svg_text = svg_text.replace("stroke='currentColor'", "stroke='white'")
    svg_text = svg_text.replace('fill="currentColor"', 'fill="white"')
    svg_text = svg_text.replace("fill='currentColor'", "fill='white'")
    # Garantir stroke-width mínimo para visibilidade
    if 'stroke-width' not in svg_text:
        svg_text = svg_text.replace('<svg ', '<svg stroke-width="1.5" ', 1)
    return svg_text

def download_svg(lucide_name: str) -> str | None:
    url = LUCIDE_BASE + lucide_name + ".svg"
    try:
        req = urllib.request.urlopen(url, timeout=10)
        return req.read().decode()
    except Exception as e:
        print(f"  ERRO ao baixar {url}: {e}", file=sys.stderr)
        return None

def render_png(svg_text: str, output_path: Path, size: int = 96) -> bool:
    with tempfile.NamedTemporaryFile(suffix=".svg", mode="w", delete=False) as f:
        f.write(svg_text)
        tmp = f.name
    try:
        result = subprocess.run(
            ["rsvg-convert", "-w", str(size), "-h", str(size),
             "--background-color", "none", "-o", str(output_path), tmp],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"  rsvg-convert erro: {result.stderr}", file=sys.stderr)
            return False
        return True
    finally:
        os.unlink(tmp)

def main():
    out_dir = Path(__file__).resolve().parent.parent.parent / "assets" / "ui" / "icons"
    out_dir.mkdir(parents=True, exist_ok=True)
    license_path = out_dir / "LICENSE-lucide.txt"
    if not license_path.exists():
        license_path.write_text(
            "Lucide Icons — ISC License\n"
            "Copyright (c) for portions of Lucide are held by Cole Bemis 2013-2022\n"
            "as part of Feather (MIT). All other copyright (c) for Lucide are held\n"
            "by Lucide Contributors 2022.\n\n"
            "Permission to use, copy, modify, and/or distribute this software for\n"
            "any purpose with or without fee is hereby granted, provided that the\n"
            "above copyright notice and this permission notice appear in all copies.\n\n"
            "THE SOFTWARE IS PROVIDED 'AS IS' AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
            "WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
            "MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"
            "ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
            "WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
            "ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"
            "OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
        )

    ok = 0
    fail = 0
    for linblux_name, lucide_name in ICON_MAP.items():
        out_file = out_dir / f"icon-{linblux_name}.png"
        # Não re-baixar se já existe e tem tamanho razoável
        if out_file.exists() and out_file.stat().st_size > 100:
            print(f"  [SKIP] {linblux_name}")
            ok += 1
            continue
        print(f"  {linblux_name} ← lucide/{lucide_name}")
        svg = download_svg(lucide_name)
        if not svg:
            fail += 1
            continue
        svg = svg_to_white(svg)
        if render_png(svg, out_file):
            ok += 1
        else:
            fail += 1

    print(f"\nConcluído: {ok} ícones gerados, {fail} falhas")
    print(f"Diretório: {out_dir}")
    return 0 if fail == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
