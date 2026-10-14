# MT7922 injected legacy rate fix: upstream review revision

Prepared 2026-10-14. This revision addresses the injected legacy rate failure discussed in [Omdrop issue 14](https://github.com/t4t5/omdrop-owl/issues/14). It is a driver patch for review, not a released kernel fix.

[Exact driver revision: 02b6c9a1ef87e55f2ccf90739e5dbcab55390cc7](https://github.com/shaecodes-wilde/mt76/commit/02b6c9a1ef87e55f2ccf90739e5dbcab55390cc7), based on openwrt/mt76 `467ae2e3f190baf737e15e318906da0dc7d0ab91`.

## What changed since the first experiment

TX status bookkeeping clears storage that overlaps `info->control.flags`, erasing `IEEE80211_TX_CTRL_RATE_INJECT` before the common descriptor builder reads it. The PCI path now saves the requested legacy hardware rate first. It uses the existing common fixed-rate setup, restores the packet's original `USE_MINRATE` flag, and replaces only the descriptor rate field.

This retains the current driver's antenna/SPE handling and ACK policy. The change is restricted to MT7922 and validates the selected band and legacy rate index. Hardware encapsulation and HT/VHT requests are excluded. The local module-version marker and Omdrop channel-selection workaround are excluded from the kernel patch.

## Offline validation

The included module compiles the selected source tree's actual PCI preparation, status bookkeeping, common TXWI builder, and rate table. Token allocation and DMA payload construction are stubbed. It does not use the running driver's internal ABI or transmit any radio traffic.

Twenty-four cases cover injected rates on 2.4/5/6 GHz, CCK short preamble, flag restoration, invalid/missing rates, ordinary traffic, hardware encapsulation, HT/VHT exclusions, another chip, ACK policy, and antenna paths. Full fixed-rate descriptors are compared with the common builder's output, allowing only the selected rate to differ.

| Source | Unpatched | Cleaned patch |
| --- | --- | --- |
| Current openwrt/mt76 base above | 11 failures / 24 cases | 24 passed / 0 failures |
| Verified Linux 7.2.5 sources | 11 failures / 24 cases | 24 passed / 0 failures |
| Linux maintainer mt76 target `2e6968e9286f` | 11 failures / 24 cases | 24 passed / 0 failures |

All six test modules were executed on `7.2.5-3-omarchy` and unloaded afterward. See [regression results](regression-results.txt) and [source/module hashes](source-hashes.json).

The current and matching-7.2.5 PCI components build without warnings against the installed 7.2.5 headers. A complete build of the current mt76 stack against those older headers is not validated: unmodified current code requires the newer `ieee80211_txq_aql_pending()` API. This is not a complete current-kernel build or hardware test.

The Linux-path patch applies to `nbd168/wireless:mt76` at `2e6968e9286f3573016b9f804d4945373a21c489`. Its PCI and common TXWI source files match the openwrt base above. That target's checkpatch reports zero errors and zero warnings with `--no-tree --no-signoff`; human DCO sign-off is still required.

## Hardware validation of this cleaned revision

Hardware: MT7922 PCI `14c3:7922`, subsystem `1a3b:5916`; iPhone 16 Pro Max. Kernel: `7.2.5-3-omarchy`. The cleaned PCI function was compiled against matching sources and temporarily loaded without a local module-version marker. Existing 7.2.5 common driver components were retained.

Active-monitor mode, channel 44: the user confirmed three incoming files completed (two PNGs and one MP4, 27,391,614 bytes total). The receiver returned HTTP 200 in about 16.73 seconds, and all files fully decoded. Sending a 727,402-byte PNG returned HTTP 200 in 14.47 seconds; the user confirmed it arrived and opened on the phone.

Shared mode, channel 44: the 727,402-byte PNG upload completed in 3.69 seconds and the user confirmed it arrived and opened. Wi-Fi stayed associated and passed 10/10 ping checks while Omdrop was active. An incoming 2,801,190-byte PNG completed in 4.16 seconds, fully decoded, and the user confirmed success. Teardown restored the original driver, profile, and Wi-Fi connection and removed the temporary test connection. Wi-Fi passed 10/10 ping checks after restoration; Ethernet remained the default route. Reboot validation is pending. Ethernet remained the default internet route. A temporary local Omdrop configuration allows channel-44 validation without adding channel policy to the kernel patch.

The phone's exact iOS version remains unverified; the previously supplied version string is not treated as confirmed.

## Independent evidence

[t4t5 reported](https://github.com/t4t5/omdrop-owl/issues/14#issuecomment-6171706287) successful transfers in both directions, in active-monitor and shared modes, on another MT7922 variant (`14c3:0616`). That report applies to the [earlier experimental patch](../mt7922-channel44/README.md), not this cleaned revision. A retest and formal Tested-by line for this exact revision are requested separately.

## Reproducing the offline check

Use a disposable test system or VM with matching kernel build headers and cfg80211 available. The test is synthetic; it cannot establish radio reliability or performance. Review the module source before loading it.

Create separate unpatched and patched mt76 source trees from the pinned base above, then build one module at a time:

```sh
make -C /lib/modules/$(uname -r)/build M="$PWD/reproducer" MT76_SOURCE=/absolute/path/to/mt76 modules
sudo insmod ./reproducer/mt7922_rate_test.ko
sudo dmesg | tail -80
sudo rmmod mt7922_rate_test
```

The baseline intentionally fails module initialization with `EINVAL` and logs 11 failures; it is not left loaded, so omit `rmmod` in that case. A passing patched module initializes normally and logs 24 PASS cases and `0 failures`; unload it afterward. Clean the build directory between source variants. Do not load either PCI component build into a kernel with incompatible mt76 components.

No reboot, long-run reliability, complete current-kernel build, or additional-chip support is claimed. This work used an AI coding assistant; the patch carries `Assisted-by: LLM` as required by the selected kernel tree's guidance. A human must review it and certify the DCO before mailing it upstream.
