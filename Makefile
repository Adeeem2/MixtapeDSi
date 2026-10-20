# MixtapeDSi top-level Makefile (official devkitARM combined template).
# Build inside devkitPro MSYS2 shell:
#   source /etc/profile.d/devkit-env.sh
#   make
# Host tests (Windows, no toolchain needed): py run_tests.py
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

export TARGET := mixtapedsi
export TOPDIR := $(CURDIR)

# Game icon for the NDS banner (leave blank for default icon)
GAME_ICON := icon.bmp

# These set the information text in the nds file
GAME_TITLE     := MixtapeDSi
GAME_SUBTITLE1 := DSi music player
GAME_SUBTITLE2 := v1.0.7

# Homebrew marker for 2020-era launchers. TWiLight Menu++ v15 (and
# nds-bootstrap v0.35.x) only classify a ROM as homebrew when the ARM9 entry
# starts with the libnds 1.x startup signature. libnds 2.x replaced that with
# the bootstub, so the ROM gets misdetected as a retail game and
# nds-bootstrap fails with "An error has occurred". TWiLight's second
# homebrew marker is the header title "NDS.TinyFB"; set it with ndstool -g.
GAME_CODE    := MXDX
MAKER_CODE   := 00
HEADER_TITLE := NDS.TinyFB

include $(DEVKITARM)/ds_rules

.PHONY: checkarm7 checkarm9 clean fetch arm7/$(TARGET).elf arm9/$(TARGET).elf

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: fetch checkarm7 checkarm9 $(TARGET).nds

fetch:
	@sh tools/fetch_helix.sh 2>/dev/null || true

#---------------------------------------------------------------------------------
checkarm7:
	$(MAKE) -C arm7

#---------------------------------------------------------------------------------
checkarm9:
	$(MAKE) -C arm9

#---------------------------------------------------------------------------------
$(TARGET).nds : arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool -c $(TARGET).nds -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
	-b $(GAME_ICON) "$(GAME_TITLE);$(GAME_SUBTITLE1);$(GAME_SUBTITLE2)" \
	-g $(GAME_CODE) $(MAKER_CODE) "$(HEADER_TITLE)"

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	$(MAKE) -C arm7

#---------------------------------------------------------------------------------
arm9/$(TARGET).elf:
	$(MAKE) -C arm9

#---------------------------------------------------------------------------------
clean:
	$(MAKE) -C arm9 clean
	$(MAKE) -C arm7 clean
	rm -f $(TARGET).nds
