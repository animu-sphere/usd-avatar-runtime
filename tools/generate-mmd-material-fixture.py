"""Generate redistributable material and combined inputs with the MMD owner's encoders."""
import argparse
import copy
import importlib.util
from pathlib import Path
import struct
import zlib


def load(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def generate(owner, out):
    pmx = load(owner / "tests/fixtures/generate_fixtures.py")
    vmd = load(owner / "tests/fixtures/generate_vmd_fixtures.py")
    model = copy.deepcopy(pmx._fixtures()["live-physics-morph.pmx"][2])
    model["textures"] = ["sample.png"]
    model["materials"] = [pmx.material("first", "first", 3, ("texture", 0), 0, 0, 0x11),
                          pmx.material("second", "second", 3, ("texture", 0), 0, 0, 0x11)]
    model["materials"][1]["sphereMode"] = 1
    model["materials"][1]["diffuse"] = (.5, .625, .75, 1.)
    offsets = []
    for operation, start in ((0, .75), (1, .03125)):
        n = 0
        offset = {"material": -1 if operation == 0 else 0, "operation": operation}
        for name, count in (("diffuse", 4), ("specular", 3), ("specularPower", 1),
                            ("ambient", 3), ("edgeColor", 4), ("edgeSize", 1),
                            ("textureTint", 4), ("sphereTint", 4), ("toonTint", 4)):
            values = tuple(start + (n + a) / 128 for a in range(count))
            n += count
            offset[name] = values[0] if count == 1 else values
        offsets.append(offset)
    model["morphs"][4]["offsets"] = offsets
    model["morphs"].append({"name": "matGroup", "englishName": "matGroup", "panel": 4,
                            "type": "group", "offsets": [(4, .5)]})
    motion = vmd.Motion("Live material coefficients")
    for frame, weight in ((0, 0.), (15, 1.), (30, -.5), (60, 0.)):
        motion.morphs.extend((("matGroup", frame, 2 * weight), ("material", frame, -.5 * weight)))
    out.mkdir(parents=True, exist_ok=True)
    (out / "material.pmx").write_bytes(pmx.encode(model)[0])
    (out / "material.vmd").write_bytes(motion.encode())
    combined = copy.deepcopy(vmd.fixtures()["live-physics-uv-mixed.vmd"][0])
    combined.morphs.extend(motion.morphs)
    (out / "combined.vmd").write_bytes(combined.encode())
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    pixels = bytes([0, 64, 128, 192, 255, 192, 64, 128, 192,
                    0, 128, 192, 64, 128, 192, 128, 64, 255])
    (out / "sample.png").write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">2I5B", 2, 2, 8, 6, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--owner", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    generate(args.owner, args.out)
