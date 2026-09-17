import csv
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class JarvisAudioContractTests(unittest.TestCase):
    def setUp(self):
        self.audio_header = (ROOT / "firmware/casa_inteligente_v4/domus_jarvis_audio.h").read_text(encoding="utf-8")
        self.transport = (ROOT / "firmware/casa_inteligente_v4/domus_dfplayer.h").read_text(encoding="utf-8")
        self.firmware = (ROOT / "firmware/casa_inteligente_v4/casa_inteligente_v4.ino").read_text(encoding="utf-8")

    def test_catalogo_tiene_14_carpetas_y_cuatro_variantes(self):
        valores = re.findall(r"\b[A-Z_]+(?:\s*=\s*1)?(?:,|\n)", self.audio_header.split("};", 1)[0])
        self.assertGreaterEqual(len(valores), 14)
        self.assertIn("esp_random() % 4U", self.audio_header)
        self.assertIn("pista == ultimaPista_", self.audio_header)

    def test_emergencia_interrumpe_y_alertas_tienen_cooldown(self):
        self.assertIn("ENFRIAMIENTO_ALERTA_MS = 30000UL", self.audio_header)
        self.assertIn("if (emergencia) transporte_.detener();", self.audio_header)

    def test_transporte_usa_play_folder_y_no_inventa_gpio(self):
        self.assertIn("enviar(0x0F, carpeta, pista)", self.transport)
        self.assertIn("begin(int8_t rx, int8_t tx, int8_t busy = -1)", self.transport)
        for gpio in range(0, 49):
            self.assertNotIn(f"GPIO{gpio}", self.transport)

    def test_banco_mantiene_audio_deshabilitado_y_pines_tbd(self):
        self.assertIn("#define MP3_RX_PIN      -1", self.firmware)
        self.assertIn("#define MP3_TX_PIN      -1", self.firmware)
        self.assertIn("#define MP3_BUSY_PIN    -1", self.firmware)
        self.assertIn("#define MP3_HABILITADO  false", self.firmware)

    def test_manifest_si_existe_tiene_112_mp3_unicos(self):
        manifest = ROOT / "audio/jarvis_sd/MANIFEST.csv"
        if not manifest.exists():
            self.skipTest("audio aún no generado")
        with manifest.open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 112)
        self.assertEqual(len({row["archivo"] for row in rows}), 112)
        self.assertEqual(
            {row["carpeta"] for row in rows},
            {f"{i:02d}" for i in range(1, 15)} | {f"{i:02d}" for i in range(51, 65)},
        )
        for row in rows:
            path = manifest.parent / row["archivo"]
            self.assertTrue(path.is_file())
            self.assertGreater(path.stat().st_size, 1000)
            self.assertTrue((ROOT / "audio" / row["biblioteca"]).is_file())

    def test_tecla_6_cambia_entre_dos_voces(self):
        self.assertIn("case N_6:", self.firmware)
        self.assertIn("jarvisAudio.cambiarVoz();", self.firmware)
        self.assertIn("voz_ == 2 ? 50U : 0U", self.audio_header)


if __name__ == "__main__":
    unittest.main()
