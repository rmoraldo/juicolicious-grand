#!/bin/bash
# builds the macOS installer from a finished Release build. run it on a Mac from the project root,
# after building into build-release, with the version number as the only argument:
#   bash installer/mac/build-pkg.sh 0.2.0
# the finished installer ends up in build-pkg.

# this stops the script at the first command that fails, instead of carrying on and making a
# broken installer.
set -euo pipefail

VERSION="${1:?Give the version number, for example: bash installer/mac/build-pkg.sh 0.2.0}"
NAME="Juicolicious Grand"
ID="com.rayhanmoraldo.juicoliciousgrand"
ARTEFACTS="build-release/JuicoliciousGrandPiano_artefacts/Release"
MAC_DIR="installer/mac"
WORK="build-pkg"
OUTPUT="$WORK/JuicoliciousGrand-$VERSION-macOS.pkg"

rm -rf "$WORK"
mkdir -p "$WORK/roots/vst3" "$WORK/roots/au" "$WORK/roots/app" "$WORK/roots/samples" "$WORK/components"

# each part gets its own folder, holding exactly what goes into that part's install location.
# ditto is the Mac copy command that keeps bundles and their code signatures intact.
ditto "$ARTEFACTS/VST3/$NAME.vst3" "$WORK/roots/vst3/$NAME.vst3"
ditto "$ARTEFACTS/AU/$NAME.component" "$WORK/roots/au/$NAME.component"
ditto "$ARTEFACTS/Standalone/$NAME.app" "$WORK/roots/app/$NAME.app"
ditto "Samples" "$WORK/roots/samples/Samples"

# Apple Silicon Macs refuse to load code with no signature at all. "-" signs each bundle ad hoc,
# which needs no Apple developer account. it doesn't remove the unsigned installer warning, but it
# lets the plugin and app load once installed.
for bundle in "$WORK/roots/vst3/$NAME.vst3" "$WORK/roots/au/$NAME.component" "$WORK/roots/app/$NAME.app"; do
    codesign --force --deep --sign - "$bundle"
    codesign --verify --deep --strict "$bundle"
done

# Git on Windows doesn't keep the "executable" flag on files, so the install scripts get it back here.
chmod +x "$MAC_DIR"/scripts/*/*

# this builds one small package for one part. the main installer bundles them together below.
build_component() {
    local part="$1"
    local location="$2"
    local plist="$WORK/$part.plist"
    # this list holds the optional pkgbuild settings for this part. the odd looking
    # ${extra[@]+"${extra[@]}"} below passes them on, and still works when the list is empty.
    local extra=()

    # by default macOS installs a bundle over any older copy it finds anywhere on the disk, even
    # outside the plugin folders. this turns that off, so each part always lands in its standard folder.
    # plutil -replace sets the setting whether or not pkgbuild wrote it into the list.
    pkgbuild --analyze --root "$WORK/roots/$part" "$plist" > /dev/null
    if /usr/libexec/PlistBuddy -c "Print :0" "$plist" > /dev/null 2>&1; then
        plutil -replace 0.BundleIsRelocatable -bool NO "$plist"
        extra+=(--component-plist "$plist")
    fi

    if [ -d "$MAC_DIR/scripts/$part" ]; then
        extra+=(--scripts "$MAC_DIR/scripts/$part")
    fi

    pkgbuild --root "$WORK/roots/$part" \
        --install-location "$location" \
        --identifier "$ID.$part" \
        --version "$VERSION" \
        ${extra[@]+"${extra[@]}"} \
        "$WORK/components/$part.pkg"
}

build_component vst3 "/Library/Audio/Plug-Ins/VST3"
build_component au "/Library/Audio/Plug-Ins/Components"
build_component app "/Applications"
build_component samples "/Library/Application Support/JuicoliciousGrand"

# distribution.xml describes the installer window: its title, the parts listed under Customize,
# and the macOS version check. the version number is filled in here.
sed "s/__VERSION__/$VERSION/g" "$MAC_DIR/distribution.xml" > "$WORK/distribution.xml"

productbuild --distribution "$WORK/distribution.xml" \
    --package-path "$WORK/components" \
    --resources "$MAC_DIR/resources" \
    "$OUTPUT"

echo "Built $OUTPUT"
