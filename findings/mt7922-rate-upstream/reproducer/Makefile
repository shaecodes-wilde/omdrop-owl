# SPDX-License-Identifier: GPL-2.0-only
MT76_SOURCE ?= $(error Set MT76_SOURCE to the source tree under test)
obj-m += mt7922_rate_test.o
ccflags-y += -I$(MT76_SOURCE) -I$(obj)

$(obj)/mt7922_rate_test.o: $(obj)/driver_functions.inc $(obj)/pci_prepare.inc

targets += driver_functions.inc pci_prepare.inc
quiet_cmd_extract = GEN     $@
      cmd_extract = python3 $(src)/extract-functions.py $(MT76_SOURCE) $(obj)

$(obj)/driver_functions.inc: $(MT76_SOURCE)/mt7921/pci_mac.c $(MT76_SOURCE)/tx.c \
 $(MT76_SOURCE)/mt76_connac_mac.c $(MT76_SOURCE)/mac80211.c $(src)/extract-functions.py FORCE
	$(call if_changed,extract)

$(obj)/pci_prepare.inc: $(obj)/driver_functions.inc
	@test -f $@
