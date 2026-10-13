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
GAME_SUBTITLE2 := mixtapedsi

include $(DEVKITARM)/ds_rules

.PHONY: checkarm7 checkarm9 clean fetch

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
	-b $(GAME_ICON) "$(GAME_TITLE);$(GAME_SUBTITLE1);$(GAME_SUBTITLE2)"

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
