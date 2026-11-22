# Optica for Android

Gradle project for the SDL3 + Dear ImGui version of Optica. The native code is the
project's own CMake build (`../CMakeLists.txt`, front end `platform/sdl`); the Java side
(`app/src/main/java/org/libsdl`) and the Gradle wrapper come from SDL 3.4.8's
`android-project` template and must stay at the SDL version set in
`platform/sdl/CMakeLists.txt`.

## Building the APK

Needs Android Studio (or the Android SDK command-line tools) and Git on the PATH: the
CMake build clones SDL3 and Dear ImGui from GitHub the first time.

1. Android Studio: **File > Open**, choose this `android` folder. Accept installing the
   missing SDK Platform 35, NDK 28.2.13676358 and CMake if asked.
2. **Build > Build App Bundle(s) / APK(s) > Build APK(s)**.
3. The APK is `app/build/outputs/apk/debug/app-debug.apk` (signed with the debug key,
   so it installs on a phone with "install unknown apps" allowed, or with
   `adb install app/build/outputs/apk/debug/app-debug.apk`).

From a command line instead: `gradlew assembleDebug` in this folder, with `ANDROID_HOME`
set to the Android SDK.

The application id is `com.example.optica`: change it in `app/build.gradle` before
publishing.
