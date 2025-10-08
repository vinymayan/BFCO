#include "Hooks.h"

#include "Settings.h"

// Declaramos as variáveis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
extern const SKSE::TaskInterface* g_task;

const std::string pluginName = "SCSI-ACTbfco-Main.esp";
const std::string skyrim = "Skyrim.esm";
const std::string update = "Update.esm";

RE::BGSAction* PowerRight = nullptr;
RE::BGSAction* PowerStanding = nullptr;
RE::BGSAction* PowerLeft = nullptr;
RE::BGSAction* PowerDual = nullptr;
RE::BGSAction* NormalAttack = nullptr;
RE::BGSAction* PowerAttack = nullptr;
RE::BGSAction* Bash = nullptr;

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

            // SKSE::log::info("Tocando animação idle FormID 0x{:X}", a_idle->GetFormID());
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

void PerformAction(RE::BGSAction* action, RE::Actor* player) {
    if (action && player) {
        std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
        data->source = RE::NiPointer<RE::TESObjectREFR>(player);
        data->action = action;
        typedef bool func_t(RE::TESActionData*);
        REL::Relocation<func_t> func{RELOCATION_ID(40551, 41557)};
        func(data.get());
    }
}

inline std::array blockedMenus = {
    RE::DialogueMenu::MENU_NAME,    RE::JournalMenu::MENU_NAME,    RE::MapMenu::MENU_NAME,
    RE::StatsMenu::MENU_NAME,       RE::ContainerMenu::MENU_NAME,  RE::InventoryMenu::MENU_NAME,
    RE::TweenMenu::MENU_NAME,       RE::TrainingMenu::MENU_NAME,   RE::TutorialMenu::MENU_NAME,
    RE::LockpickingMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME,  RE::LevelUpMenu::MENU_NAME,
    RE::Console::MENU_NAME,         RE::BookMenu::MENU_NAME,       RE::CreditsMenu::MENU_NAME,
    RE::LoadingMenu::MENU_NAME,     RE::MessageBoxMenu::MENU_NAME, RE::MainMenu::MENU_NAME,
    RE::RaceSexMenu::MENU_NAME,     RE::FavoritesMenu::MENU_NAME,  std::string_view("LootMenu"),
    std::string_view("LootMenuIE")};

bool IsAnyMenuOpen() {
    const auto ui = RE::UI::GetSingleton();
    /* if (!ui->menuStack.empty()) {
         return true;
     }*/

    // 2. Verifica menus que precisam do cursor (a maioria dos menus ImGui, como o Wheeler)
    // Se este contador for maior que 0, algo está forçando o cursor a aparecer.
    for (const auto a_name : blockedMenus) {
        if (ui->IsMenuOpen(a_name)) {
            return true;
        }
    }
    return false;
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

void AddPerk(RE::Actor* a_actor, RE::BGSPerk* a_perk) {
    // 1. Verificações de segurança para evitar crashes
    if (!a_actor || !a_perk) {
        SKSE::log::warn("AddPerk foi chamado com um ator ou perk nulo.");
        return;
    }

    // 2. A condição principal: O ator já tem este perk?
    if (a_actor->HasPerk(a_perk)) {
        SKSE::log::info("O ator '{}' já possui o perk '{}'. Nenhuma ação necessária.", a_actor->GetName(),
                        a_perk->GetName());
        return;  // Sai da função se o perk já existe
    }

    // 3. Se o ator não tem o perk, nós o adicionamos.
    //    Perks são adicionados ao 'ActorBase' (TESNPC), que é a "planta" do ator.
    auto actorBase = a_actor->GetActorBase();
    if (actorBase) {
        actorBase->AddPerk(a_perk, 1);  // O '1' é o rank do perk, geralmente 1.
        SKSE::log::info("Perk '{}' adicionado com sucesso ao ator '{}'.", a_perk->GetName(), a_actor->GetName());
    } else {
        SKSE::log::error("Não foi possível obter o ActorBase para o ator '{}'.", a_actor->GetName());
    }
}

RE::BGSPerk* GetPerkByEditorID(RE::FormID a_formID) {
    auto perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_formID);

    if (!perk) {
        // Este log será acionado se o FormID não for encontrado ou se o formulário encontrado não for um perk.
        // Usamos 0x{:X} para formatar o ID em hexadecimal, que é o padrão para FormIDs.
        SKSE::log::warn("Não foi possível encontrar o perk com FormID: 0x{:X}", a_formID);
    }

    return perk;
}

