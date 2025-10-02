#include "Hooks.h"
#include "Settings.h"

// Declaramos as vari�veis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
extern const SKSE::TaskInterface* g_task;


const std::string pluginName = "SCSI-ACTbfco-Main.esp";

RE::BGSAction* PowerRight = nullptr;
RE::BGSAction* PowerLeft = nullptr;
RE::BGSAction* PowerDual = nullptr;

AttackStateManager* AttackStateManager::GetSingleton() {
    static AttackStateManager singleton;
    return &singleton;
}

void AttackStateManager::Register() {
    SKSE::log::info("Tentando registrar listener de input...");
    auto input = RE::BSInputDeviceManager::GetSingleton();
    if (input) {
        input->AddEventSink(this);
        SKSE::log::info("SUCESSO: Listener de eventos de input registrado.");
    } else {
        // LOG ADICIONADO: Informa explicitamente se o gerenciador de input for nulo.
        SKSE::log::error(
            "FALHA: O gerenciador de input (BSInputDeviceManager) � nulo. O listener n�o p�de ser registrado.");
    }
}


RE::TESIdleForm* GetIdleByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    RE::TESForm* lookupForm = dataHandler ? dataHandler->LookupForm(a_formID, a_pluginName) : nullptr;
    if (!lookupForm) {
        SKSE::log::warn("N�o foi poss�vel encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
        return nullptr;
    }
    // Verificamos se o tipo do formul�rio � IdleForm e fazemos o cast.
    if (lookupForm->GetFormType() == RE::FormType::Idle) {
        return static_cast<RE::TESIdleForm*>(lookupForm);
    }
    SKSE::log::warn("O FormID 0x{:X} n�o � um TESIdleForm.", a_formID);
    return nullptr;
}

void PlayIdleAnimation(RE::Actor* a_actor, RE::TESIdleForm* a_idle) {
    if (a_actor && a_idle) {
        if (auto* processManager = a_actor->GetActorRuntimeData().currentProcess) {
            // [MODIFICADO] Usando a assinatura exata que voc� encontrou: (ator, idle, alvo)
            // Como a anima��o � para o pr�prio ator, ele ser� o seu pr�prio alvo.
            processManager->PlayIdle(a_actor, a_idle, a_actor);

            SKSE::log::info("Tocando anima��o idle FormID 0x{:X}", a_idle->GetFormID());
        } else {
            SKSE::log::error("N�o foi poss�vel obter o AIProcess (currentProcess) do ator.");
        }
    }
}

void CastSpell(RE::Actor* a_caster, RE::SpellItem* a_spell) {
    if (a_caster && a_spell) {
        if (auto* magicCaster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            magicCaster->CastSpellImmediate(a_spell, false, a_caster, 1.0f, false, 0.0f, nullptr);
            SKSE::log::info("Feiti�o '{}' lan�ado com sucesso.", a_spell->GetFullName());
        }
    }
}

RE::SpellItem* GetSpellByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        SKSE::log::error("Falha ao obter TESDataHandler.");
        return nullptr;
    }

    RE::TESForm* lookupForm = dataHandler->LookupForm(a_formID, a_pluginName);
    if (!lookupForm) {
        SKSE::log::warn("N�o foi poss�vel encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
        return nullptr;
    }

    if (lookupForm->GetFormType() != RE::FormType::Spell) {
        SKSE::log::warn("O FormID 0x{:X} n�o � um SpellItem.", a_formID);
        return nullptr;
    }

    return lookupForm->As<RE::SpellItem>();
}

RE::BGSAction* GetActionByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return nullptr;

    auto* lookupForm = dataHandler->LookupForm(a_formID, a_pluginName);
    if (!lookupForm) {
        SKSE::log::warn("N�o foi poss�vel encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
        return nullptr;
    }
    return lookupForm->As<RE::BGSAction>();
}

void PerformAction(RE::BGSAction* action,RE::Actor* player) {
    if (action && player) {
        std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
        data->source = RE::NiPointer<RE::TESObjectREFR>(player);
        data->action = action;
        typedef bool func_t(RE::TESActionData*);
        REL::Relocation<func_t> func{RELOCATION_ID(40551, 41557)};
        func(data.get());
    }
}



