#pragma once
#include "RE/B/BSTEvent.h"
#include "ClibUtil/singleton.hpp"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <map>
#include <string>
#include <chrono>
class AnimationEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
public:
    static AnimationEventHandler* GetSingleton() {
        static AnimationEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
};


class AttackStateManager
    : public RE::BSTEventSink<RE::InputEvent*> {  // TIPO 2: Sem ponteiro aqui
public:

    static AttackStateManager* GetSingleton() {
        static AttackStateManager singleton;
        return &singleton;
    }
    void Register();

    // Assinatura CORRETA para RE::InputEvent*
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                          RE::BSTEventSource<RE::InputEvent*>* a_source) override;

     

private:

    bool inAttackComboWindow = false;
    bool isAttacking = false;
    bool isPlayerSprinting = false;
    bool isPlayerMoving = false;
   
    void OpenComboWindow();

};


extern RE::BGSAction* PowerRight;
extern RE::BGSAction* PowerStanding;
extern RE::BGSAction* PowerLeft;
extern RE::BGSAction* PowerDual;
extern RE::BGSAction* NormalAttack;
extern RE::BGSAction* RightAttack;
extern RE::BGSAction* PowerAttack;
extern RE::BGSAction* Bash;

void GetAttackKeys();

namespace GlobalControl{
    class AnimationEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimationEventHandler* GetSingleton() {
            static AnimationEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
    };
}
