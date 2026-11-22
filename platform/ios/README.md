# Optica for iOS

The iOS app is the SDL3 + Dear ImGui version (`platform/sdl`), built by CMake's Xcode
generator. This folder holds what only iOS needs: `Info.plist.in` (landscape, full
screen, launch screen) and the app icon (`Assets.xcassets`).

## Building

Needs a Mac with Xcode, CMake 3.24 or newer and Git (CMake clones SDL3 and Dear ImGui
from GitHub the first time). From the project folder:

    cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
    open build-ios/Optica.xcodeproj

For the iOS Simulator, add `-DCMAKE_OSX_SYSROOT=iphonesimulator` to the first command.

In Xcode, choose the **optica** scheme, then a simulator or your connected iPhone or iPad,
and press Run.

## Signing (only for a real device)

Xcode signs the app with your Apple team. Either pass it to CMake with
`-DOPTICA_IOS_TEAM=<your team ID>`, or select the **optica** target in Xcode, open
**Signing & Capabilities** and choose your team.

- A free Apple ID can install the app on your own device; it stops opening after 7 days
  and must be run from Xcode again.
- The Apple Developer Program is needed for TestFlight and the App Store.

The bundle identifier is `com.example.optica`: change it with
`-DOPTICA_BUNDLE_ID=<your identifier>` before signing for a device or publishing (Apple
requires one that is unique to your team).

Set the oldest iOS version with `-DCMAKE_OSX_DEPLOYMENT_TARGET=<version>` (default 15.0).