WeaponState GetPlayerWeaponState(RE::PlayerCharacter* player) {
    if (!player) {
        return WeaponState::Invalid;
    }

    // 1. Obtenha os objetos equipados em ambas as m�os.
    auto rightHandObject = player->GetEquippedObject(false);
    auto leftHandObject = player->GetEquippedObject(true);

    // 2. CASO PRIORIT�RIO: Verificar se a m�o esquerda tem um item "inv�lido".
    //    Um item � inv�lido se ele EXISTE, mas N�O � uma arma E N�O � um escudo.
    if (leftHandObject) {
        bool isWeapon = leftHandObject->IsWeapon();
        bool isShield = false;
        if (leftHandObject->IsArmor()) {
            auto armor = leftHandObject->As<RE::TESObjectARMO>();
            if (armor && armor->IsShield()) {
                isShield = true;
            }
        }

        // Se o objeto na m�o esquerda n�o � nem arma nem escudo, o estado � inv�lido para bloqueio.
        if (!isWeapon && !isShield) {
            return WeaponState::Invalid;
        }
    }

    // A partir daqui, sabemos que a m�o esquerda est� ou vazia, ou com uma arma, ou com um escudo.

    // 3. Obtenha a arma na m�o direita para as pr�ximas checagens.
    auto rightWeapon = rightHandObject ? rightHandObject->As<RE::TESObjectWEAP>() : nullptr;

    // 4. Checagem por prioridade
    if (rightWeapon) {
        // Caso 1: Arma de Duas M�os
        auto weaponType = rightWeapon->GetWeaponType();
        if (weaponType == RE::WEAPON_TYPE::kTwoHandAxe || weaponType == RE::WEAPON_TYPE::kTwoHandSword ||
            weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow ||
            weaponType == RE::WEAPON_TYPE::kStaff) {
            return WeaponState::TwoHanded;
        }

        // Se n�o for de duas m�os, vamos ver o que tem na m�o esquerda
        if (leftHandObject) {
            if (leftHandObject->IsWeapon()) {
                return WeaponState::DualWield;  // Arma na direita, arma na esquerda
            }
            if (leftHandObject->IsArmor()) {           // J� sabemos que s� pode ser um escudo
                return WeaponState::OneHandAndShield;  // Arma na direita, escudo na esquerda
            }
        }

        // Se a m�o esquerda estiver vazia (leftHandObject � nulo)
        return WeaponState::OneHanded;  // Arma apenas na direita
    }

    // 5. Se n�o h� arma na m�o direita, o jogador est� desarmado.
    //    (O caso de arma apenas na esquerda foi tratado como inv�lido na checagem priorit�ria,
    //    pois n�o se pode bloquear com o bot�o direito nesse estado).
    return WeaponState::Unarmed;
}


