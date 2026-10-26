#!/usr/bin/env python3
"""Gera a experiência RifleDemo (mundo + sons) a partir das fontes versionadas.

As fontes legíveis vivem em assets/examples/rifledemo/*.lua. O arquivo
world.loblox.json precisa embutir o código (scripts de experiência não leem
arquivos), então este gerador mantém os dois em sincronia e cria os efeitos
sonoros determinísticos usados pela demonstração.

Uso: python3 tools/examples/build_rifledemo.py [--check]
  --check  Falha se o mundo versionado estiver diferente do gerado.
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import shutil
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DEMO = ROOT / "assets" / "examples" / "rifledemo"
FACTORY = ROOT / "assets" / "defaults" / "factory" / "world.loblox.json"
SAMPLE_RATE = 22050


def tone(seconds: float, start_hz: float, end_hz: float, volume: float,
         decay: float, noise: float = 0.0, seed: int = 1) -> list[float]:
    """Amostras de um bipe com decaimento e opção de ruído determinístico."""
    count = int(SAMPLE_RATE * seconds)
    state = seed
    samples = []
    for i in range(count):
        t = i / SAMPLE_RATE
        progress = i / max(1, count - 1)
        hz = start_hz + (end_hz - start_hz) * progress
        envelope = math.exp(-decay * t)
        value = math.sin(2 * math.pi * hz * t)
        if noise > 0:
            state = (1103515245 * state + 12345) & 0x7FFFFFFF
            value = value * (1 - noise) + noise * ((state / 0x7FFFFFFF) * 2 - 1)
        samples.append(volume * envelope * value)
    return samples


def write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(SAMPLE_RATE)
        frames = b"".join(struct.pack("<h", int(max(-1, min(1, s)) * 32767)) for s in samples)
        file.writeframes(frames)


def build_sounds() -> dict[str, str]:
    sounds = {
        "tiro": ("assets/tiro.wav", 0.16, tone(0.16, 190, 70, 0.55, 24, noise=0.85, seed=7)),
        "impacto": ("assets/impacto.wav", 0.08, tone(0.08, 900, 380, 0.35, 60, noise=0.5, seed=11)),
        "recarga": ("assets/recarga.wav", 0.5, tone(0.12, 320, 260, 0.35, 40, noise=0.2, seed=3)
                    + [0.0] * int(SAMPLE_RATE * 0.18)
                    + tone(0.12, 260, 220, 0.35, 40, noise=0.2, seed=5)),
        "equipar": ("assets/equipar.wav", 0.14, tone(0.14, 280, 640, 0.3, 18, seed=13)),
        "vazio": ("assets/vazio.wav", 0.06, tone(0.06, 520, 480, 0.28, 70, noise=0.3, seed=17)),
    }
    library = []
    for sound_id, (reference, seconds, samples) in sounds.items():
        write_wav(DEMO / reference, samples)
        library.append({"id": sound_id, "name": sound_id.capitalize(),
                        "reference": reference, "duration": round(len(samples) / SAMPLE_RATE, 3),
                        "volume": 1.0, "loop": False, "kind": "sound"})
    return {"files": {sid: value[0] for sid, value in sounds.items()}, "library": library}


def part(pid, name, parent, position, size, color, material="Metal", anchored=True,
         collidable=True, shape="Block"):
    return {"id": pid, "class": "Part", "name": name, "parent": parent,
            "shape": shape, "position": list(position), "rotation": [0.0, 0.0, 0.0],
            "size": list(size), "color": list(color) + [255], "material": material,
            "anchored": anchored, "canCollide": collidable}


def build_world(item_source: str, demo_source: str, library: list[dict]) -> dict:
    shutil.copyfile(ROOT / "midia/logo.png", DEMO / "assets/logo.png")
    factory = json.loads(FACTORY.read_text())
    base = {obj["name"]: obj for obj in factory["objects"]}
    game = factory["game"]
    game["sounds"] = library
    objects = [
        {"id": 1, "class": "Workspace", "name": "Workspace", "parent": 0},
        base["Iluminação"],
        base["Plataforma inicial"],
        base["Ponto de nascimento"],
        {"id": 5, "class": "ServerScriptService", "name": "ServerScriptService", "parent": 0},
        {"id": 6, "class": "Script", "name": "Demo.lua", "parent": 5,
         "source": demo_source, "enabled": True},
        # Protótipo autoral: o inventário o registra antes de rodar scripts. Fica
        # numa área de ensaio fora do spawn para nunca obstruir o nascimento.
        {"id": 10, "class": "Model", "name": "RifleDemo", "parent": 1,
         "modelPivot": {"position": [60.0, 2.0, 60.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": 1.0},
         "attributes": {"_linblux_item_prototype": True, "item_type": "fuzil"}},
        part(11, "Corpo", 10, (60.0, 2.0, 60.0), (1.0, 1.0, 4.4), (70, 72, 80)),
        part(12, "Cano", 10, (60.0, 2.15, 55.8), (0.4, 0.4, 4.4), (52, 54, 60)),
        part(13, "Coronha", 10, (60.0, 1.9, 63.2), (1.2, 1.1, 2.2), (110, 74, 46), "Wood"),
        {"id": 14, "class": "Model", "name": "Ferrolho", "parent": 10,
         "modelPivot": {"position": [60.0, 2.3, 60.6], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": 1.0}},
        part(15, "Culatra", 14, (60.0, 2.3, 60.6), (0.5, 0.36, 1.0), (150, 152, 160)),
        {"id": 16, "class": "Attachment", "name": "BocaDoCano", "parent": 12,
         "attachment": {"position": [0.0, 0.0, -2.4], "rotation": [0.0, 0.0, 0.0, 1.0]}},
        {"id": 17, "class": "Script", "name": "Item.lua", "parent": 10,
         "source": item_source, "enabled": True},
        dict(part(18, "Placa LINBLUX", 1, (14, 4, -14), (5, 5, 0.4), (40, 40, 46)),
             attributes={"decal_image": "assets/logo.png", "decal_face": "Front"}),
    ]
    return {"format": "loblox-project", "version": 5, "nextId": 20,
            "objects": objects, "game": game}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    sounds = build_sounds()
    world = build_world((DEMO / "Item.lua").read_text(), (DEMO / "Demo.lua").read_text(),
                        sounds["library"])
    rendered = json.dumps(world, indent=1, ensure_ascii=False) + "\n"
    target = DEMO / "world.loblox.json"
    if args.check:
        if not target.exists() or target.read_text() != rendered:
            print("world.loblox.json está desatualizado; rode o gerador", file=sys.stderr)
            return 1
        print("RifleDemo: mundo e sons em sincronia")
        return 0
    target.write_text(rendered)
    print(f"RifleDemo: {target} ({len(world['objects'])} objetos, {len(sounds['library'])} sons)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
