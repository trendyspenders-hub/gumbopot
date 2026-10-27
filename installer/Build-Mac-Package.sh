#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
source_root="${GUMBO_PAYLOAD_ROOT:-Input/GumboPot-Mac}"
work="$(mktemp -d "${TMPDIR:-/tmp}/gumbopot-pkg.XXXXXX")"
mkdir -p Output "$work/au" "$work/vst3" "$work/packages" "$work/resources" "$work/disk"
[[ -d "$source_root/Components/Gumbo Pot.component" && -d "$source_root/VST3/Gumbo Pot.vst3" ]]
grep -q '26 checks, 0 failures' "$source_root/Native-Checks.txt"
grep -q 'AU VALIDATION SUCCEEDED' "$source_root/AU-Validation.txt"
ditto "$source_root/Components/Gumbo Pot.component" "$work/au/Gumbo Pot.component"
ditto "$source_root/VST3/Gumbo Pot.vst3" "$work/vst3/Gumbo Pot.vst3"
for format in au vst3; do
  if [[ "$format" == au ]]; then bundle="$work/au/Gumbo Pot.component"; destination='/Library/Audio/Plug-Ins/Components'; else bundle="$work/vst3/Gumbo Pot.vst3"; destination='/Library/Audio/Plug-Ins/VST3'; fi
  chmod 755 "$bundle/Contents/MacOS/Gumbo Pot"
  codesign --verify --deep --strict "$bundle"
  lipo "$bundle/Contents/MacOS/Gumbo Pot" -verify_arch arm64 x86_64
  pkgbuild --analyze --root "$work/$format" "$work/$format.plist"
  python3 - "$work/$format.plist" <<'PY'
import sys,plistlib
p=sys.argv[1]
with open(p,'rb') as f:entries=plistlib.load(f)
for entry in entries:
 entry['BundleIsRelocatable']=False
 entry['BundleHasStrictIdentifier']=True
 entry['BundleOverwriteAction']='upgrade'
with open(p,'wb') as f:plistlib.dump(entries,f)
PY
  pkgbuild --root "$work/$format" --component-plist "$work/$format.plist" --ownership recommended --identifier "audio.gumbopot.$format" --version 0.3.1 --install-location "$destination" "$work/packages/$format.pkg"
done
cp installer/Welcome.html "$work/resources/Welcome.html"
cp installer/ReadMe.html "$work/resources/ReadMe.html"
productbuild --distribution installer/Distribution.xml --resources "$work/resources" --package-path "$work/packages" 'Output/GumboPot-Installer.pkg'
installer -showChoicesXML -pkg 'Output/GumboPot-Installer.pkg' > Output/Installer-Choices.xml
pkgutil --expand 'Output/GumboPot-Installer.pkg' "$work/expanded"
python3 - "$work/expanded/Distribution" <<'PY'
import sys,xml.etree.ElementTree as E
r=E.parse(sys.argv[1]).getroot()
assert r.find('title').text=='Gumbo Pot'
assert {c.attrib['id'] for c in r.findall('choice')}>={'au','vst3'}
assert len(r.findall('pkg-ref'))>=2
PY
# Test the actual product package using Apple's installer on the disposable CI Mac.
sudo installer -pkg 'Output/GumboPot-Installer.pkg' -target / -verboseR 2>&1 | tee Output/Installer-Test.txt
for format in au vst3; do
  if [[ "$format" == au ]]; then installed='/Library/Audio/Plug-Ins/Components/Gumbo Pot.component'; else installed='/Library/Audio/Plug-Ins/VST3/Gumbo Pot.vst3'; fi
  codesign --verify --deep --strict "$installed"
  lipo "$installed/Contents/MacOS/Gumbo Pot" -verify_arch arm64 x86_64
  cmp "$work/$format/$(basename "$installed")/Contents/MacOS/Gumbo Pot" "$installed/Contents/MacOS/Gumbo Pot"
  pkgutil --pkg-info "audio.gumbopot.$format" | tee -a Output/Installer-Test.txt
  pkgutil --pkg-info "audio.gumbopot.$format" | grep -q 'version: 0.3.1'
done
sudo killall AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -v aumu Gpot Gmbo 2>&1 | tee Output/AU-Validation.txt
cp 'Output/GumboPot-Installer.pkg' "$work/disk/GumboPot-Installer.pkg"
printf '%s\n' 'GUMBO POT' '' 'Double-click GumboPot-Installer.pkg and follow the installation wizard.' 'Close your DAW before installing. Restart it and rescan instruments afterwards.' '' 'Universal AU and VST3 for macOS 11 or newer, Apple Silicon and Intel.' 'The installer installs to the standard system Audio/Plug-Ins folders and asks for administrator approval.' '' 'This package is unsigned and is not Apple notarized. The plugin bundles use ad-hoc signing.' > "$work/disk/Read Me.txt"
hdiutil create -volname 'Gumbo Pot Installer' -srcfolder "$work/disk" -ov -format UDZO 'Output/GumboPot-Installer.dmg'
cp "$source_root/Native-Checks.txt" Output/Native-Checks.txt
cp "$source_root/GumboPot_Native_Interface.png" Output/GumboPot_Native_Interface.png
shasum -a 256 Output/GumboPot-Installer.pkg Output/GumboPot-Installer.dmg > Output/SHA256SUMS.txt
echo 'PASS: standard product installer, both receipts, exact installed binaries, signatures, CPU slices and Apple AU validation.' | tee -a Output/Installer-Test.txt
