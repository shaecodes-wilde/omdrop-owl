// SPDX-License-Identifier: GPL-2.0-only
/* Exercise PCI preparation and real status bookkeeping without radio I/O. */
#include <linux/module.h>
/* All mt76 routines below are compiled from the selected source tree.
 * No running driver's internal structures or exported functions are used. */
#define mt76_rates offline_mt76_rates
#define mt76_tx_status_skb_add offline_tx_status_skb_add
#define mt76_connac2_mac_tx_rate_val offline_mac_tx_rate_val
#define mt76_connac2_mac_write_txwi offline_mac_write_txwi
#include "mt7921/mt7921.h"
#include "mt76_connac2_mac.h"
#include "driver_functions.inc"

static int test_token_consume(struct mt76_dev *dev, struct mt76_txwi_cache **t)
{
	return 0;
}
static void test_write_txp(struct mt76_dev *dev, struct mt76_tx_info *info,
			   void *ptr, u32 id) {}
#define mt76_token_consume test_token_consume
#define mt76_connac_write_hw_txp test_write_txp
#define mt7921e_tx_prepare_skb test_prepare_skb
#include "pci_prepare.inc"
#undef mt7921e_tx_prepare_skb
#undef mt76_connac_write_hw_txp
#undef mt76_token_consume

static const struct mt76_driver_ops driver = { .txwi_size = 128 };
static const struct mt76_bus_ops bus = { .type = MT76_BUS_MMIO };

static int run_case(struct mt76_dev *dev, const char *name, int band, int idx,
		    u16 rate_flags, bool injected, bool requested, bool multicast,
		    bool expected_fixed, u16 expected_rate, u32 extra_flags)
{
	struct mt76_wcid wcid = {};
	struct sk_buff *skb;
	struct ieee80211_hdr *hdr;
	struct ieee80211_tx_info *info;
	__le32 txwi[128] = {};
	__le32 reference[128] = {};
	struct mt76_tx_info tx_info = {};
	u32 original_flags;
	u16 rate;
	bool fixed;
	int rc;

	skb = alloc_skb(128, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;
	hdr = skb_put_zero(skb, 64);
	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA);
	hdr->addr1[0] = multicast ? 0x01 : 0x02;
	info = IEEE80211_SKB_CB(skb);
	info->band = band;
	info->flags |= extra_flags;
	info->control.rates[0].idx = idx;
	info->control.rates[0].flags = rate_flags;
	if (injected)
		info->flags |= IEEE80211_TX_CTL_INJECTED;
	if (requested)
		info->control.flags |= IEEE80211_TX_CTRL_RATE_INJECT;

	original_flags = info->flags;
	tx_info.skb = skb;
	rc = test_prepare_skb(dev, txwi, MT_TXQ_BE, &wcid, NULL, &tx_info);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}
	fixed = !!(le32_to_cpu(txwi[2]) & MT_TXD2_FIX_RATE);
	rate = FIELD_GET(MT_TXD6_TX_RATE, le32_to_cpu(txwi[6]));
	rc = fixed != expected_fixed || (expected_fixed && rate != expected_rate) ||
	     info->control.flags != 0 || info->flags != original_flags;
	/* Compare every common fixed-rate setting, allowing only the rate to differ. */
	if (expected_fixed) {
		int word, pid;

		pid = mt76_tx_status_skb_add(dev, &wcid, skb);
		info->flags |= IEEE80211_TX_CTL_USE_MINRATE;
		mt76_connac2_mac_write_txwi(dev, reference, skb, &wcid, NULL,
					   pid, MT_TXQ_BE, 0);
		info->flags = original_flags;
		for (word = 0; word < MT_TXD_SIZE / sizeof(*reference); word++) {
			u32 difference = le32_to_cpu(reference[word] ^ txwi[word]);

			if (word == 6)
				difference &= ~MT_TXD6_TX_RATE;
			if (difference)
				pr_info("mt7922_rate_test: %s word=%d got=%#x reference=%#x\n",
					name, word, le32_to_cpu(txwi[word]),
					le32_to_cpu(reference[word]));
			rc |= !!difference;
		}
	}
	pr_info("mt7922_rate_test: %s fixed=%u rate=%#x expected=%u/%#x %s\n",
		name, fixed, rate, expected_fixed, expected_rate, rc ? "FAIL" : "PASS");
	kfree_skb(skb);
	return rc;
}

