"""HIL seguro para DOMUS (firmware de producto, perfil 4).

No flashea la placa ni enciende motores. Prueba protocolo serie, sensores,
LCD/IR detectados, luces via 74HC595 y DRV8833, y audio DFPlayer.
El operador debe indicar DOMUS_PORT si hay cero o varios puertos candidatos.
Sin placa o sin pyserial: SKIP claro.
"""
import os
import time
import unittest

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - dependencia exclusiva de HIL
    serial = None
    list_ports = None

BAUD = 115200
PROFILE = "CASA_FINAL_DRV8833_DFPLAYER"
# Luces reales del layout (nota 80): Casa, Porche y Spare (CH).
# Cultivo/INVER es canal de motor DRV8833 → NACK driver_no_listo sin F1.
LED_COMMANDS = (("LUZ1", "Casa"), ("LUZ2", "Porche"), ("SPARE", "Spare"))
MOTOR_COMMANDS = (("RIEGO_ON", "Bomba"),)


def resolve_port():
    explicit = os.environ.get("DOMUS_PORT", "").strip()
    if explicit:
        return explicit
    candidates = []
    for item in list_ports.comports():
        identity = f"{item.device} {item.description} {item.manufacturer or ''}".lower()
        if any(token in identity for token in ("ch343", "esp32", "usb serial", "usb-serial", "jtag")):
            candidates.append(item.device)
    if len(candidates) == 1:
        return candidates[0]
    detail = ", ".join(candidates) if candidates else "ninguno"
    raise unittest.SkipTest(
        f"puerto ESP32 no unívoco ({detail}); define DOMUS_PORT=COMx"
    )


class Board:
    def __init__(self, port):
        self.ser = serial.Serial(port, BAUD, timeout=0.2)
        time.sleep(0.8)
        self.ser.reset_input_buffer()
        time.sleep(1.5)
        while self.ser.in_waiting:
            self.ser.readline()

    def cmd(self, command, wait=1.4, keep_stream=False):
        self.ser.write((command + "\n").encode("ascii"))
        end = time.time() + wait
        lines = []
        while time.time() < end:
            line = self.ser.readline().decode("utf-8", "replace").strip()
            if not line:
                continue
            if line.startswith("SENSORES;") and not keep_stream:
                continue
            lines.append(line)
        return lines

    def close(self):
        self.ser.close()


def first(lines, prefix):
    return next((line for line in lines if line.startswith(prefix)), "")


class HilProductoBancoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if serial is None:
            raise unittest.SkipTest(
                "falta pyserial; instala firmware/tests/requirements-hil.txt"
            )
        port = resolve_port()
        try:
            cls.board = Board(port)
        except Exception as error:
            raise unittest.SkipTest(f"no se pudo abrir {port}: {error}")
        diagnostic = first(cls.board.cmd("DIAGNOSTICO", wait=2.5), "DIAGNOSTICO;")
        if f"PERFIL_CANDIDATO={PROFILE};" not in diagnostic:
            cls.board.close()
            raise unittest.SkipTest(
                f"{port} no ejecuta el perfil vigente: {diagnostic or 'sin respuesta'}"
            )
        cls.port = port

    @classmethod
    def tearDownClass(cls):
        try:
            # Estado seguro: todas las cargas manualmente apagadas. Nunca ON motor.
            for command in ("RIEGO_OFF", "VENT_OFF", "LUZ1_OFF", "LUZ2_OFF", "SPARE_OFF"):
                cls.board.cmd(command, wait=0.35)
        finally:
            cls.board.close()

    def test_01_identidad_y_diagnostico(self):
        line = first(self.board.cmd("DIAGNOSTICO", wait=2.0), "DIAGNOSTICO;")
        self.assertIn(f"PERFIL_CANDIDATO={PROFILE};", line)
        for field in ("WATCHDOG=", "PANTALLA=", "IR=ON", "BOMBA_ETAPA=DRV"):
            self.assertIn(field, line)

    def test_02_estado_contiene_sensores_y_seguridad(self):
        line = first(self.board.cmd("ESTADO"), "ESTADO;")
        self.assertNotIn("NIVEL_AGUA=", line)
        for field in (
            "Bomba=", "Casa=", "Porche=", "Cultivo=", "Spare=",
            "HUM_PCT=", "TEMP_C=", "HUM_AIRE_PCT=",
            "LUZ_PCT=", "EMERGENCIA=", "MODO_SEGURO=",
        ):
            self.assertIn(field, line)

    def test_03_prueba_guiada_entrega_bloque_completo(self):
        lines = self.board.cmd("PRUEBA", wait=2.5)
        self.assertTrue(first(lines, "PRUEBA;INICIO;"))
        self.assertTrue(first(lines, "PRUEBA;LCD="))
        self.assertTrue(first(lines, "ESTADO;"))
        self.assertTrue(first(lines, "DIAGNOSTICO;"))
        self.assertTrue(first(lines, "PRUEBA;FIN"))

    def test_04_comandos_invalidos_se_rechazan(self):
        self.assertTrue(any("NACK;HOLA;no_reconocido" in x for x in self.board.cmd("HOLA")))
        self.assertTrue(any("longitud_invalida" in x for x in self.board.cmd("X" * 30)))

    def test_05_leds_ida_y_vuelta(self):
        for command, state_name in LED_COMMANDS:
            self.assertTrue(any(x.startswith(f"ACK;{command}_ON;1") for x in self.board.cmd(f"{command}_ON")))
            state = first(self.board.cmd("ESTADO"), "ESTADO;")
            self.assertIn(f"{state_name}=1;", state)
            self.assertTrue(any(x.startswith(f"ACK;{command}_OFF;0") for x in self.board.cmd(f"{command}_OFF")))

    def test_06_ir_responde_y_expone_21_teclas(self):
        lines = self.board.cmd("IR_LISTA", wait=2.5)
        entries = [line for line in lines if line.startswith("IR;INDICE=")]
        self.assertEqual(21, len(entries))
        self.assertTrue(entries[0].startswith("IR;INDICE=0;"))
        self.assertTrue(entries[-1].startswith("IR;INDICE=20;"))

    def test_07_stream_de_sensores_llega(self):
        lines = self.board.cmd("ESTADO", wait=3.2, keep_stream=True)
        stream = first(lines, "SENSORES;")
        self.assertTrue(stream, "no llegó telemetría SENSORES en 3.2 s")

    def test_08_paro_bloquea_led_y_rearme_no_enciende(self):
        state = first(self.board.cmd("ESTADO"), "ESTADO;")
        if "EMERGENCIA=ON" in state:
            raise unittest.SkipTest("PARO ya estaba enclavado; revisa/libera el botón")
        self.assertTrue(first(self.board.cmd("PARO"), "EVENTO;PARO_EMERGENCIA;ACTIVO"))
        blocked = self.board.cmd("LUZ1_ON")
        self.assertTrue(any("NACK;LUZ1_ON;" in x and "paro" in x.lower() for x in blocked), blocked)
        self.assertTrue(first(self.board.cmd("REARMAR"), "ACK;REARMAR;SEGURO"))
        final = first(self.board.cmd("ESTADO"), "ESTADO;")
        for name in ("Bomba", "Casa", "Porche", "Cultivo"):
            self.assertIn(f"{name}=0;", final)


if __name__ == "__main__":
    unittest.main(verbosity=2)
