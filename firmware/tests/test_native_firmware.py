"""Execute functions extracted from the production sketch on a host C++ compiler.

The serial/ADC devices are fakes; this validates firmware logic, not electronics.
Ubuntu CI supplies g++; Windows without a host compiler reports an explicit skip.
"""
import re
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


def run_host_process(argv, timeout, allow_skip=False):
    """Ejecuta un proceso local reintentando ante bloqueos del SO.

    En Windows el antivirus puede retener un .exe recién enlazado y el
    lanzamiento falla con OSError intermitente; reintentar no cambia lo
    verificado, solo la robustez del harness. Si la política del equipo
    (App Control, WinError 4551) bloquea el binario y allow_skip es True,
    se reporta SKIP en vez de FAIL: no se puede afirmar ni negar el
    comportamiento sin ejecutar; ese caso corre en CI Ubuntu.
    """
    last = None
    for _ in range(5):
        try:
            return subprocess.run(argv, check=True, capture_output=True, timeout=timeout)
        except subprocess.CalledProcessError as error:
            stdout = error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes) else (error.stdout or "")
            stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else (error.stderr or "")
            raise RuntimeError(
                f"Proceso fallo con rc={error.returncode}: {' '.join(map(str, argv))}\n"
                f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}"
            ) from error
        except OSError as error:
            last = error
            time.sleep(0.5)
    if allow_skip:
        raise unittest.SkipTest(f"El SO bloqueó el binario ({last}); corre en CI Ubuntu")
    raise last

SKETCH = Path(__file__).resolve().parents[1] / "casa_inteligente_v4" / "casa_inteligente_v4.ino"


def function(source, signature):
    start = source.index(signature + " {")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class NativeFirmwareTests(unittest.TestCase):
    def test_production_serial_adc_and_output_logic(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("No host C++ compiler; native behavior runs on Ubuntu CI")
        source = SKETCH.read_text(encoding="utf-8")
        constants = "\n".join(re.findall(
            r"^#define (?:MAX_FALLOS_ANTES_DE_REGISTRAR|INTERVALO_AVISO_SENSOR_MS)\s+\d+(?:UL)?", source, re.M))
        polarity = re.search(r"const bool SALIDA_ACTIVA_EN_BAJO\[TOTAL_SALIDAS\] = \{.*?\};", source, re.S)[0]
        actual = "\n".join(function(source, signature) for signature in (
            "int nivelSalida(int indice, bool encendida)",
            "void revisarComandosSerial()"))
        harness = r'''
#include <cassert>
#include <string>
#include <vector>
#include <deque>
#include <cstdint>
using String = std::string;
constexpr int LOW=0, HIGH=1, TOTAL_SALIDAS=5;
struct FakeSerial {
  std::deque<char> bytes;
  int available() { return bytes.size(); }
  char read() { char c=bytes.front(); bytes.pop_front(); return c; }
  void feed(const std::string &s) { for(char c:s) bytes.push_back(c); }
} Serial;
std::string bufferComandoSerial;
bool descartarComandoHastaNuevaLinea=false;
std::vector<std::string> commands, events;
void procesarComandoTexto(const String &s) { commands.push_back(s); }
void emitirEventoLocal(const String &s) { events.push_back(s); }
void registrarError(const char*, const String&) {}
    '''
        checks = r'''
void drain() { while(Serial.available()) revisarComandosSerial(); }
int main() {
  for(int i=0;i<5;i++) {
    bool low=(i==0 || !DOMUS_SALIDAS_ECONOMICAS);
    assert(nivelSalida(i,false)==(low?HIGH:LOW));
    assert(nivelSalida(i,true)==(low?LOW:HIGH));
  }
  Serial.feed(std::string(41,'X')+"RIEGO_ON\nESTADO\r\n"); drain();
  assert(commands.size()==1 && commands[0]=="ESTADO");
  assert(events.size()==1);
  commands.clear(); events.clear();
  Serial.feed(std::string(1000,'X')); revisarComandosSerial();
  assert(Serial.available()==872); drain();
  Serial.feed("RIEGO_ON"); drain(); assert(commands.empty());
  Serial.feed("\nPARO\n"); drain();
  assert(commands.size()==1 && commands[0]=="PARO");
  assert(events.size()==1);
  commands.clear();
  Serial.feed(std::string(40,'A')+"\n"); drain();
  assert(commands.size()==1 && commands[0].size()==40);
}
'''
        with tempfile.TemporaryDirectory(prefix="domus-native-") as directory:
            cpp = Path(directory) / "test.cpp"
            cpp.write_text(harness + constants + "\n" + polarity + "\n" + actual + checks, encoding="utf-8")
            for profile in (0, 1):
                with self.subTest(economical=profile):
                    executable = Path(directory) / f"test-{profile}.exe"
                    run_host_process([compiler, "-std=c++17", "-Wall", "-Wextra",
                                      "-DDOMUS_PERFIL_CASA=0",
                                      f"-DDOMUS_SALIDAS_ECONOMICAS={profile}",
                                      str(cpp), "-o", str(executable)], timeout=60)
                    run_host_process([str(executable)], timeout=10, allow_skip=True)
