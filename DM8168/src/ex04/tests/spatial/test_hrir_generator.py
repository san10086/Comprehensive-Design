"""Import validation, deterministic generation, and source/array equivalence."""
import importlib.util
from pathlib import Path
import re
import shutil
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("generate_hrir", ROOT / "tools/generate_hrir.py")
GEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN)


class GeneratorTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="hrir-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source with spaces"
        self.source.mkdir()
        for name in ("L40e045a.wav", "R40e045a.wav", "L60e250a.wav", "R60e250a.wav"):
            shutil.copyfile(ROOT / "assets/hrir/original" / name, self.source / name)

    def fixture(self, name, rate=44100, channels=1, width=2, count=512):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(path), "wb") as wav:
            wav.setparams((channels, width, rate, count, "NONE", "not compressed"))
            wav.writeframes(b"\0" * width * channels * count)

    def test_complete_library_exact_arrays(self):
        pairs, ignored = GEN.read_library(GEN.DEFAULT_SOURCE)
        header = (ROOT / "shared/hrir_catalog.h").read_text()
        count = int(re.search(r"#define HRIR_DIRECTION_COUNT (\d+)", header).group(1))
        self.assertEqual(len(pairs), count)
        self.assertTrue(all(GEN.NAME.fullmatch(Path(name).name) is None for name in ignored))
        text = (ROOT / "dsp/dsp_spatial_coefficients.inc").read_text()
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        body = text.split("=", 1)[1]
        actual = tuple(map(int, re.findall(r"-?\d+", body)))
        expected = tuple(value for pair in pairs.values() for ear in ("L", "R")
                         for value in pair[ear][0])
        self.assertEqual(actual, expected)
        GEN.generate(GEN.DEFAULT_SOURCE, ROOT, check=True)

    def test_negative_direction_and_determinism(self):
        self.fixture("elev-40/L-40e045a.wav")
        self.fixture("elev-40/R-40e045a.wav")
        pairs = GEN.generate(self.source, self.root)
        self.assertIn((-40, 45), pairs)
        before = {name: (self.root / name).read_bytes() for name in GEN.render(pairs)}
        GEN.generate(self.source, self.root)
        self.assertEqual(before, {name: (self.root / name).read_bytes() for name in before})
        GEN.generate(self.source, self.root, check=True)
        header = self.root / "shared/hrir_catalog.h"
        header.write_bytes(b"stale")
        with self.assertRaisesRegex(ValueError, "stale"):
            GEN.generate(self.source, self.root, check=True)

    def test_invalid_import_preserves_generated_files(self):
        pairs = GEN.generate(self.source, self.root)
        before = {name: (self.root / name).read_bytes() for name in GEN.render(pairs)}
        cases = [dict(rate=48000), dict(channels=2), dict(width=3), dict(count=128)]
        for case in cases:
            with self.subTest(case=case):
                self.fixture("L0e000a.wav", **case)
                with self.assertRaisesRegex(ValueError, "mono PCM16"):
                    GEN.generate(self.source, self.root)
                self.assertEqual(before, {name: (self.root / name).read_bytes() for name in before})

    def test_missing_pair_duplicate_and_bad_angles(self):
        self.fixture("L0e000a.wav")
        with self.assertRaisesRegex(ValueError, "Missing left/right"):
            GEN.read_library(self.source)
        self.fixture("R0e000a.wav")
        self.fixture("duplicate/L0e000a.wav")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            GEN.read_library(self.source)
        (self.source / "duplicate/L0e000a.wav").unlink()
        self.fixture("L91e000a.wav")
        with self.assertRaisesRegex(ValueError, "Invalid angles"):
            GEN.read_library(self.source)

    def test_truncated_and_disguised_wav(self):
        path = self.source / "L0e000a.wav"
        path.write_bytes(b"not a RIFF WAV file")
        with self.assertRaisesRegex(ValueError, "Invalid HRIR WAV"):
            GEN.read_library(self.source)
        self.fixture("L0e000a.wav")
        path.write_bytes(path.read_bytes()[:-2])
        with self.assertRaisesRegex(ValueError, "Truncated"):
            GEN.read_library(self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
