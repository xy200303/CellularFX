# CellularFX

[![Godot 4.2+](https://img.shields.io/badge/Godot-4.2%2B-blue.svg)](https://godotengine.org)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A high-performance **cellular automata / falling-sand engine** for Godot 4, written as a GDExtension in C++ with optional GPU compute acceleration.

[中文文档](README-ZH.md)

---

## Features

- **Dual backends**: switch between CPU and GPU simulation at runtime.
  - CPU backend: single-threaded with active-region optimization.
  - GPU backend: Vulkan compute shader for massively parallel updates.
- **Material system**: define materials as Godot `Resource` assets with physics and reaction rules.
  - Built-in material types: Solid, Powder, Liquid, Gas.
  - Behaviours: gravity, diagonal sliding, horizontal flow, density-based displacement.
  - Reactions: lifetime/decay, flammability/burning, corrosion, explosion.
- **Large grids**: efficiently simulate 256×256 to 1024×1024 worlds.
- **Runtime API**: initialize, update, draw, clear, and switch backends from GDScript.
- **Editor-friendly**: works directly inside the Godot editor; materials can be configured in the Inspector.

## Supported Platforms

| Platform | Status | Notes |
|---|---|---|
| Windows | ✅ Tested | MSVC-only ABI (see Build section) |
| Linux | 🚧 Planned | |
| macOS | 🚧 Planned | |
| Android / Web | 🚧 Future | |

## Installation

### Option 1: Godot Asset Library (recommended, when published)

Search **"CellularFX"** in the Godot Asset Library and install it into your project.

### Option 2: Manual

1. Download the latest release from the [Releases](https://github.com/yourname/cellularfx/releases) page.
2. Copy the `addons/cellular_automata_engine/` folder into your Godot project.
3. Enable the plugin in **Project → Project Settings → Plugins → CellularFX**.

## Quick Start

```gdscript
extends Node2D

@onready var world: CASWorld = $CASWorld

func _ready():
    # Optional: switch to GPU backend before init()
    world.set_backend(CASWorld.BACKEND_GPU)
    world.init(256, 256)

    # Register materials
    var sand := CASMaterial.new()
    sand.material_name = "sand"
    sand.material_type = CASMaterial.TYPE_POWDER
    sand.material_color = Color(0.76, 0.70, 0.50)
    sand.density = 5
    world.register_material(sand)

    var water := CASMaterial.new()
    water.material_name = "water"
    water.material_type = CASMaterial.TYPE_LIQUID
    water.material_color = Color(0.25, 0.50, 1.0)
    water.density = 3
    world.register_material(water)

    var stone := CASMaterial.new()
    stone.material_name = "stone"
    stone.material_type = CASMaterial.TYPE_SOLID
    stone.material_color = Color(0.5, 0.5, 0.5)
    stone.density = 10
    world.register_material(stone)

    # Draw some sand
    world.set_cell(128, 10, "sand")

func _process(_delta):
    world.update()
```

## API Reference

### CASWorld

| Method | Description |
|---|---|
| `init(width, height)` | Create the simulation grid. |
| `set_backend(backend)` | `BACKEND_CPU` or `BACKEND_GPU`. Call before `init()`. |
| `get_backend_name()` | Returns `"CPU"` or `"GPU"`. |
| `register_material(material)` | Register a `CASMaterial` resource. |
| `set_cell(x, y, material_name)` | Place a material. |
| `get_cell(x, y)` | Get material name at position. |
| `update()` | Advance one simulation frame. |
| `clear()` | Clear the world. |
| `get_texture()` | Get the rendered `Texture2D`. |
| `get_particle_count()` | Get number of non-empty cells. |
| `save_world(path)` | Save the world to a binary file. |
| `load_world(path)` | Load the world from a binary file. |

### CASMaterial

| Property | Type | Description |
|---|---|---|
| `material_name` | `String` | Unique material identifier. |
| `material_type` | `int` | `TYPE_SOLID`, `TYPE_POWDER`, `TYPE_LIQUID`, `TYPE_GAS`. |
| `material_color` | `Color` | Display color. |
| `density` | `int` | Used for displacement between same-type liquids. |
| `lifetime` | `int` | Frames before the cell decays (`0` = infinite). |
| `decay_to` | `String` | Material name to become when lifetime expires. |
| `flammable` | `bool` | Catches fire when adjacent to a hot material. |
| `burn_to` | `String` | Material name to become when ignited. |
| `corrosive` | `bool` | Eats neighbouring non-corrosive materials. |
| `corrosion_residue` | `String` | Material left behind after corrosion. |
| `corrosion_chance` | `float` | Probability (0-1) to corrode a neighbour each frame. |
| `explosive` | `bool` | Detonates when adjacent to a hot material. |
| `explode_to` | `String` | Material to replace the 3×3 explosion area. |

## Backend Notes

- **GPU backend** requires a windowed/GPU-capable Godot process. In `--headless` mode it automatically falls back to CPU.
- GPU uses a Vulkan compute shader with 3 passes: vertical fall, diagonal slide, and horizontal flow. Material properties are synchronised to the GPU for future reaction support.
- CPU uses an active-region scan so empty areas are skipped and supports full reaction rules (burning, corrosion, explosion, lifetime).

## Building from Source

### Requirements

- Godot 4.2+ (Windows binaries are built with MSVC)
- Python 3 + SCons
- Visual Studio 2022 with C++ workload (Windows)
- Git submodules initialized

### Steps

```bash
git clone --recursive https://github.com/yourname/cellularfx.git
cd cellularfx
scons platform=windows target=template_debug arch=x86_64 build_profile=build_profile.json -j4
```

> ⚠️ **Important**: on Windows you must use **MSVC**, not MinGW, because Godot's official Windows binaries are MSVC-ABI and GDExtension ABI must match.

## Project Structure

```text
cellularfx/
├── addons/cellular_automata_engine/   # Plugin files, GDExtension config, icons, shaders
├── demo/                              # Example scenes and tests
├── src/
│   ├── api/                           # CASWorld, CASMaterial (GDScript-facing)
│   ├── core/                          # Cell, WorldGrid, MaterialRegistry, ISimulator
│   ├── cpu/                           # CPUSimulator
│   └── gpu/                           # GPUSimulator + compute shader
├── godot-cpp/                         # godot-cpp submodule
├── build_profile.json                 # Trimmed godot-cpp API for fast builds
├── SConstruct
└── README.md
```

## Roadmap

- [x] GDExtension skeleton & MSVC build
- [x] CPU backend MVP
- [x] CPU active-region optimization
- [x] GPU compute backend
- [x] More materials and reaction rules (fire, smoke, acid, oil, wood, gunpowder)
- [x] Data-driven material rules (lifetime, burning, corrosion, explosion)
- [ ] Multi-threaded CPU backend
- [ ] Editor plugin (brush tools, material palette, save/load)
- [ ] World serialization
- [ ] Linux & macOS support
- [ ] Visual effects (glow, distortion, particles)

## License

[MIT License](LICENSE)

## Acknowledgements

Built with [godot-cpp](https://github.com/godotengine/godot-cpp).
