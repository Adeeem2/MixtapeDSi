# MixtapeDSi — Lid-close playback

## Status: RESEARCHED, NOT YET VERIFIED ON HARDWARE

## Background

- DS/DSi lid state is readable (`KEY_LID` via `keysDown()` / PM registers, GBATEK #4004xxx power mgmt).
- Stock DS firmware *requests* sleep on lid close, but homebrew running from flashcard/SD only sleeps **if the app calls `systemSleep()` / powers down hardware**. Many DS homebrew players (Moonshell, LMP-ng) keep decoding with the lid closed by simply ignoring the lid event and leaving the sound engine + ARM7 FIFO running.
- 3DS closed-lid audio (ctrmus) relies on 3DS OS/DSP behavior and is **not transferable**; DSi requires the app-level approach above.
- DSi specifics: in DSi mode the SD controller and sound hardware stay powered as long as the app does not enter sleep; headphone jack remains live (speakers muted by jack insert, independent of lid).

## Design in MixtapeDSi

- Main loop **never calls `systemSleep()`**; `KEY_LID` event only dims/ blanks backlights (`powerOff(PM_BACKLIGHT_TOP)` etc.) and suppresses rendering, while `audio_stream_pump()` continues.
- Lid-closed controls (no screen): L/R = prev/next, A = play/pause — documented in README; verified behavior TBD.

## Minimal proof-of-concept procedure (requires DSi + flashcard/TWiLight)

1. Build with devkitARM, copy `mixtapedsi.nds` + one MP3 to SD.
2. Launch, start playback through headphones, note position.
3. Close lid 60 s, reopen: record whether audio continued, paused, faded, or stopped; note backlight state.
4. Repeat in DS mode (flashcard) and DSi-enhanced mode (TWiLight/Unlaunch).

## Tested configuration

| Item | Result |
|---|---|
| Console / firmware | NOT YET TESTED (no hardware in this environment) |
| DS mode | unknown |
| DSi-enhanced mode | unknown |
| Playback survives lid | unknown — predicted YES by design, must verify |
| Mods required | none expected (stock + flashcard/TWiLight) |

## Fallback

If hardware test shows audio stopping (e.g. PMU cuts amp), document as limitation: playback pauses on lid close and resumes on open (position preserved). No unsafe PM/firmware writes will be attempted.
