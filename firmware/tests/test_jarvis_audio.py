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

    def test_catalogo_tiene_28_eventos_y_cuatro_variantes(self):
        """Cada una de las 21 teclas (nota 46) tiene evento propio: 28 carpetas."""
        # Sensores y base original
        for evento in ("SISTEMA_LISTO", "EMERGENCIA", "RIEGO_INICIADO",
                       "AGUA_BAJA", "ERROR_SENSOR"):
            self.assertIn(evento, self.audio_header)
        # Una voz por tecla: modos, diagnostico, apagado general, sonido,
        # rearme, cultivo, ventilador y las cuatro consultas 6-9.
        for evento in ("MODO_MANUAL", "MODO_AUTO", "DIAGNOSTICO",
                       "TODO_APAGADO", "SONIDO_ACTIVADO", "SISTEMA_REARMADO",
                       "LUZ_CULTIVO_ENCENDIDA", "LUZ_CULTIVO_APAGADA",
                       "VENTILADOR_ENCENDIDO", "VENTILADOR_APAGADO",
                       "CONSULTA_TEMP", "CONSULTA_HUMEDAD", "CONSULTA_SUELO",
                       "ESTADO_COMPLETO"):
            self.assertIn(evento, self.audio_header)
        self.assertIn("NUM_EVENTOS = 28", self.audio_header)
        self.assertIn("carpetaPara", self.audio_header)
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

    def test_manifest_tiene_224_mp3_unicos(self):
        """MANIFEST.csv: 28 eventos x 4 pistas x 2 voces = 224 filas."""
        manifest = ROOT / "audio/jarvis_sd/MANIFEST.csv"
        self.assertTrue(manifest.exists(), "El manifest no existe aún")
        with manifest.open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 224, f"Expected 224 rows, got {len(rows)}")
        # 28 eventos distintos, 4 pistas por (voz, carpeta SD).
        self.assertEqual(len({r["evento"] for r in rows}), 28)
        por_voz_carpeta: dict[tuple[str, str], int] = {}
        for row in rows:
            por_voz_carpeta[(row["voz"], row["carpeta"])] = \
                por_voz_carpeta.get((row["voz"], row["carpeta"]), 0) + 1
        self.assertEqual(len(por_voz_carpeta), 56)
        for clave, total in por_voz_carpeta.items():
            self.assertEqual(total, 4, f"{clave} debe tener 4 pistas")
        # Voz 1 (Carlos): SD 01-28; voz 2 (Karla): SD 51-78.
        carlos = {r["carpeta"] for r in rows if r["voz"] == "1"}
        karla = {r["carpeta"] for r in rows if r["voz"] == "2"}
        self.assertEqual(len([r for r in rows if r["voz"] == "1"]), 112)
        self.assertEqual(len([r for r in rows if r["voz"] == "2"]), 112)
        self.assertEqual(carlos, {f"{i:02d}" for i in range(1, 29)})
        self.assertEqual(karla, {f"{i:02d}" for i in range(51, 79)})
        for row in rows:
            self.assertTrue(row["biblioteca"].startswith(
                "Carlos/" if row["voz"] == "1" else "Karla/"))
            for ruta in (AUDIO_DIR / row["biblioteca"],
                         ROOT / "audio/jarvis_sd" / row["archivo"]):
                self.assertTrue(ruta.is_file(), f"Archivo no encontrado: {ruta}")
                self.assertGreater(ruta.stat().st_size, 1000,
                                   f"Archivo demasiado pequeño: {ruta}")

    def test_doble_pulsacion_6_cambia_entre_dos_voces(self):
        """Una pulsación del 6 consulta temperatura; doble (<2 s) cambia de voz."""
        self.assertIn("case N_6:", self.firmware)
        self.assertIn("jarvisAudio.cambiarVoz();", self.firmware)
        self.assertIn("CONSULTA_TEMP", self.firmware)
        # La voz 2 usa las carpetas 51-78 (desplazamiento +50 sobre 01-28)
        self.assertIn("DESPLAZAMIENTO_VOZ_2 = 50", self.audio_header)
        self.assertIn("+ DESPLAZAMIENTO_VOZ_2", self.audio_header)

    def test_mapa_21_teclas_con_voz_propia(self):
        """Cada tecla termina en su carpeta dedicada (nota 46).

        Las teclas de menú hablan en ejecutarTeclaIRCasa; las de carga
        (sala, cultivo, ventilador, riego) hablan vía el despachador
        (ejecutarOrdenActuador/solicitar/desactivarSalida).
        """
        teclas = self.firmware.split("void ejecutarTeclaIRCasa", 1)[1].split(
            "\n}\n", 1)[0]
        for evento in ("MODO_MANUAL", "ESTADO_COMPLETO", "TODO_APAGADO",
                       "SISTEMA_REARMADO", "DIAGNOSTICO", "SONIDO_ACTIVADO",
                       "CONSULTA_TEMP", "CONSULTA_HUMEDAD", "CONSULTA_SUELO"):
            self.assertIn(evento, teclas, f"Tecla sin voz propia: {evento}")
        despacho = self.firmware.split(
            "ResultadoOrden ejecutarOrdenActuador", 1)[1].split(
            "\n}\n", 1)[0]
        for evento in ("RIEGO_INICIADO", "RIEGO_DETENIDO",
                       "LUZ_CULTIVO_ENCENDIDA", "LUZ_CULTIVO_APAGADA",
                       "VENTILADOR_ENCENDIDO", "VENTILADOR_APAGADO"):
            self.assertIn(evento, despacho + self.firmware.split(
                "bool solicitarSalida", 1)[1].split(
                "bool desactivarSalida", 1)[0],
                f"Carga sin voz propia: {evento}")

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