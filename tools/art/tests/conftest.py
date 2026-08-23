from __future__ import annotations

import sys
from pathlib import Path


ART_DIR = Path(__file__).resolve().parents[1]
if str(ART_DIR) not in sys.path:
    sys.path.insert(0, str(ART_DIR))