RE::BSEventNotifyControl AttackStateManager::ProcessEvent(RE::InputEvent* const* a_event,
                                                          RE::BSTEventSource<RE::InputEvent*>* a_source) {
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->Is3DLoaded()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    for (auto event = *a_event; event; event = event->next) {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        auto buttonEvent = event->AsButtonEvent();
        if (!buttonEvent) {
            continue;
        }

        // Vamos processar tanto o pressionar quanto o soltar do bot�o
        if (!buttonEvent->IsDown() && !buttonEvent->IsUp()) {
            continue;
        }

        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        // --- IN�CIO DA L�GICA REORDENADA ---

        // 1. VERIFICAR A��ES CUSTOMIZADAS PRIMEIRO (APENAS QUANDO O BOT�O � PRESSIONADO)
        if (buttonEvent->IsDown()) {
            bool powerAttackKeyPressed = false;
            bool comboKeyPressed = false;
            if (device == RE::INPUT_DEVICE::kMouse) {
                keyCode += 256;  // Ajusta o c�digo do mouse para corresponder �s configura��es
            }

            if (device == RE::INPUT_DEVICE::kKeyboard) {
                if (keyCode == Settings::PowerAttackKey_k) powerAttackKeyPressed = true;
                if (keyCode == Settings::comboKey_k) comboKeyPressed = true;
            } else if (device == RE::INPUT_DEVICE::kGamepad) {
                if (keyCode == Settings::PowerAttackKey_g) powerAttackKeyPressed = true;
                if (keyCode == Settings::comboKey_g) comboKeyPressed = true;
            } else if (device == RE::INPUT_DEVICE::kMouse) {
                if (keyCode == Settings::PowerAttackKey_m) powerAttackKeyPressed = true;
                if (keyCode == Settings::comboKey_m) comboKeyPressed = true;
            }

            // SE A HOTKEY DE POWER ATTACK FOI PRESSIONADA...
            if (powerAttackKeyPressed) {
                WeaponState currentState = GetPlayerWeaponState(player);
                switch (currentState) {
                    case WeaponState::OneHanded:
                    case WeaponState::Invalid:
                    case WeaponState::TwoHanded:
                        PerformAction(PowerRight, player);
                        break;
                    case WeaponState::DualWield:
                        PerformAction(PowerDual, player);
                        break;
                    default:  // Unarmed ou outros casos
                        PerformAction(PowerRight, player);
                        break;
                }
                return RE::BSEventNotifyControl::kStop;  // A��o executada, consome o input.
            }

            // SE A HOTKEY DE COMBO ATTACK FOI PRESSIONADA...
            if (comboKeyPressed) {
                bool hasCombo = false;
                player->GetGraphVariableBool("BFCO_HasCombo", hasCombo);  // L� o valor do behavior

                if (hasCombo) {
                    player->NotifyAnimationGraph("MCO_EndAnimation");
                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                        PlayIdleAnimation(player, idleToPlay);
                    }
                } else {
                    PerformAction(PowerRight, player);  // Fallback se n�o tiver combo
                }
                return RE::BSEventNotifyControl::kStop;  // A��o executada, consome o input.
            }
        }

        // 2. SE NENHUMA HOTKEY FOI ACIONADA, VERIFICA A L�GICA PADR�O DE BLOQUEIO DO MOUSE
        // A verifica��o de `device` � importante para n�o aplicar essa l�gica a bot�es de controle
        if (device == RE::INPUT_DEVICE::kMouse && keyCode == 257) {  // 257 � o bot�o direito do mouse
            if (buttonEvent->IsDown()) {
                player->NotifyAnimationGraph("blockStart");
                player->SetGraphVariableBool("IsBlocking", true);
                return RE::BSEventNotifyControl::kStop;
            } else if (buttonEvent->IsUp()) {
                player->SetGraphVariableBool("IsBlocking", false);
                player->NotifyAnimationGraph("blockStop");
                return RE::BSEventNotifyControl::kStop;
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

//// Processa eventos vindos do motor de anima��o
//RE::BSEventNotifyControl AttackStateManager::ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
//                                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) {
//    if (!a_event || !a_event->holder || !a_event->holder->IsPlayerRef()) {
//        return RE::BSEventNotifyControl::kContinue;
//    }
//
//    std::string eventName = a_event->tag.c_str();
//
//    // Eventos que ABREM a janela de combo
//    if (eventName == "MCO_WinOpen" || eventName == "MCO_PowerWinOpen" || eventName == "BFCO_NextWinStart" ||
//        eventName == "BFCO_NextPowerWinStart") {
//        if (!this->inAttackComboWindow) {
//            SKSE::log::info(">>> L�GICA: Janela de combo ABERTA pelo evento: {}.", eventName);
//            this->inAttackComboWindow = true;
//        }
//        // Eventos que FECHAM a janela de combo
//    } else if (eventName == "MCO_WinClose" || eventName == "MCO_PowerWinClose" || eventName == "BFCO_DIY_EndLoop" ||
//               eventName == "AttackWinEnd" || eventName == "MCO_Recovery" || eventName == "BFCO_DIY_recovery" ||
//               eventName == "AttackStop") {
//        if (this->inAttackComboWindow) {
//            SKSE::log::info(">>> L�GICA: Janela de combo FECHADA pelo evento: {}.", eventName);
//            this->inAttackComboWindow = false;
//        }
//    }
//
//    return RE::BSEventNotifyControl::kContinue;
//}
//
//bool AttackStateManager::HasEnoughStamina(RE::PlayerCharacter* player) {
//    // Podemos come�ar com um valor fixo para testar. 30 � um custo razo�vel para um power attack.
//    float requiredStamina = 30.0f;
//
//    float currentStamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
//
//    if (currentStamina >= requiredStamina) {
//        return true;
//    } else {
//        SKSE::log::warn("--- VERIFICA��O FALHOU: Vigor insuficiente. Necess�rio: {}, Atual: {}", requiredStamina,
//                        currentStamina);
//        // Opcional: Adicionar um som de "falha" ou efeito visual aqui, como piscar a barra de vigor.
//        return false;
//    }
//}
