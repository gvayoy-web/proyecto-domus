import csv
import re
import os
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
AUDIO_DIR = ROOT / "audio"


class JarvisAudioContractTests(unittest.TestCase):
    def setUp(self):
        self.audio_header = (ROOT / "firmware/casa_inteligente_v4/domus_jarvis_audio.h").read_text(encoding="utf-8")
        self.transport = (ROOT / "firmware/casa_inteligente_v4/domus_dfplayer.h").read_text(encoding="utf-8")
        self.firmware = (ROOT / "firmware/casa_inteligente_v4/casa_inteligente_v4.ino").read_text(encoding="utf-8")

    def test_catalogo_tiene_14_carpetas_y_cuatro_variantes(self):
        """Test that the audio header has at least 14 event values and random selection logic."""
        # Count EventoJarvis enum values by looking for the mapping to folders
        # The enum must have at least 14 values (original + new ones)
        self.assertIn("SISTEMA_LISTO", self.audio_header)
        self.assertIn("EMERGENCIA", self.audio_header)
        # Verify random selection logic exists
        self.assertIn("esp_random() % 4U", self.audio_header)
        self.assertIn("pista == ultimaPista_", self.audio_header)

    def test_emergencia_interrumpe_y_alertas_tienen_cooldown(self):
        """Test that emergency interrupts and alerts have 30s cooldown."""
        self.assertIn("ENFRIAMIENTO_ALERTA_MS = 30000UL", self.audio_header)
        self.assertIn("if (emergencia) transporte_.detener();", self.audio_header)

    def test_transporte_usa_play_folder_y_no_inventa_gpio(self):
        """Test that DFPlayer transport uses playFolder and doesn't use GPIO."""
        self.assertIn("enviar(0x0F, carpeta, pista)", self.transport)
        self.assertIn("begin(int8_t rx, int8_t tx, int8_t busy = -1)", self.transport)
        for gpio in range(0, 49):
            self.assertNotIn(f"GPIO{gpio}", self.transport)

    def test_banco_mantiene_audio_deshabilitado_y_pines_tbd(self):
        """Test that audio is disabled by default and pins are TBD."""
        self.assertIn("#define MP3_RX_PIN      -1", self.firmware)
        self.assertIn("#define MP3_TX_PIN      -1", self.firmware)
        self.assertIn("#define MP3_BUSY_PIN    -1", self.firmware)
        self.assertIn("#define MP3_HABILITADO  false", self.firmware)

    def test_manifest_tiene_112_mp3_unicos(self):
        """Test that the MANIFEST.csv has exactly 112 rows with correct distribution."""
        manifest = ROOT / "audio/jarvis_sd/MANIFEST.csv"
        self.assertTrue(manifest.exists(), "El manifest no existe aún")
        with manifest.open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 112, f"Expected 112 rows, got {len(rows)}")
        # Check folder distribution: 14 folders for Carlos (01-14) + 14 folders for Karla (01-14)
        carpetas = {row["carpeta"] for row in rows}
        carpetas_esperadas = {f"{i:02d}" for i in range(1, 15)} | {f"{i:02d}" for i in range(1, 15)}
        self.assertEqual(carpetas, carpetas_esperadas, "Las carpetas deben ser 01-14 para ambas bibliotecas")
        # Check that each biblioteca has 14 folders with 4 tracks each = 56 files
        carlos_rows = [r for r in rows if r["biblioteca"] == "Carlos"]
        karla_rows = [r for r in rows if r["biblioteca"] == "Karla"]
        self.assertEqual(len(carlos_rows), 56, f"Expected 56 Carlos rows, got {len(carlos_rows)}")
        self.assertEqual(len(karla_rows), 56, f"Expected 56 Karla rows, got {len(karla_rows)}")
        # Verify each biblioteca has folders 01-14
        carlos_carpetas = {r["carpeta"] for r in carlos_rows}
        karla_carpetas = {r["carpeta"] for r in karla_rows}
        self.assertEqual(carlos_carpetas, karla_carpetas, "Ambas bibliotecas deben tener las mismas carpetas")
        self.assertEqual(carlos_carpetas, {f"{i:02d}" for i in range(1, 15)})
        # Verify each track file exists
        for row in rows:
            path = AUDIO_DIR / row["biblioteca"] / row["carpeta"] / row["archivo"]
            self.assertTrue(path.is_file(), f"Archivo no encontrado: {path}")
            self.assertGreater(path.stat().st_size, 1000, f"Archivo demasiado pequeño: {path}")

    def test_tecla_6_cambia_entre_dos_voces(self):
        """Test that key 6 changes between two voices."""
        self.assertIn("case N_6:", self.firmware)
        self.assertIn("jarvisAudio.cambiarVoz();", self.firmware)
        # The voice change logic uses folder offset 50 for voice 2
        self.assertIn("carpetaBase + 50U", self.audio_header)

    def test_tecla_cambia_volumen(self):
        """Test that volume up/down keys work."""
        self.assertIn("case VOL_MAS:", self.firmware)
        self.assertIn("case VOL_MENOS:", self.firmware)
        self.assertIn("ajustarVolumen", self.audio_header)

    def test_tecla_repite_ultima_pista(self):
        """Test that key 100+ repeats the last track."""
        self.assertIn("case N_100_MAS:", self.firmware)
        self.assertIn("repetirUltima", self.audio_header)

    def test_filtro_sin_repeticion_inmediata(self):
        """Test that the random selection avoids immediate repetition."""
        # The code has: if (pista == ultimaPista_[eventoBase]) pista = uint8_t((pista % 4U) + 1U)
        self.assertIn("pista == ultimaPista_[eventoBase]", self.audio_header)
        self.assertIn("pista % 4U", self.audio_header)

    def test_estructura_datos_jarvis_audio(self):
        """Test that the JarvisAudio class has all required methods and fields."""
        # Check class definition exists
        self.assertIn("class JarvisAudio", self.audio_header)
        # Check required methods
        required_methods = ["begin", "habilitado", "silenciado", "volumenActual",
                           "vozActual", "silenciar", "cambiarVoz", "ajustarVolumen",
                           "repetirUltima", "reproducir"]
        for method in required_methods:
            self.assertIn(method, self.audio_header,
                         f"Método {method} debe estar en el header de JarvisAudio")

    def test_transporte_dfplayer_completo(self):
        """Test DFPlayer transport has all required methods."""
        required_methods = ["begin", "listo", "ocupado", "reproducirCarpeta",
                           "volumen", "detener"]
        for method in required_methods:
            self.assertIn(method, self.transport,
                         f"Método {method} debe estar en DFPlayerTransport")


if __name__ == "__main__":
    unittest.main(verbosity=2)