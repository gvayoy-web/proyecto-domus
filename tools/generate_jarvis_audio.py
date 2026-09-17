"""Genera la microSD de Jarvis definida por las notas Obsidian 46, 65 y 66.

Cada una de las 21 teclas CAR MP3 tiene respuesta de voz propia (nota 46);
los eventos 1-14 conservan el catálogo original y los eventos 15-28 cubren
modo manual/auto, diagnóstico, todo apagado, silencio, rearme, cultivo,
ventilador y consultas de sensores.

Requiere ``edge-tts`` y acceso de red. La voz 1 (Carlos) usa carpetas 01-28
y la voz 2 (Karla) usa 51-78; ambas llevan pistas 001-004, tal como espera
el DFPlayer Mini (playFolder 01-99).
"""

from __future__ import annotations

import argparse
import asyncio
import csv
import hashlib
import shutil
from pathlib import Path

import edge_tts


CATALOGO = {
    1: ("sistema_listo", ("Sistemas en línea.", "DOMUS operativo.", "Inicialización completa.", "Casa preparada.")),
    2: ("orden_aceptada", ("Entendido.", "Orden confirmada.", "Ejecutando instrucción.", "Como ordene.")),
    3: ("orden_rechazada", ("Orden no permitida.", "No puedo ejecutar eso.", "Solicitud bloqueada.", "Acción denegada por seguridad.")),
    4: ("luz_encendida", ("Iluminación activada.", "Luz encendida.", "He encendido la luz.", "Circuito de luz activo.")),
    5: ("luz_apagada", ("Iluminación desactivada.", "Luz apagada.", "He apagado la luz.", "Circuito de luz detenido.")),
    6: ("riego_iniciado", ("Iniciando riego.", "Bomba de agua activada.", "Regando el cultivo.", "Ciclo de riego en marcha.")),
    7: ("riego_detenido", ("Riego detenido.", "Bomba desactivada.", "Ciclo de agua finalizado.", "He detenido el riego.")),
    8: ("tierra_seca", ("La tierra está seca.", "Humedad del suelo baja.", "El cultivo necesita agua.", "Suelo por debajo del nivel ideal.")),
    9: ("tierra_humeda", ("Humedad adecuada.", "La tierra está húmeda.", "Suelo dentro del nivel esperado.", "El cultivo tiene suficiente humedad.")),
    10: ("agua_baja", ("Nivel de agua bajo.", "Depósito insuficiente.", "Riego bloqueado por falta de agua.", "Recargue el depósito.")),
    11: ("temperatura_alta", ("Temperatura elevada.", "El ambiente está caliente.", "Recomiendo ventilación.", "Umbral térmico superado.")),
    12: ("presencia", ("Presencia detectada.", "Movimiento registrado.", "Hay actividad en la casa.", "Sensor de presencia activado.")),
    13: ("emergencia", ("Emergencia activada.", "Todas las salidas fueron detenidas.", "Sistema bloqueado por seguridad.", "Paro de emergencia activo.")),
    14: ("error_sensor", ("Sensor sin respuesta.", "Lectura no válida.", "Revise las conexiones del sensor.", "Diagnóstico requerido.")),
    15: ("modo_manual", ("Modo manual activado.", "Control manual habilitado.", "Tú mandas, yo obedezco.", "Automático desactivado.")),
    16: ("modo_auto", ("Modo automático activado.", "Automatización habilitada.", "La casa se gobierna sola.", "Control automático en marcha.")),
    17: ("diagnostico", ("Diagnóstico completado.", "Revisión del sistema terminada.", "Todos los módulos responden.", "Diagnóstico sin fallos críticos.")),
    18: ("todo_apagado", ("Todas las cargas han sido apagadas.", "Casa completamente apagada.", "He detenido todas las salidas.", "Todo apagado, sistema en reposo.")),
    19: ("sonido_activado", ("Sonido activado.", "Voz reanudada.", "Jarvis en línea.", "Respuestas de voz habilitadas.")),
    20: ("sistema_rearmado", ("Sistema rearmado.", "Bloqueo liberado, todo listo.", "Seguridad restablecida.", "Puedes continuar operando.")),
    21: ("luz_cultivo_encendida", ("Iluminación suplementaria activada.", "Luz de cultivo encendida.", "Cultivo iluminado.", "Lámpara de cultivo activa.")),
    22: ("luz_cultivo_apagada", ("Iluminación suplementaria apagada.", "Luz de cultivo desactivada.", "Cultivo en penumbra.", "Lámpara de cultivo detenida.")),
    23: ("ventilador_encendido", ("He encendido la ventilación.", "Ventilador activado.", "Circulación de aire en marcha.", "Ventilación encendida.")),
    24: ("ventilador_apagado", ("He apagado la ventilación.", "Ventilador desactivado.", "Circulación de aire detenida.", "Ventilación apagada.")),
    25: ("consulta_temp", ("La temperatura aparece en pantalla.", "Revisa el termómetro en el LCD.", "Temperatura mostrada en pantalla.", "El valor térmico está en pantalla.")),
    26: ("consulta_humedad", ("La humedad aparece en pantalla.", "Humedad ambiental en el LCD.", "Valor de humedad mostrado.", "Revisa la humedad en pantalla.")),
    27: ("consulta_suelo", ("El estado del suelo y del depósito aparece en pantalla.", "Suelo y agua mostrados en el LCD.", "Nivel de cultivo en pantalla.", "Revisa suelo y depósito en pantalla.")),
    28: ("estado_completo", ("El sistema está funcionando.", "Casa operativa sin fallos.", "Todos los sistemas nominales.", "DOMUS trabajando con normalidad.")),
}

