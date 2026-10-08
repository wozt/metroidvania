from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from scripts.verify_roms import validate


class VerifyRomsTests(unittest.TestCase):
    def test_missing_file_is_rejected(self) -> None:
        valid, message = validate("aria", Path("/fichier/inexistant.gba"))
        self.assertFalse(valid)
        self.assertIn("absent", message)

    def test_unknown_file_is_rejected_with_hash(self) -> None:
        with TemporaryDirectory() as directory:
            path = Path(directory) / "fake.gba"
            path.write_bytes(b"ceci n'est pas une ROM")
            valid, message = validate("metroid", path)
            self.assertFalse(valid)
            self.assertIn("SHA-1 inconnu", message)


if __name__ == "__main__":
    unittest.main()
