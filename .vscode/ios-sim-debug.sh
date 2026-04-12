#!/bin/bash
# Boots simulator, installs and launches PetStory, attaches lldb debugger.
set -e

APP_PATH="$1"
BUNDLE_ID="com.petstory.game"

# Shut down all sims, boot the target one
xcrun simctl shutdown all 2>/dev/null || true
xcrun simctl boot 'iPhone 16 Pro'
open -a Simulator

# Wait for simulator to be ready
xcrun simctl bootstatus 'iPhone 16 Pro' -b 2>/dev/null || true

# Terminate previous instance, install, launch
xcrun simctl terminate booted "$BUNDLE_ID" 2>/dev/null || true
xcrun simctl install booted "$APP_PATH"
PID=$(xcrun simctl launch booted "$BUNDLE_ID" 2>&1 | grep -oE '[0-9]+$')

echo "=== PetStory launched on simulator, PID: $PID ==="
echo "=== Attaching lldb... ==="

# Attach lldb with symbols
exec xcrun lldb \
    -o "platform select ios-simulator" \
    -o "target create \"${APP_PATH}/PetStory\"" \
    -o "process attach --pid $PID" \
    -o "command script import $(dirname "$0")/../o2/Framework/Platforms/o2_lldb_formatters.py" \
    -o "continue"