bool IsTryingSprintAttack(RE::Actor* a_actor) {
    if (!a_actor) {
        return false;
    }

    // RE::ActorState contém flags mais detalhados sobre o estado do ator.
    // IsSprinting() é a verificação ideal para o ataque de corrida.
    auto actorState = a_actor->AsActorState();
    return actorState && actorState->IsSprinting();
}

void GetAttackKeys() {
    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    Settings::AttackKey_k = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kKeyboard);
    logger::info("AttackKey_k: {}", Settings::AttackKey_k);
    Settings::AttackKey_m = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kMouse);
    logger::info("AttackKey_m: {}", Settings::AttackKey_m);
    Settings::AttackKey_m += 256;  // Ajusta o código do mouse para corresponder às configurações
    Settings::AttackKey_g = controlMap->GetMappedKey(userEvents->rightAttack, RE::INPUT_DEVICE::kGamepad);
    logger::info("AttackKey_g (before conversion): {}", Settings::AttackKey_g);
    Settings::AttackKey_g = SKSE::InputMap::GamepadMaskToKeycode(Settings::AttackKey_g);
    logger::info("AttackKey_g (after conversion): {}", Settings::AttackKey_g);

    // leftAttackKeyKeyboard = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kKeyboard);
    // leftAttackKeyMouse = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kMouse);
    // leftAttackKeyGamepad = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kGamepad);
    // leftAttackKeyGamepad = SKSE::InputMap::GamepadMaskToKeycode(leftAttackKeyGamepad);
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
        if (!buttonEvent->IsDown() && !buttonEvent->IsUp() ) {
            continue;
        }

        if (IsAnyMenuOpen()) {
            // logger::info("menu aberto");
            return RE::BSEventNotifyControl::kContinue;
        }
        const auto* userEvents = RE::UserEvents::GetSingleton();

        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        // --- INÍCIO DA LÓGICA REORDENADA ---
        const auto playerState = player->AsActorState();

        // if (keyCode == sprintKey) {
        //     if (buttonEvent->IsDown()) {
        //         isPlayerSprinting = true;
        //         //logger::info("O jogador começou a correr.");
        //     }
        //     if (buttonEvent->IsUp()) {
        //         isPlayerSprinting = false;
        //         // logger::info("O jogador começou a correr.");
        //     }
        //     continue;
        // }
        // if (keyCode == forwardKey || keyCode == strafeLeftKey || keyCode == backKey || keyCode == strafeRightKey) {
        //     if (buttonEvent->IsDown()) {
        //         isPlayerMoving = true;
        //     }
        //     if (buttonEvent->IsUp()) {
        //         isPlayerMoving = false;
        //     }
        //     continue;
        // }

        if (!(!player->IsInKillMove() && playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
              playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetFlyState() == RE::FLY_STATE::kNone)) {
            isPlayerSprinting = false;
            return RE::BSEventNotifyControl::kContinue;
        }

        const std::string perk1 = "Critical Charge";
        const std::string perk2 = "Great Critical Charge";

        RE::BGSPerk* CriticalCharge = GetPerkByEditorID(0x58F62);
        RE::BGSPerk* GreatCriticalCharge = GetPerkByEditorID(0xCB406);
        WeaponState currentState = GetPlayerWeaponState(player);

        if (device == RE::INPUT_DEVICE::kMouse) {
            keyCode += 256;  // Ajusta o código do mouse para corresponder às configurações
        }

        bool isBlockButtonPressed = (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::BlockKey_m) ||
                                    (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::BlockKey_g ||
                                     device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::BlockKey_k);
        bool isAttackButtonPressed = (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::AttackKey_m) ||
                                     (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::AttackKey_g ||
                                      device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::AttackKey_k);

        if (isAttackButtonPressed) {
            if (buttonEvent->IsDown()) {
                logger::info("aqte aqui ta indo ativado");
                if (player->IsBlocking()|| isBlocking) {
                    if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 0) {
                        isPlayerSprinting = false;                   // Corrige o estado se a stamina acabar
                        return RE::BSEventNotifyControl::kContinue;  // Não faz o ataque de sprint
                    }
                    player->NotifyAnimationGraph("MCO_EndAnimation");
                    logger::info("Bash ativado");
                    if (auto* idleToPlay = GetIdleByFormID(0x8C0, skyrim)) {
                        PlayIdleAnimation(player, idleToPlay);
                    }
                    return RE::BSEventNotifyControl::kStop;
                }
            }
        }

        if (isBlockButtonPressed) {
            if (buttonEvent->IsDown()) {
                player->NotifyAnimationGraph("blockStart");
                player->SetGraphVariableBool("IsBlocking", true);
                player->SetGraphVariableInt("BFCO_IsBlocking", 1);
                isBlocking = true;
            } else if (buttonEvent->IsUp()) {
                player->SetGraphVariableBool("IsBlocking", false);
                player->NotifyAnimationGraph("blockStop");
                player->SetGraphVariableInt("BFCO_IsBlocking", 0);
                isBlocking = false;
            }
            return RE::BSEventNotifyControl::kStop;
        }

        const std::string bfco = "SCSI-ACTbfco-Main.esp";
        if (Settings::bPowerAttackLMB) {
            // Mapeia nossas variáveis C++ para os EditorIDs das Globals no .esp
            std::map<const char*, float> globalsToUpdate = {{"bfcoTG_LmbPowerAttackNUM", 1.0f}};

            for (auto const& [editorID, value] : globalsToUpdate) {
                RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
                if (global) {
                    global->value = value;
                    // SKSE::log::info("Global '{}' atualizada para o valor: {}", editorID, value);
                } else {
                    // SKSE::log::warn("Nao foi possivel encontrar a GlobalVariable: {}", editorID);
                }
            }
        }

        // 1. VERIFICAR AÇÕES CUSTOMIZADAS PRIMEIRO (APENAS QUANDO O BOTÃO É PRESSIONADO)
        if (buttonEvent->IsDown()) {
            bool powerAttackKeyPressed = false;
            bool comboKeyPressed = false;
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

            if (powerAttackKeyPressed) {
                if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 0) {
                    isPlayerSprinting = false;                   // Corrige o estado se a stamina acabar
                    return RE::BSEventNotifyControl::kContinue;  // Não faz o ataque de sprint
                }
                if (!Settings::bEnableDirectionalAttack) {
                    RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("bfcoTG_DirPowerAttack");
                    if (global) {
                        global->value = 0.0f;
                        // SKSE::log::info("Global '{}' atualizada para o valor: {}", editorID, value);
                    } else {
                        // SKSE::log::warn("Nao foi possivel encontrar a GlobalVariable: {}", editorID);
                    }
                }
                player->NotifyAnimationGraph("MCO_EndAnimation");
                PerformAction(PowerRight, player);

                return RE::BSEventNotifyControl::kStop;
            }

            if (comboKeyPressed) {
                bool hasCombo = false;
                if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 0) {
                    isPlayerSprinting = false;
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (Settings::hasCMF) {
                    player->GetGraphVariableBool("BFCO_HasCombo", hasCombo);
                    if (hasCombo) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                        if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                            PlayIdleAnimation(player, idleToPlay);
                        }
                    } else {
                        // Este é o ataque para quando você está PARADO e sem combo.
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PerformAction(PowerRight, player);
                    }
                    return RE::BSEventNotifyControl::kStop;
                }

                else {
                    player->NotifyAnimationGraph("MCO_EndAnimation");
                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                        PlayIdleAnimation(player, idleToPlay);
                    }

                    isPlayerSprinting = false;
                    return RE::BSEventNotifyControl::kStop;
                }
            }
            return RE::BSEventNotifyControl::kStop;
        }

        // 2. SE O BOTÃO DE BLOQUEIO FOI ACIONADO (MOUSE OU CONTROLE)...
        // if (isBlockButtonPressed && RE::PlayerCamera::GetSingleton()->IsInThirdPerson()) {
        //    // --- VERIFICAÇÃO DE ARMA/MAGIA ---
        //    bool canBlock = true;
        //    auto rightHandItem = player->GetEquippedObject(false);  // false para mão direita
        //    auto leftHandItem = player->GetEquippedObject(true);    // true para mão esquerda

        //    // Checa a mão direita
        //    if (rightHandItem) {
        //        // Se for uma magia...
        //        if (rightHandItem->Is(RE::FormType::Spell)) {
        //            canBlock = false;
        //        }
        //        // Ou se for uma arma do tipo cajado...
        //        else if (auto weapon = rightHandItem->As<RE::TESObjectWEAP>(); weapon && weapon->IsStaff()) {
        //            canBlock = false;
        //        }
        //    }

        //    // Checa a mão esquerda (só se a direita já não tiver invalidado o bloqueio)
        //    if (canBlock && leftHandItem) {
        //        // Se for uma magia...
        //        if (leftHandItem->Is(RE::FormType::Spell)) {
        //            canBlock = false;
        //        }
        //        // Ou se for uma arma do tipo cajado...
        //        else if (auto weapon = leftHandItem->As<RE::TESObjectWEAP>(); weapon && weapon->IsStaff()) {
        //            canBlock = false;
        //        }
        //    }
        //    // --- FIM DA VERIFICAÇÃO ---

        //    // Se, após todas as checagens, o jogador PODE bloquear...
        //    if (canBlock) {
        //        if (buttonEvent->IsDown()) {
        //            player->NotifyAnimationGraph("blockStart");
        //            player->SetGraphVariableBool("IsBlocking", true);
        //            return RE::BSEventNotifyControl::kStop;  // Consome o input
        //        } else if (buttonEvent->IsUp()) {
        //            player->SetGraphVariableBool("IsBlocking", false);
        //            player->NotifyAnimationGraph("blockStop");
        //            return RE::BSEventNotifyControl::kStop;  // Consome o input
        //        }
        //    }
        //}

        if (Settings::bPowerAttackLMB) {
            // Mapeia nossas variáveis C++ para os EditorIDs das Globals no .esp
            std::map<const char*, float> disable = {{"bfcoTG_LmbPowerAttackNUM", 0.0f}};

            for (auto const& [editorID, value] : disable) {
                RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
                if (global) {
                    global->value = value;
                    // SKSE::log::info("Global '{}' atualizada para o valor: {}", editorID, value);
                } else {
                    // SKSE::log::warn("Nao foi possivel encontrar a GlobalVariable: {}", editorID);
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kStop;
}

//// Processa eventos vindos do motor de animação
// RE::BSEventNotifyControl AttackStateManager::ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
//                                                           RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) {
//     if (!a_event || !a_event->holder || !a_event->holder->IsPlayerRef()) {
//         return RE::BSEventNotifyControl::kContinue;
//     }
//
//     std::string eventName = a_event->tag.c_str();
//
//     // Eventos que ABREM a janela de combo
//     if (eventName == "MCO_WinOpen" || eventName == "MCO_PowerWinOpen" || eventName == "BFCO_NextWinStart" ||
//         eventName == "BFCO_NextPowerWinStart") {
//         if (!this->inAttackComboWindow) {
//             SKSE::log::info(">>> LÓGICA: Janela de combo ABERTA pelo evento: {}.", eventName);
//             this->inAttackComboWindow = true;
//         }
//         // Eventos que FECHAM a janela de combo
//     } else if (eventName == "MCO_WinClose" || eventName == "MCO_PowerWinClose" || eventName == "BFCO_DIY_EndLoop" ||
//                eventName == "AttackWinEnd" || eventName == "MCO_Recovery" || eventName == "BFCO_DIY_recovery" ||
//                eventName == "AttackStop") {
//         if (this->inAttackComboWindow) {
//             SKSE::log::info(">>> LÓGICA: Janela de combo FECHADA pelo evento: {}.", eventName);
//             this->inAttackComboWindow = false;
//         }
//     }
//
//     return RE::BSEventNotifyControl::kContinue;
// }
//
// bool AttackStateManager::HasEnoughStamina(RE::PlayerCharacter* player) {
//     // Podemos começar com um valor fixo para testar. 30 é um custo razoável para um power attack.
//     float requiredStamina = 30.0f;
//
//     float currentStamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
//
//     if (currentStamina >= requiredStamina) {
//         return true;
//     } else {
//         SKSE::log::warn("--- VERIFICAÇÃO FALHOU: Vigor insuficiente. Necessário: {}, Atual: {}", requiredStamina,
//                         currentStamina);
//         // Opcional: Adicionar um som de "falha" ou efeito visual aqui, como piscar a barra de vigor.
//         return false;
//     }
// }
