#!/usr/bin/env bash
set -euo pipefail

if [[ "${2:-}" == ubuntu-24.04 ]]; then
    # Qt 6.4's FFmpeg renderer needs an OpenGL scene graph for VideoOutput.
    # Exercise it on a virtual X11 display with Mesa's software OpenGL driver.
    QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND= QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
        xvfb-run -a python3 scripts/smoke.py "$1"
else
    python3 scripts/smoke.py "$1"
fi
