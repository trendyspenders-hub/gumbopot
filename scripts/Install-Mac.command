#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
[[ "$(uname -s)" == Darwin ]] || { echo 'This installer is for macOS.'; exit 1; }
gumbo_backup="$HOME/Library/Application Support/Gumbo Pot/Backups/$(date +%Y%m%d-%H%M%S)"
for gumbo_format in VST3 Components; do
  gumbo_name='Gumbo Pot.vst3'
  [[ "$gumbo_format" == Components ]] && gumbo_name='Gumbo Pot.component'
  gumbo_source="$gumbo_format/$gumbo_name"
  [[ -d "$gumbo_source" ]] || { echo "Missing built plugin: $gumbo_source"; exit 1; }
  gumbo_target="$HOME/Library/Audio/Plug-Ins/$gumbo_format"
  mkdir -p "$gumbo_target"
  if [[ -e "$gumbo_target/$gumbo_name" ]]; then
    mkdir -p "$gumbo_backup/$gumbo_format"
    mv "$gumbo_target/$gumbo_name" "$gumbo_backup/$gumbo_format/$gumbo_name"
  fi
  ditto "$gumbo_source" "$gumbo_target/$gumbo_name"
  codesign --verify --deep --strict "$gumbo_target/$gumbo_name"
done
echo 'Installed Gumbo Pot VST3 and AU for this user. Restart your DAW and rescan instruments.'
echo 'AU check: auval -v aumu Gpot Gmbo'