VOCES = {
    1: ("es-HN-CarlosNeural", "-8%", "-8Hz", 0),
    2: ("es-HN-KarlaNeural", "+0%", "+2Hz", 50),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


async def generar(destino: Path, bancos: list[int]) -> None:
    destino.mkdir(parents=True, exist_ok=True)
    destino_sd = destino / "jarvis_sd"
    destino_sd.mkdir(exist_ok=True)
    filas: list[dict[str, str | int]] = []
    for banco in bancos:
        voz, rate, pitch, desplazamiento = VOCES[banco]
        nombre_voz = "Carlos" if banco == 1 else "Karla"
        for carpeta_base, (evento, frases) in CATALOGO.items():
            carpeta = carpeta_base + desplazamiento
            carpeta_biblioteca = destino / nombre_voz / f"{carpeta_base:02d}"
            carpeta_biblioteca.mkdir(parents=True, exist_ok=True)
            carpeta_sd = destino_sd / f"{carpeta:02d}"
            carpeta_sd.mkdir(exist_ok=True)
            for pista, frase in enumerate(frases, start=1):
                archivo = carpeta_biblioteca / f"{pista:03d}.mp3"
                if not archivo.exists() or archivo.stat().st_size < 1000:
                    await edge_tts.Communicate(frase, voz, rate=rate, pitch=pitch).save(str(archivo))
                    print(f"OK {archivo}: {frase}")
                archivo_sd = carpeta_sd / archivo.name
                shutil.copy2(archivo, archivo_sd)
                filas.append({
                    "voz": banco,
                    "motor_voz": voz,
                    "carpeta": f"{carpeta:02d}",
                    "pista": f"{pista:03d}",
                    "evento": evento,
                    "frase": frase,
                    "biblioteca": archivo.relative_to(destino).as_posix(),
                    "archivo": archivo_sd.relative_to(destino_sd).as_posix(),
                    "bytes": archivo_sd.stat().st_size,
                    "sha256": sha256(archivo_sd),
                })

    with (destino_sd / "MANIFEST.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=filas[0].keys())
        writer.writeheader()
        writer.writerows(filas)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("audio"))
    parser.add_argument("--bank", choices=("1", "2", "all"), default="all")
    args = parser.parse_args()
    bancos = [1, 2] if args.bank == "all" else [int(args.bank)]
    asyncio.run(generar(args.output, bancos))


if __name__ == "__main__":
    main()
