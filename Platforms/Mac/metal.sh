#!/bin/sh
# The Metal compiler of the first Xcode whose Metal toolchain is installed: without that downloadable
# component an Xcode's own metal is a stub that only reports the toolchain missing.
for developer in "$DEVELOPER_DIR" "$(xcode-select -p 2>/dev/null)" /Applications/Xcode*.app/Contents/Developer; do
    [ -n "$developer" ] && [ -d "$developer" ] || continue
    compiler=$(DEVELOPER_DIR="$developer" xcrun -f metal 2>/dev/null) || continue
    if DEVELOPER_DIR="$developer" "$compiler" --version >/dev/null 2>&1; then
        DEVELOPER_DIR="$developer" exec "$compiler" "$@"
    fi
done
echo "error: no Xcode with an installed Metal toolchain; run: xcodebuild -downloadComponent MetalToolchain" >&2
exit 1
