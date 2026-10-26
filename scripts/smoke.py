#!/usr/bin/env python3
"""Open all pages and decode a real fixture without touching the user's library."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="arca-smoke-") as tmp:
    media = Path(tmp) / "media"
    media.mkdir()
    subprocess.run([
        "ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24",
        "-t", "10", "-c:v", "mpeg4", "-pix_fmt", "yuv420p", str(media / "Prueba.2024.mp4"),
    ], check=True)
    env = os.environ.copy()
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", QT_MEDIA_BACKEND="ffmpeg",
               XDG_DATA_HOME=tmp, XDG_CONFIG_HOME=tmp, XDG_CACHE_HOME=tmp, XDG_RUNTIME_DIR=tmp)
    subprocess.run([str(binary), "--smoke-test", str(media)], env=env, check=True, timeout=45)
