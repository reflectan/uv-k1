#!/usr/bin/env python3
import os, sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from app_assets import Assets

# PMR446 Frequencies in 10 Hz units
PMR_FREQS = [
    446006250, 446018750, 446031250, 446043750,
    446056250, 446068750, 446081250, 446093750,
    446106250, 446118750, 446131250, 446143750,
    446156250, 446168750, 446181250, 446193750
]

def build():
    a = Assets("PMRSCAN")
    # UI Strings
    a.text("T_TITLE", "PMR446 SCANNER")
    a.text("T_SCANNING", "Scanning...")
    a.text("T_PAUSED", "Paused")
    
    # 32-bit frequency array
    a.u32("PMR_FREQS", PMR_FREQS)
    
    # Constants
    a.const("PMR_CHANNELS", len(PMR_FREQS))
    return a

if __name__ == "__main__":
    build().main()
