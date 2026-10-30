#!/usr/bin/env python3
"""Update only the media block in docs/MIDIA.md, using file metadata (stdlib)."""
from pathlib import Path
from urllib.parse import quote
import html

ROOT = Path(__file__).resolve().parents[1]
START, END = '<!-- LINBLUX:MEDIA:START -->', '<!-- LINBLUX:MEDIA:END -->'
IMAGE_LIMIT, VIDEO_LIMIT = 8 * 1024**2, 20 * 1024**2


def discover(root, directory, extensions, limit):
    base = root / directory
    if base.is_symlink() or not base.is_dir():
        return []
    result = []
    for path in sorted(base.iterdir(), key=lambda p: (p.name.casefold(), p.name)):
        name = path.name
        if path.is_symlink() or not path.is_file():
            continue
        if name.startswith('.') or name.endswith(('~', '.tmp', '.part')) or '.tmp.' in name:
            continue
        if path.suffix.lower() not in extensions:
            continue
        size = path.stat().st_size
        if not 0 < size <= limit:
            print(f'Omitido por tamanho: {directory}/{name} ({size} bytes)')
            continue
        result.append(path)
    return result


def update(root=ROOT):
    target = root / 'docs/MIDIA.md'
    original = target.read_text(encoding='utf-8')
    if original.count(START) != 1 or original.count(END) != 1:
        raise ValueError('MIDIA.md deve conter exatamente um par de marcadores de mídia.')
    before, rest = original.split(START)
    _, after = rest.split(END)
    images = discover(root, 'docs/prints', {'.png', '.jpg', '.jpeg', '.webp'}, IMAGE_LIMIT)
    videos = discover(root, 'docs/videos', {'.mp4', '.webm'}, VIDEO_LIMIT)
    lines = ['\n', '### Capturas', '']
    for path in images:
        url = quote(path.relative_to(root / 'docs').as_posix(), safe='/')
        label = html.escape(path.stem.replace('_', ' '), quote=True)
        lines += [f'<a href="{url}"><img src="{url}" width="480" alt="{label}"/></a>', f'<p>{label}</p>', '']
    if not images:
        lines += ['Ainda não há capturas públicas em `docs/prints/`.', '']
    lines += ['### Vídeos', '']
    for path in videos:
        url = quote(path.relative_to(root / 'docs').as_posix(), safe='/')
        label = html.escape(path.stem.replace('_', ' '), quote=True)
        cover = next((p for p in images if p.stem == path.stem), None)
        if cover:
            cover_url = quote(cover.relative_to(root / 'docs').as_posix(), safe='/')
            lines += [f'<a href="{url}"><img src="{cover_url}" width="480" alt="Assistir: {label}"/></a>']
        lines += [f'<p><a href="{url}">Assistir: {label}</a> ({path.stat().st_size / 1024**2:.1f} MiB)</p>', '']
    if not videos:
        lines += ['Nenhum vídeo público adicionado. As demonstrações disponíveis são as capturas acima.', '']
    generated = before + START + '\n'.join(lines) + '\n' + END + after
    if generated != original:
        target.write_text(generated, encoding='utf-8')
    print(f'Galeria: {len(images)} imagem(ns), {len(videos)} vídeo(s).')


if __name__ == '__main__':
    update()
