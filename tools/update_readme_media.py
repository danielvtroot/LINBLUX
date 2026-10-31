#!/usr/bin/env python3
"""Update only the media block in docs/MIDIA.md, using file metadata (stdlib)."""
from pathlib import Path
from urllib.parse import quote
import html

ROOT = Path(__file__).resolve().parents[1]
START, END = '<!-- LINBLUX:MEDIA:START -->', '<!-- LINBLUX:MEDIA:END -->'
IMAGE_LIMIT, VIDEO_LIMIT = 8 * 1024**2, 20 * 1024**2
TITLES = {
    'studio-construcao': 'LINBLUX Studio — Construção 3D',
    'explorador-servicos': 'Explorador e serviços',
    'explorador-servicos-automaticos': 'Serviços de armazenamento organizados',
}
VIDEO_URL = 'https://www.youtube.com/watch?v=jVck4UuYlWQ'
VIDEO_PENDING = (
    f'O vídeo de demonstração está publicado no YouTube: [Assistir à demonstração LINBLUX]({VIDEO_URL}). '
    'O GitHub não reproduz MP4 do repositório dentro do README. O arquivo local excede '
    'o limite de 20 MiB da galeria e não entra no Git.'
)


def title(path):
    """Public caption for a file: reviewed title, or the readable stem."""
    return TITLES.get(path.stem, path.stem.replace('_', ' '))


class MediaFiles(list):
    """Accepted files, plus how many were skipped for exceeding the size limit."""

    omitted = 0


def discover(root, directory, extensions, limit):
    base = root / directory
    if base.is_symlink() or not base.is_dir():
        return MediaFiles()
    result = MediaFiles()
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
            result.append(None)
            continue
        result.append(path)
    omitted = result.count(None)
    files = MediaFiles(path for path in result if path is not None)
    files.omitted = omitted
    return files



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
        label = html.escape(title(path), quote=True)
        lines += [f'<a href="{url}"><img src="{url}" width="480" alt="{label}"/></a>', f'<p>{label}</p>', '']
    if not images:
        lines += ['Ainda não há capturas públicas em `docs/prints/`.', '']
    lines += ['### Vídeos', '']
    for path in videos:
        url = quote(path.relative_to(root / 'docs').as_posix(), safe='/')
        label = html.escape(title(path), quote=True)
        cover = next((p for p in images if p.stem == path.stem), None)
        if cover:
            cover_url = quote(cover.relative_to(root / 'docs').as_posix(), safe='/')
            lines += [f'<a href="{url}"><img src="{cover_url}" width="480" alt="Assistir: {label}"/></a>']
        lines += [f'<p><a href="{url}">Assistir: {label}</a> ({path.stat().st_size / 1024**2:.1f} MiB)</p>', '']
    if not videos:
        if videos.omitted:
            lines += [VIDEO_PENDING, '']
        else:
            lines += ['Nenhum vídeo público adicionado. As demonstrações disponíveis são as capturas acima.', '']
    generated = before + START + '\n'.join(lines) + '\n' + END + after
    if generated != original:
        target.write_text(generated, encoding='utf-8')
    print(f'Galeria: {len(images)} imagem(ns), {len(videos)} vídeo(s).')


if __name__ == '__main__':
    update()
