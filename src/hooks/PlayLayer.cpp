#include "../includes.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(ScarletPlayLayer, PlayLayer) {
    void addObject(GameObject* object) {
        if (!PlayLayer::get())
            return PlayLayer::addObject(object);

        if (noEffect && object->m_objectType != GameObjectType::InverseMirrorPortal && object->m_objectType != GameObjectType::NormalMirrorPortal)
            object->m_hasNoEffects = true;

        if (!layoutMode)
            return PlayLayer::addObject(object);

        if (filter.contains(object->m_objectID)) {
            object->m_isHide = true;
            return;
        }

        object->m_hasNoGlow = true;
        object->m_isHide = false;
        object->m_hasNoAudioScale = false;
        object->m_detailUsesHSV = false;
        object->m_baseUsesHSV = false;
        object->m_activeDetailColorID = -1;
        object->m_activeMainColorID = -1;
        object->m_isDontEnter = true;
        object->m_isDontFade = true;
        object->m_ignoreFade = true;
        object->m_ignoreEnter = true;
        object->m_hasParticles = false;
        object->setOpacity(255);
        if (!noEffect)
            object->m_hasNoEffects = false;

        PlayLayer::addObject(object);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (player->m_isDead || object == m_anticheatSpike)
            return PlayLayer::destroyPlayer(player, object);

        if (noclip && noclipP1 && player == m_player1) return;
        if (noclip && noclipP2 && player == m_player2) return;

        PlayLayer::destroyPlayer(player, object);

        flipPlayer = player->m_isSecondPlayer ? 2 : 1;

        autoclickerHoldingP1 = false;
        autoclickerTimerP1 = INT_MAX;
        autoclickerHoldingP2 = false;
        autoclickerTimerP2 = INT_MAX;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        this->applyStartFade();
        if (flipOnDeath && flipPlayer != 0) {
            if (flipOnDeathP1 && flipPlayer == 1 || flipOnDeathBoth) {
                if (!flipOnDeathSwift) {
                    if (flipOnDeathLogicP1) {
                        queueButton((int)PlayerButton::Jump, true, false ^
                        GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    }
                    flipPlayer = 0;
                    flipOnDeathLogicP1 = !flipOnDeathLogicP1;
                } else {
                    queueButton((int)PlayerButton::Jump, true, false ^
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    queueButton((int)PlayerButton::Jump, false, false ^
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    flipPlayer = 0;
                }
            }
            if (flipOnDeathP2 && flipPlayer == 2 || flipOnDeathBoth) {
                if (!flipOnDeathSwift) {
                    if (flipOnDeathLogicP2 || flipOnDeathSwift) {
                        queueButton((int)PlayerButton::Jump, true, true ^
                        GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    }
                    flipPlayer = 0;
                    flipOnDeathLogicP2 = !flipOnDeathLogicP2;
                } else {
                    queueButton((int)PlayerButton::Jump, true, true ^
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    queueButton((int)PlayerButton::Jump, false, true ^
                    GameManager::sharedState()->getGameVariable(GameVar::Flip2PlayerControls),0.0);
                    flipPlayer = 0;
                }
            }
            this->processQueuedButtons(0, true);
            #ifdef GEODE_IS_WINDOWS
            if (flipOnDeathUnfreeze)
                PostMessage(hwnd, WM_KEYDOWN, 0x56, 0);
                PostMessage(hwnd, WM_KEYUP, 0x56, 0);
            #endif
        }
    }

    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("PlayLayer::resetLevel", Priority::First)) {
            geode::log::warn("Failed to set hook priority.");
        }
    }

    void onStartFadeRemoved(CCNode*) {
        // The action sequence below has finished and removed the layer itself;
        // clear the pointer so applyStartFade() is able to run again on the
        // next attempt instead of thinking a fade is still in progress.
        m_startFadeLayer = nullptr;
    }

    void applyStartFade() {
        if (fadeLevel && fadeLevelInDuration > 0) {
            if (m_startFadeLayer) return;

            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_startFadeLayer = CCLayerColor::create(ccc4(0, 0, 0, 255), winSize.width, winSize.height);

            this->addChild(m_startFadeLayer, 100);

            auto fadeAction = CCFadeOut::create(fadeLevelInDuration);
            auto clearPointer = CCCallFuncN::create(this, callfuncN_selector(ScarletPlayLayer::onStartFadeRemoved));
            auto removeAction = CCRemoveSelf::create();
            m_startFadeLayer->runAction(CCSequence::create(fadeAction, clearPointer, removeAction, nullptr));

            if (fadeAudio && fadeAudioInDuration > 0) {
                FMODAudioEngine::sharedEngine()->fadeMusic(fadeAudioInDuration, 0, 0.0f, 1.f);
            }
        }
    }

    void onEnterTransitionDidFinish() {
        if (layoutMode) GJBaseGameLayer::get()->toggleGlitter(false);

        PlayLayer::onEnterTransitionDidFinish();
    }

    void startGame() {
        PlayLayer::startGame();
        this->applyStartFade();

        if (restartFirstFrame) {
            geode::queueInMainThread([this]{ PlayLayer::get()->resetLevel(); });
        }
    }

    void onEndFadeRemoved(CCNode*) {
        m_endFadeLayer = nullptr;
    }

    void showEndLayer() {
        PlayLayer::showEndLayer();

        if (fadeLevel && fadeLevelOutDuration > 0) {
            if (m_endFadeLayer)
                return;

            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_endFadeLayer = CCLayerColor::create(ccc4(0, 0, 0 , 255), winSize.width, winSize.height);

            this->addChild(m_endFadeLayer, 1000);

            auto fadeIn = CCFadeIn::create(fadeLevelOutDuration);
            auto hold = CCDelayTime::create(fadeLevelOutTimeout);
            auto fadeOut = CCFadeOut::create(fadeLevelOutDuration);
            auto clearPointer = CCCallFuncN::create(this, callfuncN_selector(ScarletPlayLayer::onEndFadeRemoved));
            auto removeAction = CCRemoveSelf::create();

            m_endFadeLayer->runAction(CCSequence::create(fadeIn, hold, fadeOut, clearPointer, removeAction, nullptr));

            if (fadeAudio && fadeAudioOutDuration > 0) {
                FMODAudioEngine::sharedEngine()->fadeMusic(fadeAudioOutDuration, 0, 1.0f, 0.0f);
            }
        }
    }

    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (!hideNewBest)
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }

    void playGravityEffect(bool flip) {
        if (!noEffect)
        PlayLayer::playGravityEffect(flip);
    }
};