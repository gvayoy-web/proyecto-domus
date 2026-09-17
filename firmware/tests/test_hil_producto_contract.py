"""Contrato estático del ejecutor HIL seguro del perfil de banco."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
HIL = ROOT / "firmware" / "tests" / "test_hil_producto.py"


class HilProductoContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = HIL.read_text(encoding="utf-8")

    def test_apunta_al_perfil_de_producto_vigente(self):
        self.assertIn('PROFILE = "BANCO_COMPLETO_S8050_IR"', self.source)
        self.assertIn('DOMUS_PORT', self.source)

    def test_no_ordena_encender_motores(self):
        self.assertNotIn('cmd("RIEGO_ON"', self.source)
        self.assertNotIn('cmd("VENT_ON"', self.source)
        self.assertIn('"RIEGO_OFF", "VENT_OFF"', self.source)

    def test_verifica_bloque_prueba_ir_y_paro(self):
        for token in ('"PRUEBA"', '"IR_LISTA"', '"PARO"', '"REARMAR"'):
            self.assertIn(token, self.source)

    def test_stream_sensores_existe_en_firmware(self):
        # test_07 espera SENSORES; cada <=3.2 s: el firmware debe emitirlo.
        sketch = (ROOT / "firmware" / "casa_inteligente_v4" /
                  "casa_inteligente_v4.ino").read_text(encoding="utf-8")
        self.assertIn('"SENSORES;TEMP_C=%.1f', sketch)
        self.assertIn("INTERVALO_TELEMETRIA_SENSORES_MS", sketch)
        self.assertIn("emitirTelemetriaSensores();", sketch)


if __name__ == "__main__":
    unittest.main()
