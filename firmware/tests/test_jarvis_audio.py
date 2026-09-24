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

    def test_catalogo_tiene_21_botones_mas_fallo(self):
        """21 botones (nota 64) + FALLO; carpeta = botón, voz 2 = +50."""
        for evento in ("CH_MENOS", "CH =", "CH_MAS", "ANTERIOR", "PLAY",
                       "SIGUIENTE", "VOL_MENOS", "VOL_MAS", "EQ", "TECLA_0",
                       "TECLA_100", "TECLA_200", "TECLA_1", "TECLA_2",
                       "TECLA_3", "TECLA_4", "TECLA_5", "TECLA_6", "TECLA_7",
                       "TECLA_8", "TECLA_9", "FALLO"):
            self.assertIn(evento, self.audio_header)
        self.assertIn("NUM_EVENTOS = 22", self.audio_header)
        self.assertIn("carpetaPara", self.audio_header)
        self.assertIn("reproducirGrupo", self.audio_header)
        self.assertIn("reproducirEstado", self.audio_header)
        # Códigos físicos de la nota 64 documentados en el enum IR.
        ir = (ROOT / "firmware/casa_inteligente_v4/domus_ir_casa.h").read_text(
            encoding="utf-8")
        for codigo in ("0x45", "0x46", "0x47", "0x44", "0x43", "0x40",
                       "0x07", "0x15", "0x09", "0x16", "0x19", "0x0D",
                       "0x0C", "0x18", "0x5E", "0x08", "0x1C", "0x5A",
                       "0x42", "0x52", "0x4A"):
            self.assertIn(codigo, ir)
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

    def test_perfil4_habilita_audio_con_pines_remapeados(self):
        """Test that audio is enabled in profile 4 with pin remapping."""
        self.assertIn("CASA_FINAL_DRV8833_DFPLAYER", self.firmware)
        self.assertIn("PERFIL_CASA == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER", self.firmware)
        self.assertIn("transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo,", self.firmware)
        self.assertIn("MP3_BUSY_PIN", self.firmware)
        self.assertIn("AUDIO_CANDIDATO_HABILITADO", self.audio_header)

    def test_manifest_tiene_176_mp3_unicos(self):
        """MANIFEST.csv: 22 eventos x 4 pistas x 2 voces = 176 filas."""
        manifest = ROOT / "audio/jarvis_sd/MANIFEST.csv"
        self.assertTrue(manifest.exists(), "El manifest no existe aún")
        with manifest.open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 176, f"Expected 176 rows, got {len(rows)}")
        # 22 eventos distintos, 4 pistas por (voz, carpeta SD).
        self.assertEqual(len({r["evento"] for r in rows}), 22)
        por_voz_carpeta: dict[tuple[str, str], int] = {}
        for row in rows:
            por_voz_carpeta[(row["voz"], row["carpeta"])] = \
                por_voz_carpeta.get((row["voz"], row["carpeta"]), 0) + 1
        self.assertEqual(len(por_voz_carpeta), 44)
        for clave, total in por_voz_carpeta.items():
            self.assertEqual(total, 4, f"{clave} debe tener 4 pistas")
        # Voz 1 (Carlos): SD 01-22; voz 2 (Karla): SD 51-72.
        carlos = {r["carpeta"] for r in rows if r["voz"] == "1"}
        karla = {r["carpeta"] for r in rows if r["voz"] == "2"}
        self.assertEqual(len([r for r in rows if r["voz"] == "1"]), 88)
        self.assertEqual(len([r for r in rows if r["voz"] == "2"]), 88)
        self.assertEqual(carlos, {f"{i:02d}" for i in range(1, 23)})
        self.assertEqual(karla, {f"{i:02d}" for i in range(51, 73)})
        for row in rows:
            self.assertTrue(row["biblioteca"].startswith(
                "Carlos/" if row["voz"] == "1" else "Karla/"))
            for ruta in (AUDIO_DIR / row["biblioteca"],
                         ROOT / "audio/jarvis_sd" / row["archivo"]):
                self.assertTrue(ruta.is_file(), f"Archivo no encontrado: {ruta}")
                self.assertGreater(ruta.stat().st_size, 1000,
                                   f"Archivo demasiado pequeño: {ruta}")

    def test_tecla_100_alterna_modo_sin_audio(self):
        """100+ alterna MANUAL/AUTO (antes cambiaba la voz; sin DFPlayer)."""
        teclas = self.firmware.split("void ejecutarTeclaIRCasa", 1)[1].split(
            "\n}\n", 1)[0]
        modo = teclas.split("case N_100_MAS:", 1)[1]
        self.assertIn("fijarModoManualIR", modo)
        self.assertIn("fijarModoAutoIR", modo)
        self.assertNotIn("jarvisAudio.cambiarVoz", teclas)
        self.assertNotIn("ultimaN6Ms", self.firmware)
        # FALLO sigue en el despachador (respuesta a pedidos rechazados).
        despacho = self.firmware.split(
            "ResultadoOrden ejecutarOrdenActuador", 1)[1].split(
            "\n}\n", 1)[0]
        self.assertIn("EventoJarvis::FALLO", despacho)
        self.assertIn('responderJarvis("No funciono.")', despacho)

    def test_puerta_ordenada_rechaza_senales_mientras_habla(self):
        """Mientras Jarvis habla o 1.5 s tras la orden, IR responde OCUPADO."""
        self.assertIn("BLOQUEO_IR_TRAS_ORDEN_MS 1500", self.firmware)
        self.assertIn("irPuertaOcupada", self.firmware)
        self.assertIn("NACK;IR;OCUPADO", self.firmware)
        self.assertIn("irBloqueadoHastaMs = millis() + BLOQUEO_IR_TRAS_ORDEN_MS",
                      self.firmware)
        # La saltan: aprendizaje, tecla 0 y (fuera del IR) PARO físico/Serial.
        puerta = self.firmware.split("bool irPuertaOcupada", 1)[1].split(
            "\n}\n", 1)[0]
        self.assertIn("N_0", puerta)
        self.assertIn("jarvisAudio.ocupado()", puerta)
        self.assertIn("bool ocupado() const", self.audio_header)

    def test_mapa_21_teclas_sin_voz_en_el_mando(self):
        """Mapa feria: sin audio en ejecutarTeclaIRCasa; modos y luces útiles.

        Menú/consultas solo Serie; las cargas siguen con carpetaSalida
        cuando exista DFPlayer (el despachador no cambia).
        """
        teclas = self.firmware.split("void ejecutarTeclaIRCasa", 1)[1].split(
            "\n}\n", 1)[0]
        # CH- = modo manual y CH+ = modo automático (nota 46).
        self.assertIn("fijarModoManualIR", teclas)
        self.assertIn("fijarModoAutoIR", teclas)
        # Sin rutas de audio/LCD en el mando.
        self.assertNotIn("anunciarJarvis", teclas)
        self.assertNotIn("jarvisAudio", teclas)
        self.assertNotIn("pantallaFinal", teclas)
        self.assertNotIn("AUDIO_DESHABILITADO", teclas)
        # Tope usable.
        for marca in ("case CH_MENOS:", "case CH:", "case CH_MAS:",
                      "case PLAY:", "case VOL_MENOS:", "case VOL_MAS:",
                      "case EQ:", "ACK;IR;LUCES_TODAS_ON", "ACK;IR;DEMO_ON"):
            self.assertIn(marca, teclas, f"Falta marca tope: {marca}")
        despacho = self.firmware.split(
            "ResultadoOrden ejecutarOrdenActuador", 1)[1].split(
            "\n}\n", 1)[0]
        self.assertIn("carpetaSalida(orden.indiceRele)", despacho)
        self.assertIn("carpetaSalida(indice)", self.firmware)
        # El PARO usa el grupo de emergencia de su botón (carpeta 10, 3-4).
        self.assertIn("TECLA_0, false, 3, 4", self.firmware)

    def test_tecla_cambia_volumen(self):
        """VOL+ ya no es volumen: es demo; el API de audio queda para el futuro."""
        teclas = self.firmware.split("void ejecutarTeclaIRCasa", 1)[1].split(
            "\n}\n", 1)[0]
        self.assertIn("case VOL_MAS:", teclas)
        self.assertIn("case VOL_MENOS:", teclas)
        volmas = teclas.split("case VOL_MAS:", 1)[1].split("case EQ:", 1)[0]
        self.assertIn("DEMO_ON", volmas)
        self.assertNotIn("ajustarVolumen", volmas)
        self.assertIn("ajustarVolumen", self.audio_header)

    def test_repetir_solo_por_serial(self):
        """Repetir salió del mando (100+ = voz); sigue por Serial REPETIR."""
        self.assertIn('comando == "REPETIR"', self.firmware)
        self.assertIn("ACK;REPETIR", self.firmware)
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
        required_methods = ["begin", "habilitado", "silenciado", "ocupado",
                           "volumenActual", "vozActual", "silenciar",
                           "cambiarVoz", "ajustarVolumen", "repetirUltima",
                           "reproducir", "reproducirGrupo", "reproducirEstado",
                           "carpetaPara"]
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