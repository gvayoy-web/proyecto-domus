"""Genera la microSD de Jarvis: 21 carpetas, una por botón del mando CAR MP3.

Códigos físicos en la nota Obsidian 64, acciones por botón en la nota 46 y
formato DFPlayer en la nota 65. Convención de variantes por carpeta:
conmutadores 1 = ON manual, 2 = OFF manual, 3 = ON automático,
4 = OFF automático; botón 0: 1-2 apagado, 3-4 emergencia; EQ: 1-2
diagnóstico, 3-4 fallo de sensor; tecla 8: 1-2 consulta, 3 tierra seca
(reservada), 4 depósito bajo; tecla 9: estado (3 = "Sistemas en línea":
arranque y cambio de voz); 200+: 1-2 rearme logrado, 3-4 sigue bloqueado;
100+: 1/3 voz Carlos, 2/4 voz Karla (un toque alterna; repetir: Serial).
Tecla 4 = todas_luces (reemplaza ventilador muerto).

Requiere ``edge-tts`` y acceso de red. La voz 1 (Carlos) usa carpetas 01-21
y la voz 2 (Karla) usa 51-71; ambas llevan pistas 001-004, tal como espera
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
    1: ("ch_menos", ("Modo manual activado.", "Control manual habilitado.", "Tú mandas, yo obedezco.", "Automático desactivado.")),
    2: ("ch_pagina", ("Página siguiente.", "Cambiando de página.", "En pantalla lo ves.", "Mira el LCD.")),
    3: ("ch_mas", ("Modo automático activado.", "Automatización habilitada.", "La casa se gobierna sola.", "Control automático en marcha.")),
    4: ("casa", ("Casa encendida.", "Casa apagada.", "Casa en automático.", "Casa apagada en automático.")),
    5: ("play_silencio", ("Sonido activado.", "Silencio activado.", "Voz reanudada.", "Jarvis enmudecido.")),
    6: ("porche", ("Porche encendido.", "Porche apagado.", "Porche en automático.", "Porche apagado en automático.")),
    7: ("vol_menos", ("Volumen más bajo.", "Bajando el volumen.", "Volumen al mínimo.", "Casi en silencio.")),
    8: ("vol_mas", ("Volumen más alto.", "Subiendo el volumen.", "Volumen al máximo.", "Se escucha fuerte.")),
    9: ("eq_diagnostico", ("Diagnóstico completado.", "Revisión del sistema terminada.", "Sensor sin respuesta.", "Revisa las conexiones del sensor.")),
    10: ("tecla_0", ("Todo apagado.", "Cargas detenidas.", "Emergencia activada.", "Paro de emergencia, todo detenido.")),
    11: ("tecla_100_voz", ("Voz Carlos activada.", "Voz Karla activada.", "Hablo como Carlos.", "Hablo como Karla.")),
    12: ("tecla_200", ("Sistema rearmado.", "Bloqueo liberado, todo listo.", "Sigue bloqueado.", "No puedo rearmar todavía.")),
    13: ("tecla_1_sala", ("Luz de sala encendida.", "Luz de sala apagada.", "Sala iluminada en automático.", "Sala apagada en automático.")),
    14: ("tecla_2_cuarto", ("Luz de cuarto encendida.", "Luz de cuarto apagada.", "Cuarto iluminado en automático.", "Cuarto apagado en automático.")),
    15: ("tecla_3_cultivo", ("Luz de cultivo encendida.", "Luz de cultivo apagada.", "Cultivo iluminado en automático.", "Cultivo apagado en automático.")),
    16: ("todas_luces", ("Todas las luces encendidas.", "Todas las luces apagadas.", "Luces encendidas en automático.", "Luces apagadas en automático.")),
    17: ("tecla_5_riego", ("Iniciando riego.", "Riego detenido.", "Riego automático en marcha.", "Riego automático detenido.")),
    18: ("tecla_6_temp", ("La temperatura aparece en pantalla.", "Revisa el termómetro en el LCD.", "Temperatura mostrada en pantalla.", "El valor térmico está en pantalla.")),
    19: ("tecla_7_humedad", ("La humedad aparece en pantalla.", "Humedad ambiental en el LCD.", "Valor de humedad mostrado.", "Revisa la humedad en pantalla.")),
    20: ("tecla_8_suelo", ("Suelo y depósito en pantalla.", "Revisa suelo y agua en el LCD.", "La tierra está seca.", "Depósito bajo, riego bloqueado.")),
    21: ("tecla_9_estado", ("El sistema está funcionando.", "Casa operativa sin fallos.", "Sistemas en línea.", "DOMUS trabajando con normalidad.")),
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
