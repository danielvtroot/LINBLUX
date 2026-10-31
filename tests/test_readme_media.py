"""Regression checks for the metadata-only public media gallery generator."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('media', Path(__file__).resolve().parents[1] / 'tools/update_readme_media.py')
media = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(media)


class MediaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('docs/prints', 'docs/videos'):
            (self.root / name).mkdir(parents=True)
        self.doc = self.root / 'docs/MIDIA.md'
        self.doc.write_text('Manual\n' + media.START + '\nantigo\n' + media.END + '\nFinal\n')

    def test_idempotent_and_preserves_surrounding_text(self):
        media.update(self.root)
        before = self.doc.read_bytes()
        media.update(self.root)
        self.assertEqual(before, self.doc.read_bytes())
        self.assertTrue(before.startswith(b'Manual\n'))
        self.assertTrue(before.endswith(b'\nFinal\n'))

    def test_names_encoded_and_video_cover(self):
        (self.root / 'docs/prints/A & B.png').write_bytes(b'image')
        (self.root / 'docs/videos/A & B.mp4').write_bytes(b'video')
        media.update(self.root)
        text = self.doc.read_text()
        self.assertIn('videos/A%20%26%20B.mp4', text)
        self.assertIn('prints/A%20%26%20B.png', text)
        self.assertIn('A &amp; B', text)
        self.assertEqual(text.count('Assistir: A &amp; B</a>'), 1)

    def test_hidden_temporary_empty_large_and_external_symlink_ignored(self):
        for name in ('.private.png', 'shot.tmp.png', 'empty.png'):
            (self.root / 'docs/prints' / name).touch()
        (self.root / 'docs/prints/not-image.txt').write_text('test')
        external = self.root / 'external.png'
        external.write_bytes(b'private')
        (self.root / 'docs/prints/link.png').symlink_to(external)
        with (self.root / 'docs/videos/large.mp4').open('wb') as stream:
            stream.truncate(media.VIDEO_LIMIT + 1)
        media.update(self.root)
        text = self.doc.read_text()
        for name in ('private', 'shot.tmp', 'empty.png', 'not-image', 'link.png', 'large.mp4'):
            self.assertNotIn(name, text)

    def test_reviewed_titles_and_pending_video_note(self):
        (self.root / 'docs/prints/studio-construcao.png').write_bytes(b'image')
        with (self.root / 'docs/videos/linblux.mp4').open('wb') as stream:
            stream.truncate(media.VIDEO_LIMIT + 1)
        media.update(self.root)
        text = self.doc.read_text()
        self.assertIn('alt="LINBLUX Studio — Construção 3D"', text)
        self.assertIn(media.VIDEO_PENDING, text)
        self.assertNotIn('Nenhum vídeo público adicionado', text)
        self.assertNotIn('linblux.mp4', text)

    def test_unknown_stem_falls_back_to_readable_name(self):
        (self.root / 'docs/prints/nova_captura.png').write_bytes(b'image')
        media.update(self.root)
        self.assertIn('<p>nova captura</p>', self.doc.read_text())

    def test_reject_invalid_markers_without_writing(self):
        self.doc.write_text('Manual sem marcadores\n')
        with self.assertRaises(ValueError):
            media.update(self.root)
        self.assertEqual(self.doc.read_text(), 'Manual sem marcadores\n')


if __name__ == '__main__':
    unittest.main()
