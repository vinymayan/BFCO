#pragma once
#include "RE/B/BSTEvent.h"
#include "ClibUtil/singleton.hpp"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <map>
#include <string>
#include <chrono>

extern const std::string pluginName;
extern const std::string skyrim;
extern const std::string update;
extern const std::string dragon;

extern RE::BGSAction* PowerRight;
extern RE::BGSAction* Draw;
extern RE::BGSAction* Seath;
extern RE::BGSAction* PowerStanding;
extern RE::BGSAction* PowerLeft;
extern RE::BGSAction* PowerDual;
extern RE::BGSAction* NormalAttack;
extern RE::BGSAction* RightAttack;
extern RE::BGSAction* PowerAttack;
extern RE::BGSAction* Bash;

extern RE::TESIdleForm* PowerNormal;
extern RE::TESIdleForm* AutoAA;
extern RE::TESIdleForm* AutoAABow;
extern RE::TESIdleForm* PowerH2H;
extern RE::TESIdleForm* PowerBash;
extern RE::TESIdleForm* SprintPower;
extern RE::TESIdleForm* ComboAttackE;
extern RE::TESIdleForm* BlockStart;
extern RE::TESIdleForm* BashStart;
extern RE::TESIdleForm* BashRelease;
extern RE::TESIdleForm* BlockRelease;
extern RE::TESIdleForm* Dodge;
extern RE::TESIdleForm* TailSmash;

RE::TESIdleForm* GetIdleByFormID(RE::FormID a_formID, const std::string& a_pluginName);

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



void GetAttackKeys();

namespace GlobalControl{
    inline bool isProcessingRegistration = false;

    class AnimationEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimationEventHandler* GetSingleton() {
            static AnimationEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
    };

    class PC3DLoadEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    public:
        
        static PC3DLoadEventHandler* GetSingleton() {
            static PC3DLoadEventHandler singleton;
            return &singleton;
        }
        
        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
    };
}
