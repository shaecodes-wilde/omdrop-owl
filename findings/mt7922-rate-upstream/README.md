# MT7920/MT7921/MT7922 injected legacy rate fix

Updated 2026-10-14. This report covers the original cleaned fix and its later family-guard revision for the injected legacy rate failure discussed in [Omdrop issue 14](https://github.com/t4t5/omdrop-owl/issues/14). It is included in the default [`master` branch of shaecodes-wilde/mt76](https://github.com/shaecodes-wilde/mt76/tree/master). It has not been accepted into upstream mt76 or released in a standard kernel.

[Original MT7922-only driver revision: 02b6c9a1ef87e55f2ccf90739e5dbcab55390cc7](https://github.com/shaecodes-wilde/mt76/commit/02b6c9a1ef87e55f2ccf90739e5dbcab55390cc7), based on openwrt/mt76 `467ae2e3f190baf737e15e318906da0dc7d0ab91`.

## What changed since the first experiment

TX status bookkeeping clears storage that overlaps `info->control.flags`, erasing `IEEE80211_TX_CTRL_RATE_INJECT` before the common descriptor builder reads it. The PCI path now saves the requested legacy hardware rate first. It uses the existing common fixed-rate setup, restores the packet's original `USE_MINRATE` flag, and replaces only the descriptor rate field.

This retains the current driver's antenna/SPE handling and ACK policy. The original cleaned change was restricted to MT7922 and validates the selected band and legacy rate index. The later family-guard revision below changes only the model selection. Hardware encapsulation and HT/VHT requests are excluded. The local module-version marker and Omdrop channel-selection workaround are excluded from the kernel patch.

## Family guard revision, 2026-10-14

[Driver revision `2a2f085dc1f920cabf8b8bc18686b9f974b27dac`](https://github.com/shaecodes-wilde/mt76/commit/2a2f085dc1f920cabf8b8bc18686b9f974b27dac) widens the existing guard to MT7920, MT7921 (`mt76_chip() == 0x7961`), and MT7922. The rest of the cleaned PCI fix is unchanged. There is no `is_mt7921()` helper in this source. MT7902 is deliberately excluded even though it also uses this PCI driver; removing the guard entirely would broaden scope to that card.

The expanded reproducer contains 72 cases: 23 descriptor cases for each of MT7920/MT7921/MT7922, two MT7902 exclusion cases, and an unrelated-chip exclusion case. It retains rate/band validation, ordinary traffic, encapsulation and HT/VHT exclusions, flag restoration, ACK, and antenna comparisons. Synthetic 6 GHz cases exercise descriptor handling and do not establish that every chip supports that band on hardware. Logs now identify the chip for each case. The old MT7921 exclusion case has been replaced with positive MT7921 coverage.

Current validation: the PCI source object and expanded test module compile without warnings against `7.2.5-3-omarchy` headers; driver and test diffs pass checkpatch with zero errors and warnings (`--no-tree --no-signoff`). A standalone check extracted the actual guard expression and its source helpers, compiled them, and checked every 16-bit chip ID; only the intended three were selected. See [build details and hashes](family-validation.json).

**The expanded kernel test module has not been executed.** Loading it requires password-authenticated sudo, unavailable in this session. No live driver was replaced and no new phone transfers were performed. Exact-revision hardware confirmation remains pending. MT7920 has no hardware test evidence. The original 24-case results below are historical results for `02b6c9a1`, not pass results for this 72-case suite.

The follow-up commit links the independent MT7921 report. Before a cumulative upstream submission, revise the original MT7922-only description and verify the earliest affected commit: its existing MT7922-addition `Fixes:` tag does not by itself describe the older MT7921 failure. No human DCO sign-off has been added and no upstream mailing has been made.

## Earlier MT7922-only offline validation

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

Shared mode, channel 44: the 727,402-byte PNG upload completed in 3.69 seconds and the user confirmed it arrived and opened. Wi-Fi stayed associated and passed 10/10 ping checks while Omdrop was active. An incoming 2,801,190-byte PNG completed in 4.16 seconds, fully decoded, and the user confirmed success. These are individual transfer observations, not a performance benchmark.

Teardown restored the original driver, profile, and Wi-Fi connection and removed the temporary test connection. Wi-Fi passed 10/10 ping checks after restoration; Ethernet remained the default internet route. Reboot validation is pending. A temporary local Omdrop configuration allowed channel-44 validation without adding channel policy to the kernel patch. That initial validation loaded the cleaned driver temporarily; merging the fork does not install it automatically.

The phone's exact iOS version remains unverified; the previously supplied version string is not treated as confirmed.

## Persistent local setup after validation

The cleaned PCI function is now installed on the test computer as DKMS `omdrop-mt7921e/7.2.5-airdrop2`, restricted to `7.2.5-3-omarchy`. Both disk and loaded modules report srcversion `D5CFC98B9B40208307ABC3F`. Omdrop uses shared mode with a persistent channel-44 Wi-Fi connection; Ethernet remains the default internet route. The older driver and profile are retained for local rollback. The boot image was inspected and contains no embedded mt7921e module. A reboot has not yet been tested.

A post-install 727,402-byte PNG upload completed in 2.63 seconds and the user confirmed it arrived without issue. An incoming 63,738-byte JPEG fully decoded at 1085×814 and the user confirmed Sent. Final shared-mode Wi-Fi checks passed 10/10 pings. The setup was left enabled rather than restored to the older active-monitor experiment.

A first incoming post-install file was saved intact, but the phone stayed on Sending: the receiver returned HTTP 200 and left the connection open until its 30-second idle timeout. A separate local Omdrop receiver change adds `Connection: close` after a successful upload, retaining the existing drain for a delayed closing chunk. A loopback regression reproduced the missing close response; protocol/limits checks and targeted completion/wait/port checks passed after the existing keepalive test was updated for explicit closure. The next incoming phone transfer reported Sent. This receiver change is local to the Omdrop plugin and is not part of the mt76 patch or an upstream plugin release.

## Additional recording and PDF transfers

Further incoming transfers succeeded in shared mode on channel 44 with the persistent cleaned driver and the separate local receiver completion change described above:

| Content | Saved size | Upload time | Average file throughput | Verification |
| --- | --- | --- | --- | --- |
| 44 min 28.608 s M4A/AAC recording | 43,170,107 bytes (43.17 MB) | 30.181 s | 1.43 MB/s | Sender-reported size matched; full audio stream decoded without errors; user reported Sent |
| Nine-page PDF | 2,896,475 bytes (2.90 MB) | 9.760 s | 0.30 MB/s | Sender-reported size matched; PDF parsed successfully |

Timing runs from the receiver's `/Upload` handler entry to its HTTP 200 completion response. Throughput uses saved file bytes; these are individual observations rather than a performance benchmark. See [sanitized measurements](large-transfer-results.json). Personal filenames and file contents are omitted. This adds larger-file and additional-format evidence on the same hardware; reboot and longer reliability testing remain pending.

## Independent evidence

[verusferro reported](https://github.com/t4t5/omdrop-owl/issues/14#issuecomment-6177921979) successful transfers using `02b6c9a1` with the MT7922 check removed on MT7921 PCIe `14c3:7961`, `mt7921e`, CachyOS kernel 7.2.9. Only mt7921e was rebuilt; the other mt76 components stayed stock. Active monitor, channel 44, iPad: 58,976,364 bytes received in 37.0 seconds and 8,223,915 bytes sent in 20.3 seconds. Both returned HTTP 200; the received video decoded cleanly. This is independent hardware evidence for the cleaned rate fix with a different guard, not confirmation of the exact new revision.

[t4t5 reported](https://github.com/t4t5/omdrop-owl/issues/14#issuecomment-6171706287) successful transfers in both directions, in active-monitor and shared modes, on another MT7922 variant (`14c3:0616`). That report applies to the [earlier experimental patch](../mt7922-channel44/README.md), not this cleaned revision. That MT7922 report remains distinct from the later MT7921 test of the cleaned revision with its guard removed.

## Reproducing the offline check

Use a disposable test system or VM with matching kernel build headers and cfg80211 available. The test is synthetic; it cannot establish radio reliability or performance. Review the module source before loading it.

Create separate unpatched and patched mt76 source trees from the pinned base above, then build one module at a time:

```sh
make -C /lib/modules/$(uname -r)/build M="$PWD/reproducer" MT76_SOURCE=/absolute/path/to/mt76 modules
sudo insmod ./reproducer/mt7922_rate_test.ko
sudo dmesg | tail -80
sudo rmmod mt7922_rate_test
```

An unpatched baseline is expected to fail module initialization with `EINVAL`; it is not left loaded, so omit `rmmod` in that case. The expanded suite is expected to log 72 PASS cases and `0 failures` on a passing family-patched tree, which has not yet been confirmed by execution; unload it afterward. Clean the build directory between source variants. Do not load either PCI component build into a kernel with incompatible mt76 components.

No reboot, long-run reliability, complete current-kernel build, MT7920 hardware support, or exact-revision hardware validation of the widened guard is claimed. This work used an AI coding assistant; the original patch carries `Assisted-by: LLM`, and the family follow-up uses `Assisted-by: Codex:gpt-6.1-sol [exec_command]` to satisfy the installed checkpatch format. A human must review it and certify the DCO before mailing it upstream.
