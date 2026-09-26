# PROJECT DOMUS — entrega de software

Estado: software entregado y presentado al jurado (26 sep 2026); validación
física completa queda pendiente según nota 82.

## Producto

- `firmware/casa_inteligente_v4/`: única implementación funcional.
- `firmware/domus_esqueleto/`: wrapper del producto con perfil 3 para el
  hardware disponible hoy.
- `firmware/diagnosticos/domus_banco_integracion/`: lector seguro de LCD,
  sensores, puntos de calibración y códigos IR; no acciona salidas.
- `firmware/legacy/domus_esqueleto/`: implementación anterior archivada.
- `tools/jarvis_pc/jarvis.html`: consola de puerto serie con subtítulos y
  voz TTS en español, diagnóstico hablado y SFX por las bocinas del PC
  (interfaz presentada; `smoke_jarvis.js` la valida con 64 checks).

## Garantías verificables por software

El firmware valida mapa GPIO, perfiles, calibración, histéresis, propiedad
manual/automática, nivel de agua, timeout, PARO, rearme, modo seguro y límites
de entrada Serial. El mando IR exige aprendizaje real y códigos únicos.
Última carga HIL 8/8 en COM9; bomba GPIO17 verificada en vivo
(`RIEGO_ON → SALIDA Bomba ENCENDIDO (GPIO verificado)`).

Jarvis funciona por mando IR y puerto serie con respuestas habladas desde el
PC (TTS del navegador). No existe reconocimiento de voz ni ruta de IA;
`RECONOCIMIENTO_VOZ=NO_USADO` en DIAGNOSTICO. El audio DFPlayer está
deshabilitado (su RX era GPIO17, pin hoy ocupado por la bomba).

## Fuera del alcance de software

Quedan pendientes mediciones eléctricas, calibración definitiva de suelo/LDR
(`CALIBRACION=PROVISIONAL`), códigos IR restantes del mando concreto, pruebas
en vivo de microSD, y la reactivación de audio DFPlayer (exige recableado).

Ejecutar desde la raíz:

```powershell
python tools/validate_project.py
python tools/validate_markdown.py
python -m unittest discover -s firmware/tests
node tools/jarvis_pc/smoke_jarvis.js
```
