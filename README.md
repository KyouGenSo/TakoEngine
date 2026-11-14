# TakoEngine

[![DebugBuild](https://github.com/KyouGenSo/TakoEngine/actions/workflows/DebugBuild.yml/badge.svg?event=push)](https://github.com/KyouGenSo/TakoEngine/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/KyouGenSo/TakoEngine/actions/workflows/ReleaseBuild.yml/badge.svg?event=push)](https://github.com/KyouGenSo/TakoEngine/actions/workflows/ReleaseBuild.yml)
[![Release](https://img.shields.io/github/v/release/KyouGenSo/TakoEngine)](https://github.com/KyouGenSo/TakoEngine/releases)

**English** | [日本語](README.ja.md)

A DirectX 12 game engine for Windows. Build scenes in the editor (TakoEditor) and write gameplay in C++ scripts, which are recompiled and swapped in without closing the editor.

![TakoEditor](.github/images/editor.png)

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [Developing the Engine](#developing-the-engine)
- [Third-Party Libraries](#third-party-libraries)
- [Changelog](#changelog)
- [License](#license)

## Features

**Editor**
- Scene editor built on Actors and Components, with nested prefabs and multi-selection
- One Undo / Redo history shared by every editor
- Shader, Material, Particle, Animation, Camera Animation, UI Layout, Behavior Tree and Render Pipeline editors
- Crash recovery: unsaved scenes can be restored on the next launch, and a minidump is written to `Generated/Crashes/`

**C++ Scripting**
- Hot reload: compile from the editor (Ctrl+B, or Auto Compile when the editor regains focus) and swap in the new `Game.dll`. Property values and selection are kept
- Reflection with `TCLASS` / `TPROPERTY`: properties show up in the Inspector and are saved to scene files without extra code
- Built-in components: Mesh, Camera, Light, Collider (AABB / OBB / Sphere), AudioSource, Animator, BehaviorTree, ParticleSystem, PostProcess, SkyBox, UILayout

**Rendering**
- DirectX 12 with a render graph; the render pipeline can be edited in the editor
- Root signatures and resource bindings generated from shader reflection
- Materials, shadow maps, decals, skinned animation
- GPU particles and multi-pass post effects
- SDF text for in-game UI

**Distribution**
- Release builds produce a single exe
- Prebuilt SDK zips on [Releases](https://github.com/KyouGenSo/TakoEngine/releases); game projects install and update the engine with `UpdateEngine.bat`
- Projects saved in an older format are migrated automatically, after a backup

## Requirements

- Windows 10 / 11 (64-bit) and a GPU that supports DirectX 12
- Visual Studio 2026 with the "Desktop development with C++" workload (used to compile scripts)

## Getting Started

### Engine and projects

| Folder | How to tell |
|---|---|
| Engine | Has `bin`, `build`, `project` and `templates` (this repository, or an extracted release zip) |
| Project (game) | Has `<Name>.takoproject`, `<Name>.sln` and `GameProject` |

A project uses the engine through its `Engine\TakoEngine` folder, which is one of:

- **Link**: projects created in the editor point to the engine that created them. Updating that engine updates every linked project
- **Copy**: installed by `UpdateEngine.bat`. It belongs to that project only and is updated with `UpdateEngine.bat`

### Create a new game

1. Download `TakoEngine-<version>.zip` from [Releases](https://github.com/KyouGenSo/TakoEngine/releases) and extract it where it will stay. This is the engine; don't move it later
2. Run `bin\Debug\TakoEditor.exe`. When it says "No project was specified", choose **No** to create a new project
3. Choose a location and a name **outside** the engine folder
4. Open `<Name>.sln` in Visual Studio and press F5 to launch the editor
5. Put scripts in `GameProject\Script` (in Assets, open `Script` and right-click > **New Script**). Saving and returning to the editor compiles them automatically (Ctrl+B to compile manually)
6. To ship, build the **Release** configuration. The exe is written to `Generated\outputs\Release`

To update the engine, extract the new zip to the same place and rebuild the game's sln.

### Join a team project

1. Clone the game repository (or download it as a zip)
2. Run `UpdateEngine.bat` at the root. It installs the engine version written in `.takoproject` into `Engine\TakoEngine`
3. Open `<Name>.sln` in Visual Studio and press F5 to launch the editor

Run `UpdateEngine.bat` again to update to the latest release (`UpdateEngine.bat -Version 2.1.0` picks a version).

> [!NOTE]
> Launch a project from its own sln. Opening it with another engine's editor can crash, because the game code does not match that engine.

## Developing the Engine

1. Clone this repository, open `project\TakoEngine.sln` in Visual Studio and build. F5 launches the editor
2. Create a test project outside the engine: `bin\Debug\TakoEditor.exe --new <location> <name>`. It links to this engine, so rebuilding the engine takes effect immediately (rebuild the game's sln as well when headers change)
3. Build the distribution zip with `pwsh tools\package_sdk.ps1`, and check that the zip works on its own with `pwsh tools\test_sdk.ps1`
4. To release, update `VERSION` and `CHANGELOG.md`, commit, then run `pwsh tools\release.ps1`

## Third-Party Libraries

| Library | Used for | License |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Editor UI | MIT |
| [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) | Behavior Tree editor | MIT |
| [Assimp](https://github.com/assimp/assimp) | Model loading | BSD-3-Clause |
| [DirectXTex](https://github.com/microsoft/DirectXTex) | Texture loading | MIT |
| [miniaudio](https://github.com/mackron/miniaudio) | Audio | Public Domain / MIT-0 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON | MIT |
| [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders) | Editor icons | zlib |

## Changelog

See [CHANGELOG.md](CHANGELOG.md) (written in Japanese).

## License

TakoEngine is released under the [MIT License](LICENSE). Third-party libraries are under their own licenses (see [Third-Party Libraries](#third-party-libraries)).
