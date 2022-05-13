## Pilot Light (WIP)

This repo is a collection of minimal examples for various graphics APIs. The goals of each example are:
1. Single file.
2. No build system.
3. No abstractions.
4. No dependencies.
5. Demonstrate practical usage.
6. Easy to extend.
7. Easy to follow.
8. Simple flow control.

The purpose of these examples is to provide newcomers with easy-to-follow, easy-to-extend starter examples. Finding decent tutorials to start learning a graphics API can be frustrating. Most tutorials try to teach an API by obscuring it, making it object oriented, and demonstrating unscalable methods of using the it. If you are like me, it pisses you off because you end up spending more time untangling the example/demo framework than learning the API! Hopefully these examples can help others who are annoyed with the lack of straight forward graphics API examples.

### Current Graphics APIs
* Directx 11
* Directx 12
* Vulkan (Windows & Linux)

## Windows Requirements
- [_git_](https://git-scm.com/)
- [_Visual Studio 2019_ or _2022_](https://visualstudio.microsoft.com/vs/) with the following workflows (just for the toolchain):
  * Desktop development with C++
  * Game development with C++
- [Vulkan SDK](https://vulkan.lunarg.com/)
  * Ensure **VK_LAYER_PATH** environment variable is set.

## Linux Requirements
- [_git_](https://git-scm.com/)
- [Vulkan SDK](https://vulkan.lunarg.com/)
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
  * _build.bat_ for **Windows**
  * _build.sh_ for **Linux**
  * _build_mac.sh_ for **MacOs**