static int __init mt7922_rate_test_init(void)
{
	struct mt792x_dev *radio;
	struct mt76_dev *dev;
	struct ieee80211_hw *hw;
	struct wiphy *wiphy;
	struct ieee80211_supported_band band2 = {
		.bitrates = mt76_rates, .n_bitrates = 12,
	};
	struct ieee80211_supported_band band5 = {
		.bitrates = mt76_rates + 4, .n_bitrates = 8,
	};
	int failed = 0;
	/* OFDM 12 Mbps: mode 1, hardware rate index 10. */
	u16 ofdm12 = FIELD_PREP(MT_TX_RATE_MODE, MT_PHY_TYPE_OFDM) |
		     FIELD_PREP(MT_TX_RATE_IDX, 10);

	radio = kzalloc(sizeof(*radio), GFP_KERNEL);
	dev = radio ? &radio->mt76 : NULL;
	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	wiphy = kzalloc(sizeof(*wiphy), GFP_KERNEL);
	if (!dev || !hw || !wiphy) {
		failed = -ENOMEM;
		goto out;
	}
	dev->bus = &bus;
	dev->drv = &driver;
	dev->rev = 0x7922U << 16;
	dev->phy.hw = hw;
	dev->phy.antenna_mask = 3;
	hw->wiphy = wiphy;
	wiphy->bands[NL80211_BAND_2GHZ] = &band2;
	wiphy->bands[NL80211_BAND_5GHZ] = &band5;
	wiphy->bands[NL80211_BAND_6GHZ] = &band5;

#define CHECK(name, band, idx, flags, injected, requested, multicast, fixed, rate) \
	failed += !!run_case(dev, name, band, idx, flags, injected, requested, \
			     multicast, fixed, rate, 0)
	CHECK("injected-12Mbps-2GHz", 0, 6, 0, true, true, false, true, ofdm12);
	CHECK("injected-12Mbps-5GHz", 1, 2, 0, true, true, false, true, ofdm12);
	CHECK("injected-multicast-12Mbps", 0, 6, 0, true, true, true, true, ofdm12);
	CHECK("injected-6Mbps", 0, 4, 0, true, true, false, true, 0x4b);
	CHECK("injected-54Mbps", 1, 7, 0, true, true, false, true, 0x4c);
	CHECK("injected-CCK-short-preamble", 0, 1, IEEE80211_TX_RC_USE_SHORT_PREAMBLE,
	      true, true, false, true, 5);
	failed += !!run_case(dev, "8023-encapsulation-unchanged", 0, 6, 0,
			    true, true, false, false, 0,
			    IEEE80211_TX_CTL_HW_80211_ENCAP);
	CHECK("invalid-band", NUM_NL80211_BANDS, 6, 0,
	      true, true, false, false, 0);
	failed += !!run_case(dev, "injected-minrate-flag-preserved", 0, 6, 0,
			    true, true, false, true, ofdm12,
			    IEEE80211_TX_CTL_USE_MINRATE);
	failed += !!run_case(dev, "ordinary-minrate-unchanged", 0, 6, 0,
			    false, false, false, true, 0,
			    IEEE80211_TX_CTL_USE_MINRATE);
	CHECK("ordinary-unicast", 0, 6, 0, false, false, false, false, 0);
	CHECK("ordinary-multicast", 0, 6, 0, false, false, true, true, 0);
	CHECK("injected-no-rate", 0, 6, 0, true, false, false, false, 0);
	CHECK("requested-not-injected", 0, 6, 0, false, true, false, false, 0);
	CHECK("negative-index", 0, -1, 0, true, true, false, false, 0);
	CHECK("index-out-of-range", 0, 12, 0, true, true, false, false, 0);
	CHECK("missing-band", 2, 6, 0, true, true, false, false, 0);
	CHECK("HT-unchanged", 0, 6, IEEE80211_TX_RC_MCS, true, true, false, false, 0);
	CHECK("VHT-unchanged", 0, 6, IEEE80211_TX_RC_VHT_MCS, true, true, false, false, 0);
	CHECK("injected-12Mbps-6GHz", NL80211_BAND_6GHZ, 2, 0,
	      true, true, false, true, ofdm12);
	failed += !!run_case(dev, "noack-policy-preserved", 1, 2, 0,
			    true, true, false, true, ofdm12,
			    IEEE80211_TX_CTL_NO_ACK);
	dev->phy.antenna_mask = 1;
	CHECK("antenna-path-0-preserved", 1, 2, 0, true, true, false, true, ofdm12);
	dev->phy.antenna_mask = 2;
	CHECK("antenna-path-1-preserved", 1, 2, 0, true, true, false, true, ofdm12);
	dev->phy.antenna_mask = 3;
	dev->rev = 0x7961U << 16;
	CHECK("other-chip-unchanged", 0, 6, 0, true, true, false, false, 0);
	pr_info("mt7922_rate_test: %d failures\n", failed);
out:
	kfree(wiphy);
	kfree(hw);
	kfree(radio);
	return failed ? -EINVAL : 0;
}

static void __exit mt7922_rate_test_exit(void) {}
module_init(mt7922_rate_test_init);
module_exit(mt7922_rate_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Offline MT7922 injected legacy rate descriptor regression test");
