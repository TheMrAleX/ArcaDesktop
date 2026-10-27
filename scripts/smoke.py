#!/usr/bin/env python3
"""Open all pages and decode a real fixture without touching the user's library."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="arca-smoke-") as tmp:
    media = Path(tmp) / "media"
    media.mkdir()
    ass = media / "Prueba.ass"
    ass.write_text("[Script Info]\nScriptType: v4.00+\nPlayResX: 320\nPlayResY: 180\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\nStyle: Default,DejaVu Sans,18,&H0000FFFF,&H000000FF,&H00000000,&H80000000,-1,0,0,0,100,100,0,0,1,1,1,2,10,10,12,1\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\nDialogue: 0,0:00:00.00,0:00:20.00,Default,,0,0,0,,{\\i1}Arca: subtítulos ASS con estilo\n", encoding="utf-8")
    subprocess.run([
        "ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24",
        "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
        "-f", "lavfi", "-i", "sine=frequency=660:sample_rate=48000", "-i", str(ass),
        "-map", "0:v", "-map", "1:a", "-map", "2:a", "-map", "3:s", "-t", "20",
        "-c:v", "mpeg4", "-pix_fmt", "yuv420p", "-c:a", "aac", "-c:s", "ass",
        "-metadata:s:a:0", "language=eng", "-metadata:s:a:1", "language=spa",
        "-metadata:s:s:0", "language=spa", "-disposition:s:0", "forced", str(media / "Prueba.2024.mkv"),
    ], check=True)
    ass.unlink()
    for season, episode in [(1, 1), (1, 2), (2, 1)]:
        shutil.copyfile(media / "Prueba.2024.mkv", media / f"Serie.S{season:02d}E{episode:02d}.mkv")
    env = os.environ.copy()
    env.setdefault("QT_QPA_PLATFORM", "offscreen")
    env.setdefault("QSG_RHI_BACKEND", "opengl")
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    env["ARCA_TEST_AUDIO_NULL"] = "1"
    env.update(XDG_DATA_HOME=tmp, XDG_CONFIG_HOME=tmp, XDG_CACHE_HOME=tmp, XDG_RUNTIME_DIR=tmp)
    subprocess.run([str(binary), "--smoke-test", str(media)], env=env, check=True, timeout=45)
