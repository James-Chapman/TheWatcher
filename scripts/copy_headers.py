from pathlib import Path
import shutil
import sys


source = Path(sys.argv[1])
destination = Path(sys.argv[2])

for header in source.rglob("*.h"):
    target = destination / header.relative_to(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(header, target)
