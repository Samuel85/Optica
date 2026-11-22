# Building Optica

Optica has two front ends over the same portable core (`core/`: the optics model, the
editing rules and their unit tests):

| Front end | Folder | Platforms |
|---|---|---|
| Win32 + GDI | `platform/windows` | Windows |
| SDL3 + Dear ImGui | `platform/sdl` | Linux, Android, iOS (and Windows, untested) |

For building the application CMake is needed. The SDL front end downloads its two libraries itself:
* [SDL3](https://github.com/libsdl-org/SDL) 3.4.8,
* [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b.

Both libraries will be downloaded automatically from Github.

All platforms need a **C++20 compiler**. The versions below are the ones the [continuous integration](#continuous-integration-and-releases) builds with; older ones may work but are not tested.

- [Windows](#windows)
- [Linux](#linux)
- [Android](#android)
- [iOS](#ios)
- [Build options](#build-options)
- [Continuous integration and releases](#continuous-integration-and-releases)

## Windows

### Dependencies

- **Visual Studio 2022 or newer** with the *Desktop development with C++* workload. It
  includes the MSVC compiler and CMake. The free Community edition is enough.

### Build

From a *Developer PowerShell for VS* (or any terminal where `cmake` is found), in the project folder:

```
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

The program will be located at `build\Release\optica.exe`.

You can also open the project folder directly in Visual Studio (*File > Open > Folder*) and select the project `optica` as a target and compile.

On Windows the default front end is the Win32 + GDI one. The SDL front end can be chosen with `-DOPTICA_FRONTEND=SDL` (see [Build options](#build-options)); it is not tested on Windows.

## Linux

### Dependencies

A C++20 compiler (GCC 11 or newer, or Clang), CMake 3.22 or newer, Git, and the X11 and OpenGL development files SDL3 builds against.

Ubuntu (tested on Ubuntu 24.04):

```
sudo apt-get update
sudo apt-get install build-essential cmake git ninja-build \
    libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev \
    libxss-dev libxtst-dev libxfixes-dev libgl-dev libegl-dev
```

```
sudo dnf install gcc-c++ cmake git ninja-build \
    libX11-devel libXext-devel libXcursor-devel libXi-devel libXrandr-devel \
    libXScrnSaver-devel libXtst-devel libXfixes-devel mesa-libGL-devel mesa-libEGL-devel
```

Using `ninja-build` is optional. If this is not installed remove `-G Ninja` on the following instructions.

### Build

In the project folder:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

The program will be located in `build/platform/sdl/optica`. It needs no SDL installed to run: SDL is linked into it and loads X11 and OpenGL when it starts.

## Android

The Android project is in `android/`. Gradle builds the same CMake project with the SDL
front end, for 64-bit ARM phones and tablets (`arm64-v8a`) and the emulator (`x86_64`).

### Dependencies

- **JDK 17**
- **Android SDK** with:
  - SDK Platform 35 (`platforms;android-35`)
  - NDK 28.2.13676358 (`ndk;28.2.13676358`)
  - CMake 3.22.1 (`cmake;3.22.1`)
- **Git**, which CMake uses to download SDL3 and Dear ImGui. On Windows it must be on
  the `PATH` that Android Studio sees.

**With Android Studio** (the easiest): install
[Android Studio](https://developer.android.com/studio); it includes a JDK and the SDK
manager. Open the `android` folder with *File > Open*; when it asks to install the
missing SDK platform, NDK or CMake, accept.

**From the command line**: install a JDK 17 (e.g. `sudo apt-get install openjdk-17-jdk`,
or `winget install EclipseAdoptium.Temurin.17.JDK`), download the Android
[command-line tools](https://developer.android.com/studio#command-line-tools-only), set
`ANDROID_HOME` to the SDK folder, and install the packages:

```
sdkmanager --licenses
sdkmanager --install "platforms;android-35" "ndk;28.2.13676358" "cmake;3.22.1"
```

### Build

In Android Studio: *Build > Build App Bundle(s) / APK(s) > Build APK(s)*.

From the command line, in the `android` folder:

```
bash ./gradlew assembleDebug   # Linux and macOS
gradlew assembleDebug          # Windows
```

The APK will be located in `android/app/build/outputs/apk/debug/app-debug.apk`. It is signed with the
debug key, so it installs on a device that allows installing unknown apps, or with
`adb install android/app/build/outputs/apk/debug/app-debug.apk`.

The application id is `com.example.optica`: change it in `android/app/build.gradle`
before publishing.

## iOS

The iOS app is built from an Xcode project that CMake generates. `platform/ios/` holds
the app's `Info.plist` and icon.

### Dependencies

A Mac with:

- **Xcode**, from the App Store. Open it once to install its components.
- **CMake 3.24 or newer**: `brew install cmake` with [Homebrew](https://brew.sh), or the
  installer from [cmake.org](https://cmake.org/download/).
- **Git**: included with Xcode's command-line tools (`xcode-select --install`).

### Build

In the project folder:

```
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
open build-ios/Optica.xcodeproj
```

In Xcode, choose the **optica** scheme and a simulator or a connected iPhone or iPad,
then *Product > Run*.

For the iOS Simulator only, add `-DCMAKE_OSX_SYSROOT=iphonesimulator` to the first
command.

The oldest iOS version the app runs on is 15.0; change it with
`-DCMAKE_OSX_DEPLOYMENT_TARGET=<version>`.

### Signing (for a real device)

Xcode signs the app with your Apple team: pass it to CMake with
`-DOPTICA_IOS_TEAM=<team ID>`, or select the **optica** target in Xcode and choose the
team under *Signing & Capabilities*.

- A free Apple ID can install the app on your own device; it stops opening after 7 days.
- The Apple Developer Program is needed for TestFlight and the App Store.

The bundle identifier is `com.example.optica`: change it with
`-DOPTICA_BUNDLE_ID=<identifier>` before signing (Apple requires one unique to your team).

Unsigned builds, as the continuous integration makes them:

```
cmake --build build-ios --config Release -- CODE_SIGNING_ALLOWED=NO
```

## Build options

Passed to the first `cmake` command as `-D<option>=<value>`:

| Option | Values | Default |
|---|---|---|
| `OPTICA_FRONTEND` | `GDI` (Windows only), `SDL`, `NONE` (only the core and its tests) | `GDI` on Windows, `SDL` elsewhere |
| `OPTICA_BUNDLE_ID` | iOS bundle identifier | `com.example.optica` |
| `OPTICA_IOS_TEAM` | Apple development team ID | empty: choose it in Xcode |
| `FETCHCONTENT_SOURCE_DIR_SDL3` | a local copy of SDL 3.4.8, to build offline | downloaded |
| `FETCHCONTENT_SOURCE_DIR_IMGUI` | a local copy of Dear ImGui 1.92.9b, to build offline | downloaded |

On Linux and Windows, an SDL3 already installed on the system (3.2 or newer) is used
instead of downloading it, when CMake finds it.

The unit tests (`ctest`) are built for Windows and Linux, not for Android or iOS.