#!/usr/bin/env python3
"""Preserve command output and expose bounded diagnostics in Actions annotations."""
import subprocess
import sys

result = subprocess.run(sys.argv[1:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
output = result.stdout.decode(errors="replace")
print(output, end="", flush=True)
if result.returncode:
    message = output[-6000:].replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    print("::error title=Error de compilación o prueba::" + message, flush=True)
sys.exit(result.returncode)
