from pathlib import Path
import sys


root = Path(sys.argv[1])
for source in sorted((root / "src" / "libsodium").rglob("*.c")):
    print(source.relative_to(root).as_posix())
