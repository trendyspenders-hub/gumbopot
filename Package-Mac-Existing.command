#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ "$(uname -s)" == Darwin ]] || { echo 'This packaging step requires macOS.'; exit 1; }
gumbo_recovered="${1:-recovered}"
[[ -d "$gumbo_recovered/GumboPot-Mac" ]] || { echo 'Download the completed Mac build artifact into recovered/ first.'; exit 1; }
grep -E '[0-9]+ checks, 0 failures' "$gumbo_recovered/Native-Checks.txt"
mkdir -p Output
gumbo_stage='Output/GumboPot-Mac'
ditto "$gumbo_recovered/GumboPot-Mac" "$gumbo_stage"
cp "$gumbo_recovered/Native-Checks.txt" Output/Native-Checks.txt
cp "$gumbo_recovered/GumboPot_Native_Interface.png" Output/GumboPot_Native_Interface.png
chmod +x "$gumbo_stage/Install-Mac.command"
for gumbo_bundle in "$gumbo_stage/VST3/Gumbo Pot.vst3" "$gumbo_stage/Components/Gumbo Pot.component"; do
  chmod +x "$gumbo_bundle/Contents/MacOS/Gumbo Pot"
  codesign --force --deep --sign - "$gumbo_bundle"
  codesign --verify --deep --strict "$gumbo_bundle"
  lipo "$gumbo_bundle/Contents/MacOS/Gumbo Pot" -verify_arch arm64 x86_64
done
bash "$gumbo_stage/Install-Mac.command"
# Refresh this runner user's registration daemon after introducing a new AU bundle.
killall -u "$(id -un)" AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -a 2>&1 | tee Output/AU-Components.txt
auval -v aumu Gpot Gmbo 2>&1 | tee Output/AU-Validation.txt
cp Output/AU-Validation.txt "$gumbo_stage/AU-Validation.txt"
hdiutil create -volname 'Gumbo Pot' -srcfolder "$gumbo_stage" -ov -format UDZO Output/GumboPot-Mac-universal.dmg
ditto -c -k --sequesterRsrc --keepParent "$gumbo_stage" Output/GumboPot-Mac-universal.zip
echo 'Universal Mac plugins verified, AU validated, DMG and ZIP packaged.'
