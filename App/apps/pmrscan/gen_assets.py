#!/usr/bin/env python3
import os, sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from app_assets import Assets

# PMR446 Frequencies in 10 Hz units (Corrected)
PMR_FREQS = [
    44600625, 44601875, 44603125, 44604375,
    44605625, 44606875, 44608125, 44609375,
    44610625, 44611875, 44613125, 44614375,
    44615625, 44616875, 44618125, 44619375
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
