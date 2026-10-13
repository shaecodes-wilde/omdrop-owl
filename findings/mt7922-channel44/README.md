# MT7922 AirDrop findings: working transfers on channel 44

We found a fix that made AirDrop work both ways on one MT7922 computer. This records the patch and results already tested, so other people can inspect or reproduce them.

This builds on [kmod-midori's active-monitor work (#13)](https://github.com/t4t5/omdrop-owl/pull/13) and [t4t5's integration (#24)](https://github.com/t4t5/omdrop-owl/pull/24), and addresses the MT7922 sending-rate failure discussed in [issue #14](https://github.com/t4t5/omdrop-owl/issues/14). It does not claim to fix all devices or modes in that thread.

## Tested setup

- MT7922 PCI `14c3:7922`, subsystem `1a3b:5916`, ASIC revision `79220010`, driver `mt7921e`.
- Omarchy kernel `7.2.5-3-omarchy`. Matching driver sources: Linux v7.2.5, commit `a300e35c0a4b4a38fb53742ea6e2a203c98ee523` in `gregkh/linux`.
- Omdrop plugin 0.16.1; backend 0.1.1 with #24's active-monitor support and the channel-selection adjustment below.
- iPhone 16 Pro Max; iOS `26.6.61` as reported by the tester.
- Wi-Fi used as an active monitor on channel 44; Ethernet provided internet. Runtime power management was disabled for the Omdrop window and restored afterward.

## What was wrong

The Wi-Fi driver was losing the requested sending speed while preparing packets. Specifically, `mt76_tx_status_skb_add()` clears status-driver storage that overlaps `info->control.flags`, before `mt76_connac2_mac_write_txwi()` runs.

A first attempt checked the requested-rate flag only inside the common descriptor builder. An isolated test passed, but that change did not take effect on real packets because the flag had already been cleared.

The working patch saves the requested legacy rate in `mt7921e_tx_prepare_skb()` before that bookkeeping, then sets the final fixed-rate descriptor afterward. It is guarded to MT7922 and excludes HT/VHT requests and invalid rate indices/bands.

Live driver traces showed the requested 12 Mbps descriptor value (`0x4a`) on announcements and unicast after the PCI fix. These were descriptor observations, not independent over-the-air rate measurements. Channel 6 remained unreliable; channel 44 produced successful transfers.

## What succeeded

- A 3,444,343-byte HEIC photo arrived from the iPhone in about six seconds. The saved file had the expected size and fully decoded at 5712×4284.
- Another 362,816-byte JPEG arrived after reloading the installed driver and restarting Omdrop. It fully decoded.
- The JPEG was accepted and sent back to the same iPhone successfully in 11.92 seconds.
- A later incoming M4A audio file (561,938 bytes, AAC, 48 kHz stereo, 34.8 seconds) completed its upload in approximately 1.15 seconds. The saved size matched the sender’s reported size, and the full audio stream decoded without errors. The recording and its filename are omitted.
- The original PCI regression test failed both requested 12 Mbps cases before the fix and passed all twelve cases afterward. It exercised real status bookkeeping and descriptor construction, with only token allocation and DMA payload assembly stubbed.
- Wi-Fi checks after reload/teardown had no packet loss (five packets per check). Ethernet stayed the default internet route.
- Receiver upload-rate and timeout protections stayed enabled.

## Patch files

[linux-7.2.5-mt7922-tested.patch](linux-7.2.5-mt7922-tested.patch) records the installed experimental driver change against Linux v7.2.5. It includes the local `7.2.5-airdrop1` module-version marker used by the channel-selection guard.

[active-monitor-tested-channel-selection.patch](active-monitor-tested-channel-selection.patch) is the companion change against #24's `MT7922` branch. It tries channels 44, 149, then 6 only when that marked module is loaded; regulatory channel filtering remains intact. The default stays on channel 6 without the marked module. The active-monitor profile used was:

```text
mt7921e untested active-monitor station no yes MT7922 local rate experiment
```

The untested hardware opt-in was enabled. These files document the working experiment; they are not an automatic installer or a merge-ready upstream kernel patch. Source/build ABI matching matters when replacing this driver, and the tested marker must not be treated as proof of support on another kernel.

For an upstream submission, omit the local marker and preserve the common builder's full fixed-rate setup. Current mt76 has additional antenna/SPE handling that this older backport does not reproduce for newly forced unicast. A refinement to reuse that common setup was prepared and compiled, but additional runtime checks were set aside and it is not the hardware-tested patch linked here.

## Limits

This is one computer and one running kernel. A reboot, longer sessions, large transfers, other kernel versions, the `14c3:0616` MT7922 variant, and other chipsets still need testing. The iOS version above is the tester's report, not an independently verified screenshot.

The current kernel/build artifacts, personal photos, receiver keys, sender tokens, Wi-Fi credentials, device names/addresses, and machine-specific installation scripts are not included.
