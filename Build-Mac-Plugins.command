#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
[[ "$(uname -s)" == Darwin ]] || { echo 'VST3/AU Mac binaries must be built on macOS.'; exit 1; }
xcode-select -p >/dev/null || { echo 'Install Apple Command Line Tools: xcode-select --install'; exit 1; }
command -v cmake >/dev/null || { echo 'Install CMake 3.22+ (for example: brew install cmake), then run again.'; exit 1; }
if [[ ! -d JUCE ]]; then git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE; fi
[[ "$(git -C JUCE rev-parse HEAD)" == 51d11a2be6d5c97ccf12b4e5e827006e19f0555a ]] || { echo 'JUCE must be exactly version 8.0.4. Move the JUCE folder aside and retry.'; exit 1; }
mkdir -p Output
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DGUMBO_BUILD_TESTS=ON -DGUMBO_BUILD_STANDALONE=OFF
cmake --build build-mac --config Release --target GumboPot_VST3 GumboPot_AU GumboNativeChecks --parallel 2
gumbo_checks="$(find build-mac/GumboNativeChecks_artefacts -type f \( -name GumboNativeChecks -o -name 'Gumbo Native Checks' \) -perm -111 -print -quit)"
[[ -n "$gumbo_checks" ]] || { echo 'Native check executable was not produced.'; exit 1; }
"$gumbo_checks" "$PWD/Output/GumboPot_Native_Interface.png" | tee Output/Native-Checks.txt
gumbo_vst='build-mac/GumboPot_artefacts/Release/VST3/Gumbo Pot.vst3'
gumbo_au='build-mac/GumboPot_artefacts/Release/AU/Gumbo Pot.component'
for gumbo_bundle in "$gumbo_vst" "$gumbo_au"; do
  [[ -d "$gumbo_bundle" ]] || { echo "Missing build output: $gumbo_bundle"; exit 1; }
  codesign --force --deep --sign - "$gumbo_bundle"
  codesign --verify --deep --strict "$gumbo_bundle"
  gumbo_binary="$gumbo_bundle/Contents/MacOS/Gumbo Pot"
  lipo "$gumbo_binary" -verify_arch arm64 x86_64
done
gumbo_stage='Output/GumboPot-Mac'
mkdir -p "$gumbo_stage/VST3" "$gumbo_stage/Components"
ditto "$gumbo_vst" "$gumbo_stage/VST3/Gumbo Pot.vst3"
ditto "$gumbo_au" "$gumbo_stage/Components/Gumbo Pot.component"
cp scripts/Install-Mac.command "$gumbo_stage/Install-Mac.command"
cp INSTALL_NATIVE.md "$gumbo_stage/READ-ME.md"
cp Output/GumboPot_Native_Interface.png "$gumbo_stage/GumboPot_Native_Interface.png"
cp Output/Native-Checks.txt "$gumbo_stage/Native-Checks.txt"
bash "$gumbo_stage/Install-Mac.command"
killall -u "$(id -un)" AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -a 2>&1 | tee Output/AU-Components.txt
if ! grep -Eq 'aumu[[:space:]]+Gpot[[:space:]]+Gmbo' Output/AU-Components.txt && [[ "${GITHUB_ACTIONS:-false}" == true ]]; then
  sudo mkdir -p /Library/Audio/Plug-Ins/Components
  sudo ditto "$gumbo_stage/Components/Gumbo Pot.component" '/Library/Audio/Plug-Ins/Components/Gumbo Pot.component'
  sudo killall AudioComponentRegistrar 2>/dev/null || true
  sleep 2
  auval -a 2>&1 | tee Output/AU-Components.txt
fi
auval -v aumu Gpot Gmbo 2>&1 | tee Output/AU-Validation.txt
cp Output/AU-Validation.txt "$gumbo_stage/AU-Validation.txt"
hdiutil create -volname 'Gumbo Pot' -srcfolder "$gumbo_stage" -ov -format UDZO 'Output/GumboPot-Mac-universal.dmg'
ditto -c -k --sequesterRsrc --keepParent "$gumbo_stage" 'Output/GumboPot-Mac-universal.zip'
echo 'Built and locally validated universal Mac VST3/AU. DMG and ZIP are in Output/.'
echo 'The bundles use local ad-hoc signing. Developer ID signing and notarization are separate distribution steps.'
