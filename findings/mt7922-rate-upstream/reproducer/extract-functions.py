#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile the selected driver's real PCI/status/TXWI routines in isolation."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])


def section(filename, start, end):
    text = (root / filename).read_text()
    if text.count(start) != 1 or text.count(end) != 1:
        raise SystemExit(f"Review extraction boundaries in {filename}")
    return text[text.index(start):text.index(end)]


rates = section("mac80211.c", "struct ieee80211_rate mt76_rates[] = {",
                "EXPORT_SYMBOL_GPL(mt76_rates);")
status = section("tx.c", "int\nmt76_tx_status_skb_add(",
                 "EXPORT_SYMBOL_GPL(mt76_tx_status_skb_add);")
common = section("mt76_connac_mac.c", "#define __bitrate_mask_check(",
                 "EXPORT_SYMBOL_GPL(mt76_connac2_mac_write_txwi);")
common = common.replace("EXPORT_SYMBOL_GPL(mt76_connac2_mac_tx_rate_val);", "")
output.joinpath("driver_functions.inc").write_text(rates + status + common)
pci = section("mt7921/pci_mac.c", "int mt7921e_tx_prepare_skb(",
              "\nint mt7921e_mac_reset(")
output.joinpath("pci_prepare.inc").write_text("static " + pci)
