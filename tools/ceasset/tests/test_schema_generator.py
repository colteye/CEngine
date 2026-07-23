from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
GENERATOR = ROOT / "tools" / "schema" / "generate.py"
ENGINE_GAME = ROOT / "schemas" / "engine.game.json"
sys.path.insert(0, str(ROOT / "tools" / "ceasset"))

from ceassetlib.game_schema import load_game_schema, make_schema_entity


class SchemaGeneratorTests(unittest.TestCase):
    def test_generated_cpp_is_standalone_standard_library_code(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            header = Path(temporary) / "entities.h"
            subprocess.run(
                [
                    sys.executable,
                    str(GENERATOR),
                    "--manifest",
                    str(ENGINE_GAME),
                    "--cpp-header",
                    str(header),
                    "--namespace",
                    "Test::Generated",
                ],
                check=True,
            )
            source = header.read_text(encoding="utf-8")
            generated_files = {path.name for path in Path(temporary).iterdir()}

        includes = {
            line for line in source.splitlines() if line.startswith("#include")
        }
        self.assertEqual(generated_files, {"entities.h"})
        self.assertEqual(
            includes,
            {
                "#include <cstddef>",
                "#include <cstdint>",
                "#include <cstring>",
            },
        )
        self.assertNotIn("glm", source)
        self.assertNotIn("AssetHandle", source)
        self.assertNotIn("EntityFactory", source)
        self.assertNotIn("BinaryReader", source)
        self.assertIn("bytes[offset + 3u]) << 24u", source)

    def test_game_file_contains_data_not_cpp_bindings(self) -> None:
        game = json.loads(ENGINE_GAME.read_text(encoding="utf-8"))
        encoded = json.dumps(game)
        self.assertNotIn('"cpp"', encoded)
        self.assertNotIn("cpp_namespace", encoded)
        self.assertNotIn("header", encoded)

    def test_new_game_entity_generates_for_cpp_and_python(self) -> None:
        compiler = shutil.which(os.environ.get("CXX", "c++"))
        self.assertIsNotNone(compiler, "a C++ compiler is required")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "pickup.game.json"
            header = root / "generated" / "pickup_entities.h"
            bundle = root / "generated" / "game.json"
            manifest.write_text(json.dumps({
                "format_version": 1,
                "module": {
                    "id": "pickup_game",
                    "name": "Pickup Game",
                    "version": "1.0.0",
                },
                "content": {
                    "source_roots": ["assets/source"],
                    "asset_roots": ["assets/compiled"],
                },
                "asset_types": [
                    {"name": "item", "extension": ".citem"},
                ],
                "entities": [{
                    "classname": "pickup",
                    "version": 1,
                    "fields": [
                        {"name": "transform", "type": "transform"},
                        {"name": "amount", "type": "u32", "default": 1},
                    ],
                }],
            }), encoding="utf-8")

            subprocess.run([
                sys.executable,
                str(GENERATOR),
                "--manifest", str(manifest),
                "--cpp-header", str(header),
                "--namespace", "PickupGame::Generated",
                "--bundle", str(bundle),
            ], cwd=root.parent, check=True)

            source = root / "compile_test.cpp"
            source.write_text(
                '#include "generated/pickup_entities.h"\n'
                "int main() {\n"
                "    std::uint8_t bytes[PickupGame::Generated::PickupSize]{};\n"
                "    PickupGame::Generated::Pickup pickup;\n"
                "    return PickupGame::Generated::Read(\n"
                "        bytes, sizeof(bytes), 0, pickup) ? 0 : 1;\n"
                "}\n",
                encoding="utf-8",
            )
            executable = root / "compile_test"
            subprocess.run([
                compiler,
                "-std=c++17",
                "-I", str(root),
                str(source),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)

            game = load_game_schema(bundle)
            pickup = make_schema_entity(game, "pickup", amount=3)
            self.assertEqual(pickup.values["amount"], 3)
            self.assertEqual(game.asset_types[0]["extension"], ".citem")


if __name__ == "__main__":
    unittest.main()
