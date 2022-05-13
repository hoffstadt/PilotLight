## Pilot Light (WIP)

This repo is a collection of minimal examples for various graphics APIs. The goal of each example is to have a single file that is easy to follow and no abstractions.

### Current Graphics APIs
* Directx 11
* Directx 12
* Vulkan (Windows & Linux)

## Windows Requirements
**Requirements**
- [_git_](https://git-scm.com/)
- [_Visual Studio 2019_ or _2022_](https://visualstudio.microsoft.com/vs/) with the following workflows (just for the toolchain):
  * Desktop development with C++
  * Game development with C++
- [Vulkan SDK (only for vulkan example)](https://vulkan.lunarg.com/)
  * Ensure **VK_LAYER_PATH** environment variable is set.

## Linux Requirements
**Requirements**
- [_git_](https://git-scm.com/)
- [Vulkan SDK (only for vulkan example)](https://vulkan.lunarg.com/)
- x11-dev
- xkbcommon-x11-dev
- xkbcommon-xcb-dev

## MacOs Requirements
Not ready.

## Building
1. From within a local directory, enter the following bash commands:
```
git clone --recursive https://github.com/hoffstadt/PilotLight
```
2. Run build script in respective folder.
  * _build.bat_ for Windows
  * _build.sh_ for Linux
  * _build_mac.sh_ for MacOs
