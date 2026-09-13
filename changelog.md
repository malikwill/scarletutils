# Changelog

# 2.6.0
- Reworked the menu into two separate, independently draggable windows ("Main" and "Visuals") instead of tabs in one window, each with its own animated collapse arrow, closed by default on open
- Added a quick fade in/out animation when opening and closing the menu
- Fixed Maintain Gravity not working in dual mode with a single player
- Fixed Maintain Gravity eating real input entirely while active
- Fixed Maintain Gravity behaving like an uncontrollable autoclicker while holding input — corrections are now edge-triggered on the actual gravity flip instead of re-firing every frame
- Added separate Player 1 / Player 2 toggles for Maintain Gravity
- Fixed several menu options (Level/Audio Fade, Hide Endscreen, Hide New Best) being unavailable on Android for no real reason
- Fixed the level fade-in not replaying on every attempt
- Added a timeout to the level fade-out so the screen doesn't stay stuck black
- Flip Input On Death now detects whether Silicate is installed and available on the current platform, and is disabled with an explanation instead of silently doing nothing
- Fixed a bug where the Windows-only Unfreeze option would send its keyup unconditionally on every death, regardless of whether it was enabled
- Fixed 9 separate item-width leaks in the menu's popups that could corrupt the UI's internal state the longer any of them stayed open
- Fixed the menu window being able to get stuck at an off-screen position with no way back
- Fixed a style-scaling bug that could send the menu window flying off to an unusable position over time
- Resized the menu to be noticeably smaller and wider than tall, with a slightly bigger scrollbar grab area
- Added a "Verbose Logging" setting (off by default) to help diagnose future bugs without spamming the console during normal use
- Removed unused leftover code

# 2.5.1
- Added iOS support
- Fixed maintain gravity not working on mobile
- Fixed respawning while holding a dash orb causing a crash
- Fixed flip on death swift mode

# 2.5.0
- Added mobile support
- Added stacked orb optimization
- Fixed macOS support
- Optimized no effects

# 2.4.2
- New logo
- Added a pause menu button to open the GUI
- Fixed a bug where no effects unlinked dual gravity
- Fixed a bug where layout mode would impede no effects from functioning properly
- Removed some leftover debugging logs

# 2.4.1
- Bugfixes
- A lot of them.

# v2.4.0
- Numerous bugfixes
- Flip Input On Death re-added
- Cosmetic changes

# v2.3.4
- Codebase reorganization

# v2.3.3
- Geode index release