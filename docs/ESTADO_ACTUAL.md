# Estado actual de PROJECT DOMUS

Actualizado: 26 de septiembre de 2026 (cierre y presentación al jurado). La [nota 82](../obsidian/proyect%20domus/82%20-%20Cierre%20jurado%20bomba%20GPIO17%20Jarvis%20PC%20y%20documentacion.md) es la bitácora de cierre; la [auditoría 63](../obsidian/proyect%20domus/63%20-%20Auditoria%20total%20de%20Obsidian%20y%20estado%20real.md) sigue siendo la autoridad documental.

El firmware `firmware/casa_inteligente_v4/` es el producto, perfil 4 (`CASA_FINAL_DRV8833_DFPLAYER`), montado y probado en COM9. Hardware real: bomba en **GPIO17 directo** (GPIO ALTO = riego ON, apaga luces, timeout 120 s), LEDs Casa/Porche/Spare en GPIO5/GPIO8/GPIO6 (activos en HIGH), suelo GPIO15 y LDR GPIO3, IR HX1838 GPIO12. LCD y DHT11 descartados (quemados); DFPlayer deshabilitado (su RX era GPIO17, hoy bomba); microSD habilitada en firmware (`SD=ON`) pero sin pruebas en vivo.

La interfaz `tools/jarvis_pc/jarvis.html` (Jarvis/NEXUS) es la consola por puerto serie: subtítulos y voz TTS en español, diagnóstico hablado, SFX estéreo por las bocinas del PC, panel con Diagnóstico / Prueba guiada / Recuperar. Es la interfaz presentada al jurado.

Validación: **115 tests locales en verde** (contract, audio, pantalla, HIL, nativos) más el smoke de 64 checks del HTML con 0 fallos. Última carga HIL 8/8 COM9. Nota: la compilación nativa g++ de esta máquina quedó dañada en el entorno (cc1plus de WinGet GCC 16.1.0 crashea incluso con un `int main(){}` desde el 30/7/2026); las suites nativas se compilan en CI Ubuntu — no es un fallo del código.

Preparar el banco con [PRUEBA_HOY](../firmware/PRUEBA_HOY.md) y el [diagrama vigente](../visualizaciones/domus-banco-final-s8050-ir.svg). Las notas antiguas y los DOCX se conservan para trazabilidad, sin autoridad para cablear o comprar.
