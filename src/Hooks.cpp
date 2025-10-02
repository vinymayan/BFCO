#include "Hooks.h"
#include "Settings.h"

// Declaramos as variáveis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
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
            "FALHA: O gerenciador de input (BSInputDeviceManager) é nulo. O listener não pôde ser registrado.");
    }
}


RE::TESIdleForm* GetIdleByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    RE::TESForm* lookupForm = dataHandler ? dataHandler->LookupForm(a_formID, a_pluginName) : nullptr;
    if (!lookupForm) {
        SKSE::log::warn("Não foi possível encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
        return nullptr;
    }
    // Verificamos se o tipo do formulário é IdleForm e fazemos o cast.
    if (lookupForm->GetFormType() == RE::FormType::Idle) {
        return static_cast<RE::TESIdleForm*>(lookupForm);
    }
    SKSE::log::warn("O FormID 0x{:X} não é um TESIdleForm.", a_formID);
    return nullptr;
}

void PlayIdleAnimation(RE::Actor* a_actor, RE::TESIdleForm* a_idle) {
    if (a_actor && a_idle) {
        if (auto* processManager = a_actor->GetActorRuntimeData().currentProcess) {
            // [MODIFICADO] Usando a assinatura exata que você encontrou: (ator, idle, alvo)
            // Como a animação é para o próprio ator, ele será o seu próprio alvo.
            processManager->PlayIdle(a_actor, a_idle, a_actor);

            SKSE::log::info("Tocando animação idle FormID 0x{:X}", a_idle->GetFormID());
        } else {
            SKSE::log::error("Não foi possível obter o AIProcess (currentProcess) do ator.");
        }
    }
}

