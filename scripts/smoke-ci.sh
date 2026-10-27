#!/usr/bin/env bash
set -euo pipefail
QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND= QSG_RHI_BACKEND=opengl LIBGL_ALWAYS_SOFTWARE=1 \
    xvfb-run -a dbus-run-session -- python3 scripts/smoke.py "$1"
