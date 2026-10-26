#!/usr/bin/env bash
set -euo pipefail

if [[ "${2:-}" == ubuntu-24.04 ]]; then
    # Exercise Qt 6.4's stable backend on a real scene graph and virtual display.
    QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND= QT_MEDIA_BACKEND=gstreamer QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
        xvfb-run -a python3 scripts/smoke.py "$1"
else
    python3 scripts/smoke.py "$1"
fi
