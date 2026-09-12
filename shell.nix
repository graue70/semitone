{ pkgs ? import <nixpkgs> {} }:
with pkgs;
let
  buildToolsVersion = "36.0.0";
  cmakeVersion = "3.31.1";
  ndkVersion = "28.2.13676358";
  androidComposition = androidenv.composeAndroidPackages {
    includeEmulator = false;
    includeSources = false;
    includeSystemImages = false;
    useGoogleAPIs = false;
    useGoogleTVAddOns = false;
    includeNDK = true;
    ndkVersions = [ndkVersion];
    platformVersions = ["36"];
    buildToolsVersions = [buildToolsVersion];
    cmakeVersions = [cmakeVersion];
  };
in mkShell rec {
  ANDROID_SDK_ROOT = "${androidComposition.androidsdk}/libexec/android-sdk";
  ANDROID_NDK_ROOT = "${ANDROID_SDK_ROOT}/ndk/${ndkVersion}";
  ANDROID_NDK = ANDROID_NDK_ROOT;
  GRADLE_OPTS = "-Dorg.gradle.project.android.aapt2FromMavenOverride=${ANDROID_SDK_ROOT}/build-tools/${buildToolsVersion}/aapt2";
  shellHook = ''
    export PATH="$(echo "$ANDROID_SDK_ROOT/cmake/${cmakeVersion}".*/bin):$PATH"
  '';
}
