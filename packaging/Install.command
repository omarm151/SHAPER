#!/bin/bash
# Illuminize Shaper installer for macOS.
# Copies the VST3 and AU into your user plug-in folders and clears the download
# quarantine flag (the plugin is not notarized by Apple).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

echo "Installing Illuminize Shaper..."
rm -rf "$VST3_DIR/Illuminize Shaper.vst3" "$AU_DIR/Illuminize Shaper.component"
cp -R "$HERE/Illuminize Shaper.vst3" "$VST3_DIR/"
cp -R "$HERE/Illuminize Shaper.component" "$AU_DIR/"
xattr -dr com.apple.quarantine "$VST3_DIR/Illuminize Shaper.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine "$AU_DIR/Illuminize Shaper.component" 2>/dev/null || true
# Ask macOS to re-scan Audio Units.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Done."
echo "  VST3: $VST3_DIR/Illuminize Shaper.vst3"
echo "  AU:   $AU_DIR/Illuminize Shaper.component"
echo
echo "In Ableton Live: Settings > Plug-Ins > turn on 'Use VST3 Plug-In System Folders'"
echo "and 'Use Audio Units', then click Rescan. Look under Plug-Ins > Illuminize."