void CastSpell(RE::Actor* a_caster, RE::SpellItem* a_spell) {
    if (a_caster && a_spell) {
        if (auto* magicCaster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            magicCaster->CastSpellImmediate(a_spell, false, a_caster, 1.0f, false, 0.0f, nullptr);
            SKSE::log::info("Feitiço '{}' lançado com sucesso.", a_spell->GetFullName());
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
        SKSE::log::warn("Não foi possível encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
        return nullptr;
    }

    if (lookupForm->GetFormType() != RE::FormType::Spell) {
        SKSE::log::warn("O FormID 0x{:X} não é um SpellItem.", a_formID);
        return nullptr;
    }

    return lookupForm->As<RE::SpellItem>();
}

RE::BGSAction* GetActionByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return nullptr;

    auto* lookupForm = dataHandler->LookupForm(a_formID, a_pluginName);
    if (!lookupForm) {
        SKSE::log::warn("Não foi possível encontrar o FormID 0x{:X} no plugin {}", a_formID, a_pluginName);
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

    // 1. Obtenha os objetos equipados em ambas as mãos.
    auto rightHandObject = player->GetEquippedObject(false);
    auto leftHandObject = player->GetEquippedObject(true);

    // 2. CASO PRIORITÁRIO: Verificar se a mão esquerda tem um item "inválido".
    //    Um item é inválido se ele EXISTE, mas NÃO É uma arma E NÃO É um escudo.
    if (leftHandObject) {
        bool isWeapon = leftHandObject->IsWeapon();
        bool isShield = false;
        if (leftHandObject->IsArmor()) {
            auto armor = leftHandObject->As<RE::TESObjectARMO>();
            if (armor && armor->IsShield()) {
                isShield = true;
            }
        }

        // Se o objeto na mão esquerda não é nem arma nem escudo, o estado é inválido para bloqueio.
        if (!isWeapon && !isShield) {
            return WeaponState::Invalid;
        }
    }

    // A partir daqui, sabemos que a mão esquerda está ou vazia, ou com uma arma, ou com um escudo.

    // 3. Obtenha a arma na mão direita para as próximas checagens.
    auto rightWeapon = rightHandObject ? rightHandObject->As<RE::TESObjectWEAP>() : nullptr;

    // 4. Checagem por prioridade
    if (rightWeapon) {
        // Caso 1: Arma de Duas Mãos
        auto weaponType = rightWeapon->GetWeaponType();
        if (weaponType == RE::WEAPON_TYPE::kTwoHandAxe || weaponType == RE::WEAPON_TYPE::kTwoHandSword ||
            weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow ||
            weaponType == RE::WEAPON_TYPE::kStaff) {
            return WeaponState::TwoHanded;
        }

        // Se não for de duas mãos, vamos ver o que tem na mão esquerda
        if (leftHandObject) {
            if (leftHandObject->IsWeapon()) {
                return WeaponState::DualWield;  // Arma na direita, arma na esquerda
            }
            if (leftHandObject->IsArmor()) {           // Já sabemos que só pode ser um escudo
                return WeaponState::OneHandAndShield;  // Arma na direita, escudo na esquerda
            }
        }

        // Se a mão esquerda estiver vazia (leftHandObject é nulo)
        return WeaponState::OneHanded;  // Arma apenas na direita
    }

    // 5. Se não há arma na mão direita, o jogador está desarmado.
    //    (O caso de arma apenas na esquerda foi tratado como inválido na checagem prioritária,
    //    pois não se pode bloquear com o botão direito nesse estado).
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

        // Vamos processar tanto o pressionar quanto o soltar do botão
        if (!buttonEvent->IsDown() && !buttonEvent->IsUp()) {
            continue;
        }

        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        // --- INÍCIO DA LÓGICA REORDENADA ---

        // 1. VERIFICAR AÇÕES CUSTOMIZADAS PRIMEIRO (APENAS QUANDO O BOTÃO É PRESSIONADO)
        if (buttonEvent->IsDown()) {
            bool powerAttackKeyPressed = false;
            bool comboKeyPressed = false;
            if (device == RE::INPUT_DEVICE::kMouse) {
                keyCode += 256;  // Ajusta o código do mouse para corresponder às configurações
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
                return RE::BSEventNotifyControl::kStop;  // Ação executada, consome o input.
            }

            // SE A HOTKEY DE COMBO ATTACK FOI PRESSIONADA...
            if (comboKeyPressed) {
                bool hasCombo = false;
                player->GetGraphVariableBool("BFCO_HasCombo", hasCombo);  // Lê o valor do behavior

                if (hasCombo) {
                    player->NotifyAnimationGraph("MCO_EndAnimation");
                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                        PlayIdleAnimation(player, idleToPlay);
                    }
                } else {
                    PerformAction(PowerRight, player);  // Fallback se não tiver combo
                }
                return RE::BSEventNotifyControl::kStop;  // Ação executada, consome o input.
            }
        }

        // 2. SE NENHUMA HOTKEY FOI ACIONADA, VERIFICA A LÓGICA PADRÃO DE BLOQUEIO DO MOUSE
        // A verificação de `device` é importante para não aplicar essa lógica a botões de controle
        if (device == RE::INPUT_DEVICE::kMouse && keyCode == 257) {  // 257 é o botão direito do mouse
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

//// Processa eventos vindos do motor de animação
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
//            SKSE::log::info(">>> LÓGICA: Janela de combo ABERTA pelo evento: {}.", eventName);
//            this->inAttackComboWindow = true;
//        }
//        // Eventos que FECHAM a janela de combo
//    } else if (eventName == "MCO_WinClose" || eventName == "MCO_PowerWinClose" || eventName == "BFCO_DIY_EndLoop" ||
//               eventName == "AttackWinEnd" || eventName == "MCO_Recovery" || eventName == "BFCO_DIY_recovery" ||
//               eventName == "AttackStop") {
//        if (this->inAttackComboWindow) {
//            SKSE::log::info(">>> LÓGICA: Janela de combo FECHADA pelo evento: {}.", eventName);
//            this->inAttackComboWindow = false;
//        }
//    }
//
//    return RE::BSEventNotifyControl::kContinue;
//}
//
//bool AttackStateManager::HasEnoughStamina(RE::PlayerCharacter* player) {
//    // Podemos começar com um valor fixo para testar. 30 é um custo razoável para um power attack.
//    float requiredStamina = 30.0f;
//
//    float currentStamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
//
//    if (currentStamina >= requiredStamina) {
//        return true;
//    } else {
//        SKSE::log::warn("--- VERIFICAÇÃO FALHOU: Vigor insuficiente. Necessário: {}, Atual: {}", requiredStamina,
//                        currentStamina);
//        // Opcional: Adicionar um som de "falha" ou efeito visual aqui, como piscar a barra de vigor.
//        return false;
//    }
//}
