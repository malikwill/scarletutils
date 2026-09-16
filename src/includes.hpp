#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/CheckpointObject.hpp>

using namespace geode::prelude;
extern bool menuVisible;

// Gates every diagnostic geode::log::info call this mod makes. Reads the
// "Verbose Logging" mod setting (off by default) rather than caching it, so
// toggling the setting takes effect immediately without needing a restart.
bool verboseLoggingEnabled();

extern bool noDeathEffect;
extern bool hideEndscreen;
extern bool hideNewBest;
extern bool noEffect;

extern bool fadeLevel;
extern double fadeLevelInDuration;
extern double fadeLevelOutDuration;
extern double fadeLevelOutTimeout;

extern bool fadeAudio;
extern double fadeAudioInDuration;
extern double fadeAudioOutDuration;

extern bool straightFlyP1;
extern bool straightFlyP2;
extern double straightFlyThresholdP1;
extern double straightFlyThresholdP2;

extern bool straightUfoP1;
extern bool straightUfoP2;
extern double straightUfoTargetP1;
extern double straightUfoTargetP2;
extern double straightUfoThresholdP1;
extern double straightUfoThresholdP2;

extern cocos2d::ccColor3B layoutModeColorBackground;
extern cocos2d::ccColor3B layoutModeColorGround;

// Flip Input On Death: Wave-mode, practice-mode-only auto-retry helper.
// Every frame while active (and in a wave section, in practice mode, and
// alive), it snapshots the player's current state as a private rolling
// checkpoint of our own — separate from the player's real, manually-placed
// checkpoints, so it never interferes with those. On death, once the level
// resets, it loads that snapshot back (landing within a frame or so of the
// actual death, not frame-perfect, since there's no Silicate-style
// backwards-stepping engine underneath this) and queues the opposite held
// state from whatever was active right before dying, so the retry tries
// the other option automatically. Turning it off just stops updating and
// acting on the snapshot — whatever attempt is in progress continues from
// wherever it currently is.
extern bool flipOnDeath;
extern CheckpointObject* waveFlipCheckpoint;
extern bool waveFlipHeldAtDeath;

extern bool autoSwift;
extern bool extraClick;
extern int  extraClickAmount;

extern bool clickGreenDash;
extern bool clickBlackOrbs;
extern bool clickJumpPads;
extern bool clickGravityPads;
extern bool clickedJumpPad;

extern bool straightFly;
extern bool straightUfo;

extern bool maintainGravity;
extern bool mirrorInput;
extern bool mirrorInputInverted;

extern bool autoclickerP1;
extern bool autoclickerP2;

extern int autoclickerEveryP1;
extern int autoclickerEveryP2;
extern int autoclickerHoldP1;
extern int autoclickerHoldP2;

extern int autoclickerTimerP1;
extern int autoclickerTimerP2;
extern bool autoclickerHoldingP1;
extern bool autoclickerHoldingP2;

extern bool autoclickerSwiftP1;
extern bool autoclickerSwiftP2;

extern bool noclip;
extern bool noclipP1;
extern bool noclipP2;

extern bool restartFirstFrame;

extern bool layoutMode;
extern bool blackOrbUfo;

extern bool releaseGravityOrbsPrevent;
extern bool optimizeStackedOrbs;

extern cocos2d::CCLayerColor* m_startFadeLayer;
extern cocos2d::CCLayerColor* m_endFadeLayer;

std::array<float, 3> colorToFloat(cocos2d::ccColor3B color);
cocos2d::ccColor3B floatToColor(float* col);

extern std::unordered_set<int> decoration;
extern std::unordered_set<int> filter;

extern bool optimizeRingJump;
extern bool optimizeStartDashing;
extern bool optimizeStopDashing;