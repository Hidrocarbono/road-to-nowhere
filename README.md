Here is the fully translated version with the updated title.

---

PrimeXT - Road to Nowhere <img align="right" width="128" height="128" src="https://github.com/Hidrocarbono/xash3d-fwgs/raw/master/game_launch/icon-xash-material.png" alt="Xash3D FWGS icon" />

https://github.com/Hidrocarbono/xash3d-fwgs/actions/workflows/c-cpp.yml/badge.svg https://img.shields.io/discord/355697768582610945?logo=Discord&label=International%20Discord%20chat https://img.shields.io/badge/Telegram_chat-gray?logo=Telegram

[!CAUTION]
Download Xash3D FWGS only from official sources. Third-party builds, "modified launchers", "optimized repacks", and random mirrors often come with malware, miners, spyware, and credential stealers. We cannot guarantee anything we haven't built ourselves. Get official binaries only from the releases page.

This fork of PrimeXT is being developed specifically for the Road to Nowhere mod (originally built for Xash3D FWGS), focusing on immersion, visual modernization, and scripting tools for developers.

Key systems and modifications:

· 100% script-driven weapon system — inspired by Uncle Mike's format (Paranoia 2). Add new weapons by placing models in models/ and creating files in scripts/weapons/, without recompiling the source code. Includes support for iron sights, FOV zoom, recoil, and a dynamic HUD (names and icons read directly from VGUI). The network protocol has been expanded to support up to 32 simultaneous weapons.
· Custom fonts and Subtitles — Support for .ttf fonts generated via script (bitmap) with $font and $fontsize directives in titles.txt. Custom subtitle system with colors, dynamic positioning, and automatic replacement of %player_name% with the player's name.
· Post-Processing and Visual Immersion — Lens dirt, vignette, film grain, sunshafts, and bloom. Pulsing bloody HUD, progressive dizziness, and heavy breathing based on damage, plus cinematic death with suspended sounds.
· Engine and Physics Enhancements — Player body (legs) visible when looking down (root cause fix in viewent), func_train with custom sounds, and accent fixes (UTF-8 → cp1252) for Portuguese text.
· Full Compatibility — All build fixes (ODR, memory corruption, crash when re-giving weapons) and the addition of the 9 entities missing from primext.fgd for mapping.

Installation

You can read the detailed installation guide on our documentation site: available in English and Russian.

Technical Notes

Implementation details, discovered engine pitfalls, and the roadmap for remaining features are documented in the source code (comments in pt_br) and in CHANGELOG_AGENT.md at the repository root for future developers. These modifications are being developed with Claude and Hermes, running on DeepSeek V4 Flash.