#pragma once
#include "RE/B/BSTEvent.h"
#include "ClibUtil/singleton.hpp"

class AttackStateManager : public RE::BSTEventSink<RE::InputEvent*>  // TIPO 1: Note o ponteiro em RE::InputEvent*
                            {  // TIPO 2: Sem ponteiro aqui
public:
    static AttackStateManager* GetSingleton();
    void Register();

    // Assinatura CORRETA para RE::InputEvent*
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                          RE::BSTEventSource<RE::InputEvent*>* a_source) override;



private:

    bool inAttackComboWindow = false;
    bool isAttacking = false;
    bool isPlayerSprinting = false;
};

enum class WeaponState {
    Unarmed,           // Mãos vazias
    OneHanded,         // Arma de 1M na direita, esquerda livre
    OneHandAndShield,  // Arma de 1M na direita, escudo na esquerda
    TwoHanded,         // Arma de 2M
    DualWield,         // Duas armas de 1M
    Invalid            // Estado inválido para bloqueio (ex: magia na mão)
};
extern RE::BGSAction* PowerRight;
extern RE::BGSAction* PowerStanding;
extern RE::BGSAction* PowerLeft;
extern RE::BGSAction* PowerDual;
extern RE::BGSAction* NormalAttack;

