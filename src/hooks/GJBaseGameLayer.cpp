#include "../includes.hpp"
#include "../updater/updater.hpp"
#include "Geode/DefaultInclude.hpp"
#include <Geode/Enums.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerButtonCommand.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

void runMaintainGravity() {
    auto bgl = GJBaseGameLayer::get();
    if (maintainGravity) {
        // The correction below is only meaningful right at the moment
        // gravity actually flips — that's the one instant GD's own hold
        // tracking can get out of sync with reality. Previously this ran
        // unconditionally every single frame the raw comparison looked
        // mismatched, which isn't the same thing: while the player is
        // genuinely holding input, that comparison can stay "mismatched"
        // for reasons unrelated to a real flip, so it kept re-queuing a
        // button toggle frame after frame — indistinguishable from an
        // autoclicker firing on its own the moment you touched the input.
        // Tracking the previous isUpsideDown value and only correcting on
        // the frame it actually changes fixes that: it fires once per real
        // flip, exactly the case this feature exists for, and does nothing
        // at all the rest of the time regardless of whether you're holding.
        static bool prevUpsideDownP1 = bgl->m_player1->m_isUpsideDown;
        static bool prevUpsideDownP2 = bgl->m_player2->m_isUpsideDown;
        bool flippedP1 = bgl->m_player1->m_isUpsideDown != prevUpsideDownP1;
        bool flippedP2 = bgl->m_player2->m_isUpsideDown != prevUpsideDownP2;
        prevUpsideDownP1 = bgl->m_player1->m_isUpsideDown;
        prevUpsideDownP2 = bgl->m_player2->m_isUpsideDown;

        bool p1maintain = bgl->m_player1->m_holdingButtons[1] != bgl->m_player1->m_isUpsideDown;
        bool p2maintain = bgl->m_player2->m_holdingButtons[1] != bgl->m_player2->m_isUpsideDown;

        bool p1holding = bgl->m_uiLayer->m_p1Jumping || bgl->m_uiLayer->m_p1TouchId != -1;
        bool p2holding = bgl->m_uiLayer->m_p2Jumping || bgl->m_uiLayer->m_p2TouchId != -1;

        if (GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls))
            std::swap(p1holding, p2holding);

        // Only clear the queue when we're actually about to replace it with
        // our own correction below — clearing it unconditionally every
        // frame (as before) wiped out real button presses on every frame
        // that wasn't a flip frame, since nothing was queued back in their
        // place. Corrections are now edge-triggered (see above), so most
        // frames need to leave the queue completely untouched.
        bool willCorrectP1 = maintainGravityP1 && flippedP1 &&
            (p1holding || (autoclickerHoldingP1 && autoclickerP1)) != p1maintain;
        bool willCorrectP2 = maintainGravityP2 && flippedP2 &&
            (p2holding || (autoclickerHoldingP2 && autoclickerP2)) != p2maintain &&
            bgl->m_gameState.m_isDualMode && bgl->m_levelSettings->m_twoPlayerMode;

        if (willCorrectP1 || willCorrectP2)
            bgl->m_queuedButtons.clear();

        if (willCorrectP1) {
            bgl->queueButton((int)PlayerButton::Jump, !bgl->m_player1->m_holdingButtons[1],
            GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
            autoclickerTimerP1 = INT32_MAX;
        }

        if (willCorrectP2) {
            bgl->queueButton((int)PlayerButton::Jump, !bgl->m_player2->m_holdingButtons[1],
            !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
            autoclickerTimerP2 = INT32_MAX;
        }
    }
}

void runMirrorInput() {
    auto bgl = GJBaseGameLayer::get();
    if (mirrorInput) {
        // Same reasoning as Maintain Gravity above: only fire the instant
        // player 1's own input actually changes (press or release), never
        // continuously — a continuous version would eat real input the
        // exact same way Maintain Gravity's original version did.
        static bool prevP1Holding = false;

        bool p1Holding = bgl->m_uiLayer->m_p1Jumping || bgl->m_uiLayer->m_p1TouchId != -1;
        bool p2HoldingRaw = bgl->m_uiLayer->m_p2Jumping || bgl->m_uiLayer->m_p2TouchId != -1;
        if (GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls))
            std::swap(p1Holding, p2HoldingRaw);

        bool changed = p1Holding != prevP1Holding;
        prevP1Holding = p1Holding;

        if (changed) {
            bool targetP2Hold = mirrorInputInverted ? !p1Holding : p1Holding;
            // Note: this shares the same queue as Maintain Gravity's
            // correction above. Both are edge-triggered on genuinely rare,
            // narrow events (a gravity flip, or a real press/release), so
            // the two clearing each other out in the same single tick is an
            // accepted, very unlikely edge case rather than something
            // worth restructuring both functions around.
            bgl->m_queuedButtons.clear();
            bgl->queueButton((int)PlayerButton::Jump, targetP2Hold,
                !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
        }
    }
}

class $modify(ScarletGJBaseGameLayer, GJBaseGameLayer) {
    void playExitDualEffect(PlayerObject* player) {
        if (!(m_playerDied && noDeathEffect || noEffect))
        GJBaseGameLayer::playExitDualEffect(player);
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        runMaintainGravity();
        runMirrorInput();
        bool didReleaseGravityOrb = false;
        auto copy = m_queuedButtons;
        for (auto button : copy) {
            auto player = button.m_isPlayer2 ^ GameManager::sharedState()->getGameVariable(
            GameVar::Flip2PlayerControls) ? m_player2 : m_player1;

            if (autoSwift || releaseGravityOrbsPrevent) {
                m_queuedButtons.erase(
                    std::remove_if(m_queuedButtons.begin(), m_queuedButtons.end(),
                    [](auto input) { return !input.m_isPush; }),
                    m_queuedButtons.end()
                );
                releaseGravityOrbsPrevent = false;
            }

            if (button.m_isPush) {
                PlayerButtonCommand fakeInput;
                fakeInput.m_isPlayer2 = button.m_isPlayer2;
                fakeInput.m_button = PlayerButton::Jump;
                fakeInput.m_step = 0;
                fakeInput.m_timestamp = 0.0;
                auto index = player->m_touchingRings->count();

                for (auto i = 0; i < index; i++) {
                    auto orb = static_cast<RingObject*>(player->m_touchingRings->objectAtIndex(i));
                    if (orb->m_objectType == GameObjectType::DashRing && clickGreenDash) {
                        fakeInput.m_isPush = false;
                        #ifndef GEODE_IS_ANDROID
                        m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                        #else
                        m_queuedButtons.push_back(fakeInput);
                        for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                            std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                        #endif

                        fakeInput.m_isPush = true;
                        #ifndef GEODE_IS_ANDROID
                        m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                        #else
                        m_queuedButtons.push_back(fakeInput);
                        for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                            std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                        #endif
                    } else break;
                }

                for (auto i = 0; i < index; i++) {
                    auto orb = static_cast<RingObject*>(player->m_touchingRings->objectAtIndex(i));
                    if (orb->m_objectType == GameObjectType::DropRing && (blackOrbUfo || straightUfo || clickBlackOrbs)) {
                        if ((player->m_yVelocity <= 0 && !player->m_isUpsideDown) ||
                            (player->m_yVelocity >= 0 && player->m_isUpsideDown) ||
                            clickBlackOrbs) {
                            fakeInput.m_isPush = false;
                            #ifndef GEODE_IS_ANDROID
                            m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                            #else
                            m_queuedButtons.push_back(fakeInput);
                            for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                                std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                            #endif
                                
                                fakeInput.m_isPush = true;
                            #ifndef GEODE_IS_ANDROID
                            m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                            #else
                            m_queuedButtons.push_back(fakeInput);
                            for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                                std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                            #endif
                        }
                    } else break;
                }

                for (auto i = 0; i < index; i++) {
                    auto orb = static_cast<RingObject*>(player->m_touchingRings->objectAtIndex(i));
                    if ((orb->m_objectType == GameObjectType::GravityDashRing ||
                        orb->m_objectType == GameObjectType::GravityRing ||
                        orb->m_objectType == GameObjectType::GreenRing) &&
                        maintainGravity && !didReleaseGravityOrb) {

                            fakeInput.m_isPush = false;
                            m_queuedButtons.push_back(fakeInput);
                            didReleaseGravityOrb = true;
                            releaseGravityOrbsPrevent = true;
                    }
                }

                if (extraClick) {
                    for (int i = 0; i < extraClickAmount; i++) {
                        fakeInput.m_isPush = false;
                        #ifndef GEODE_IS_ANDROID
                        m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                        #else
                        m_queuedButtons.push_back(fakeInput);
                        for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                            std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                        #endif
                        
                        fakeInput.m_isPush = true;
                        #ifndef GEODE_IS_ANDROID
                        m_queuedButtons.insert(m_queuedButtons.begin(), fakeInput);
                        #else
                        m_queuedButtons.push_back(fakeInput);
                        for (size_t i = m_queuedButtons.size() - 1; i > 0; --i) // workaround for geode bug
                            std::swap(m_queuedButtons[i], m_queuedButtons[i - 1]);
                        #endif
                    }
                }

                if (autoSwift && m_queuedButtons.back().m_isPush) {
                    fakeInput.m_isPush = false;
                    m_queuedButtons.push_back(fakeInput);
                }
            }
        }
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }

    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("GJBaseGameLayer::processQueuedButtons", Priority::First)) {
            geode::log::warn("Failed to set hook priority.");
        }
    }

    void updateColor(
        ccColor3B& color, float fadeTime, int colorID, bool blending, float opacity,
        ccHSVValue& copyHSV, int colorIDToCopy, bool copyOpacity,
        EffectGameObject* callerObject, int unk1, int unk2
    ) {
        if (!PlayLayer::get() || !layoutMode)
            return GJBaseGameLayer::updateColor(
                color, fadeTime, colorID, blending, opacity, copyHSV,
                colorIDToCopy, copyOpacity, callerObject, unk1, unk2
            );

        switch (colorID) {
            case 1000: { // BG
                color = layoutModeColorBackground;
                break;
            }
            case 1001: { // G1
                color = layoutModeColorGround;
                break;
            }
            default: {
                color = {255, 255, 255};
                break;
            }
        }
        GJBaseGameLayer::updateColor(
            color, fadeTime, colorID, blending, opacity, copyHSV,
            colorIDToCopy, copyOpacity, callerObject, unk1, unk2
        );
    }

    void createBackground(int background) {
        if (layoutMode && !LevelEditorLayer::get())
            background = 13;
        GJBaseGameLayer::createBackground(background);
    }

    void createMiddleground(int middleground) {
        if (layoutMode && !LevelEditorLayer::get())
            middleground = 0;
        GJBaseGameLayer::createMiddleground(middleground);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        #ifdef GEODE_IS_MOBILE
        m_allowedButtons.clear(); // funny way to allow swift clicks on mobile
        #endif
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

$execute {
    ScarletUtils::UpdateHook::preTps([](bool isHalfTick) {
        auto bgl = GJBaseGameLayer::get();
        if (layoutMode)
            bgl->toggleGlitter(false);
        clickedJumpPad = false;
        optimizeRingJump = false;
        optimizeStartDashing = false;
        optimizeStopDashing = false;

        if (straightUfo) {
            if (straightUfoP1 &&
                ((bgl->m_player1->getPositionY() < straightUfoTargetP1 - straightUfoThresholdP1 &&
                  bgl->m_player1->m_yVelocity <= 0 && !bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getPositionY() > straightUfoTargetP1 + straightUfoThresholdP1 &&
                  bgl->m_player1->m_yVelocity > 0 && !bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getPositionY() > straightUfoTargetP1 + straightUfoThresholdP1 &&
                  bgl->m_player1->m_yVelocity >= 0 && bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getPositionY() < straightUfoTargetP1 - straightUfoThresholdP1 &&
                  bgl->m_player1->m_yVelocity < 0 && bgl->m_player1->m_isUpsideDown))) {
                bgl->queueButton((int)PlayerButton::Jump, false,
                GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                bgl->queueButton((int)PlayerButton::Jump, true,
                GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
            }

            if (straightUfoP2 &&
                ((bgl->m_player2->getPositionY() < straightUfoTargetP2 - straightUfoThresholdP2 &&
                  bgl->m_player2->m_yVelocity <= 0 && !bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getPositionY() > straightUfoTargetP2 + straightUfoThresholdP2 &&
                  bgl->m_player2->m_yVelocity > 0 && !bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getPositionY() > straightUfoTargetP2 + straightUfoThresholdP2 &&
                  bgl->m_player2->m_yVelocity >= 0 && bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getPositionY() < straightUfoTargetP2 - straightUfoThresholdP2 &&
                  bgl->m_player2->m_yVelocity < 0 && bgl->m_player2->m_isUpsideDown))) {
                bgl->queueButton((int)PlayerButton::Jump, false,
                !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                bgl->queueButton((int)PlayerButton::Jump, true,
                !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
            }
        }

        if (straightFly) {
            if (straightFlyP1 &&
                ((bgl->m_player1->getYVelocity() < -straightFlyThresholdP1 &&
                  !bgl->m_player1->m_holdingButtons[1] && !bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getYVelocity() > straightFlyThresholdP1 &&
                  bgl->m_player1->m_holdingButtons[1] && !bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getYVelocity() > straightFlyThresholdP1 &&
                  !bgl->m_player1->m_holdingButtons[1] && bgl->m_player1->m_isUpsideDown) ||
                 (bgl->m_player1->getYVelocity() < -straightFlyThresholdP1 &&
                  bgl->m_player1->m_holdingButtons[1] && bgl->m_player1->m_isUpsideDown)))
                bgl->queueButton((int)PlayerButton::Jump, !bgl->m_player1->m_holdingButtons[1],
                GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);

            if (straightFlyP2 &&
                ((bgl->m_player2->getYVelocity() < -straightFlyThresholdP2 &&
                  !bgl->m_player2->m_holdingButtons[1] && !bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getYVelocity() > straightFlyThresholdP2 &&
                  bgl->m_player2->m_holdingButtons[1] && !bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getYVelocity() > straightFlyThresholdP2 &&
                  !bgl->m_player2->m_holdingButtons[1] && bgl->m_player2->m_isUpsideDown) ||
                 (bgl->m_player2->getYVelocity() < -straightFlyThresholdP2 &&
                  bgl->m_player2->m_holdingButtons[1] && bgl->m_player2->m_isUpsideDown)))
                bgl->queueButton((int)PlayerButton::Jump, !bgl->m_player2->m_holdingButtons[1],
                !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
        }

        if (autoclickerP1) {
            if (autoclickerHoldingP1 && autoclickerTimerP1 >= autoclickerHoldP1) {
                if (!autoclickerSwiftP1)
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                else {
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                }
                autoclickerHoldingP1 = false;
                autoclickerTimerP1 = 0;
            }
            if (!autoclickerHoldingP1 && autoclickerTimerP1 >= autoclickerEveryP1) {
                if (!autoclickerSwiftP1)
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                else {
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                }
                autoclickerHoldingP1 = true;
                autoclickerTimerP1 = 0;
            }
            autoclickerTimerP1++;
        }
        if (autoclickerP2) {
            if (autoclickerHoldingP2 && autoclickerTimerP2 >= autoclickerHoldP2) {
                if (!autoclickerSwiftP2)
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                else {
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                }
                autoclickerHoldingP2 = false;
                autoclickerTimerP2 = 0;
            }
            if (!autoclickerHoldingP2 && autoclickerTimerP2 >= autoclickerEveryP2) {
                if (!autoclickerSwiftP2)
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                else {
                    bgl->queueButton((int)PlayerButton::Jump, true,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                    bgl->queueButton((int)PlayerButton::Jump, false,
                    !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
                }
                autoclickerHoldingP2 = true;
                autoclickerTimerP2 = 0;
            }
            autoclickerTimerP2++;
        }
        
        runMaintainGravity();
        runMirrorInput();
    });
};