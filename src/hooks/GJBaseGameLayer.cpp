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

// Removes any existing queued entry for the given player+button before
// adding a fresh one, instead of clearing the whole queue. A blanket
// clear() wipes out anything ELSE already queued that same tick too —
// another feature's own correction, or the source player's own real input
// event — which is exactly what caused the Maintain Gravity and Mirror
// Input bugs earlier. This only removes what it's about to replace, so
// Straight Fly's (or anything else's) own queued correction from earlier
// in the same tick survives untouched.
void queuePlayerButtonReplacing(GJBaseGameLayer* bgl, bool isPlayer2, bool push) {
    bgl->m_queuedButtons.erase(
        std::remove_if(bgl->m_queuedButtons.begin(), bgl->m_queuedButtons.end(),
            [isPlayer2](const PlayerButtonCommand& cmd) {
                return cmd.m_isPlayer2 == isPlayer2 && cmd.m_button == PlayerButton::Jump;
            }),
        bgl->m_queuedButtons.end());
    bgl->queueButton((int)PlayerButton::Jump, push, isPlayer2, 0.0);
}

void runMaintainGravity() {
    auto bgl = GJBaseGameLayer::get();
    if (maintainGravity) {
        bool p1maintain = bgl->m_player1->m_holdingButtons[1] != bgl->m_player1->m_isUpsideDown;
        bool p2maintain = bgl->m_player2->m_holdingButtons[1] != bgl->m_player2->m_isUpsideDown;

        bool p1holding = bgl->m_uiLayer->m_p1Jumping || bgl->m_uiLayer->m_p1TouchId != -1;
        bool p2holding = bgl->m_uiLayer->m_p2Jumping || bgl->m_uiLayer->m_p2TouchId != -1;

        if (GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls))
            std::swap(p1holding, p2holding);

        bgl->m_queuedButtons.clear();

        if ((p1holding || (autoclickerHoldingP1 && autoclickerP1)) != p1maintain) {
            bgl->queueButton((int)PlayerButton::Jump, !bgl->m_player1->m_holdingButtons[1],
            GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), 0.0);
            autoclickerTimerP1 = INT32_MAX;
        }

        if ((p2holding || (autoclickerHoldingP2 && autoclickerP2)) != p2maintain &&
            bgl->m_gameState.m_isDualMode && bgl->m_levelSettings->m_twoPlayerMode) {
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
        // a player's own input actually changes (press or release), never
        // continuously — a continuous version would eat real input the
        // exact same way Maintain Gravity's original version did.
        //
        // Behavior: whichever player provides input is the source for that
        // instant — the other player copies it (or does the opposite, if
        // Inverted). Same physical/logical swap as Maintain Gravity above
        // so this still tracks the right player if Flip2PlayerControls is on.
        //
        // Crucially: no clear() anywhere here. That was the bug in the
        // one-directional version — clearing the queue before adding only
        // the mirrored correction also wiped out the source player's own
        // real button event for that same frame, which is why player 1
        // stopped responding to its own input the moment the feature was
        // turned on. Just appending each correction and leaving whatever
        // real input is already queued alone fixes that for both directions.
        static bool prevP1Holding = false;
        static bool prevP2Holding = false;

        bool p1Holding = bgl->m_uiLayer->m_p1Jumping || bgl->m_uiLayer->m_p1TouchId != -1;
        bool p2Holding = bgl->m_uiLayer->m_p2Jumping || bgl->m_uiLayer->m_p2TouchId != -1;
        if (GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls))
            std::swap(p1Holding, p2Holding);

        bool p1Changed = p1Holding != prevP1Holding;
        bool p2Changed = p2Holding != prevP2Holding;
        prevP1Holding = p1Holding;
        prevP2Holding = p2Holding;

        if (verboseLoggingEnabled() && (p1Changed || p2Changed)) {
            geode::log::info(
                "Scarlet Utils: mirrorInput p1Holding={} p1Changed={} p2Holding={} p2Changed={}",
                p1Holding, p1Changed, p2Holding, p2Changed);
        }

        // Player 1 provided input -> mirror it onto player 2.
        if (p1Changed) {
            bool target = mirrorInputInverted ? !p1Holding : p1Holding;
            queuePlayerButtonReplacing(bgl,
                !GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), target);
        }

        // Player 2 provided input -> mirror it onto player 1.
        if (p2Changed) {
            bool target = mirrorInputInverted ? !p2Holding : p2Holding;
            queuePlayerButtonReplacing(bgl,
                GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls), target);
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