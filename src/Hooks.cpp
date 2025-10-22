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
RE::BGSAction* RightAttack = nullptr;
RE::BGSAction* PowerAttack = nullptr;
RE::BGSAction* Bash = nullptr;



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
    /*auto* gsc = RE::GameSettingCollection::GetSingleton();
    RE::Setting* setting = gsc->GetSetting("fInitialPowerAttackDelay");
    float newDelay = 1100.0f / 1000.0f;
    setting->data.f = newDelay;*/

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

        if (IsAnyMenuOpen()) {
            return RE::BSEventNotifyControl::kContinue;
        }
       
        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        const auto playerState = player->AsActorState();
        

        if (!(!player->IsInKillMove() && playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
              playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetFlyState() == RE::FLY_STATE::kNone)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (device == RE::INPUT_DEVICE::kMouse) {
            keyCode += 256;  // Ajusta o código do mouse para corresponder às configurações
        }

        bool isBlockBtnPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::BlockKey_k) ||
                                 (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::BlockKey_m) ||
                                 (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::BlockKey_g);
        
        bool isAttackBtnPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::AttackKey_k) ||
                                  (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::AttackKey_m) ||
                                  (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::AttackKey_g);

        bool isPowerAttackKeyPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::PowerAttackKey_k) ||
                                       (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::PowerAttackKey_m) ||
                                       (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::PowerAttackKey_g);

        

        bool comboKeyPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::comboKey_k) ||
                                 (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::comboKey_m) ||
                                 (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::comboKey_g);


       
       
           
        bool isStrong = false;
        bool canAttack = false;
        bool hasCombo = false;
        player->GetGraphVariableBool("BFCO_IsPlayerInputOK", canAttack);
        player->GetGraphVariableBool("ADTF_ShouldDelay", isStrong);
        float currentStamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        auto* PowerNormal = GetIdleByFormID(0x8C5, pluginName);
        auto* PowerH2H = GetIdleByFormID(0x839, pluginName);
        auto* PowerBash = GetIdleByFormID(0x8C0, pluginName);
        auto* SprintPower = GetIdleByFormID(0x8BE, pluginName);
        auto* ComboAttackE = GetIdleByFormID(0x8BF, pluginName);
        auto* BlockStart = GetIdleByFormID(0x13217, skyrim);
        auto* BashStart = GetIdleByFormID(0x1B417, skyrim);
        auto* BashRelease = GetIdleByFormID(0x1457A, skyrim);
        auto* BlockRelease = GetIdleByFormID(0x13ACA, skyrim);
        auto* Idle = GetActionByFormID(0x13002, skyrim);
        auto* Dodge = GetIdleByFormID(0x935, pluginName);
        int isDodging = 0;
        int revocery = 0;

         if (isBlockBtnPressed) {
            if (buttonEvent->IsDown()) {
                Settings::_isCurrentlyBlocking = true;
                player->SetGraphVariableInt("BFCO_IsBlocking", 1);
                player->SetGraphVariableBool("IsBlocking", true);
                PlayIdleAnimation(player, BlockStart);
            }

            if (buttonEvent->IsUp()) {
                Settings::_isCurrentlyBlocking = false;
                player->NotifyAnimationGraph("blockStop");
                PlayIdleAnimation(player, BlockRelease);
                
            }
        }

        if (buttonEvent->IsDown()) {
            
            if (isAttackBtnPressed) {

               if(player->NotifyAnimationGraph("MCO_EndAnimation")){
                    logger::info("MCO_EndAnimation notified.");
               }

                player->GetGraphVariableInt("TDM_Dodge", isDodging);
                player->GetGraphVariableInt("MCO_IsInRecovery", revocery);
                
                if (Dodge->conditions.IsTrue(player, player)) {
                    player->NotifyAnimationGraph("attackStart");
                }
                if (Settings::_isCurrentlyBlocking) {
                    if (currentStamina > 0) {
                            PlayIdleAnimation(player, BashStart);
                    } else {
                        player->NotifyAnimationGraph("bashFail");
                    }
                    
                }
                else if (canAttack) {
                    player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);  
                }
                else if (isStrong) {
                    player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                }
            }

            if (isPowerAttackKeyPressed) {
                if (currentStamina <= 0) {
                    return RE::BSEventNotifyControl::kContinue;  
                }
                    if (SprintPower->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, SprintPower);
                    } else if (PowerBash->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerBash);
                    } else if (PowerH2H->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerH2H);
                    } else {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                        PlayIdleAnimation(player, PowerNormal);
                        //PerformAction(PowerRight, player);
                    }
                
            }
            if (Settings::bEnableComboAttack) {
                if (comboKeyPressed) {
                    
                    if (currentStamina <= 0) {
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    if (Settings::hasCMF) {
                        player->GetGraphVariableBool("BFCO_HasCombo", hasCombo);
                        if (hasCombo) {
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            if (SprintPower->conditions.IsTrue(player, player)) {
                                PlayIdleAnimation(player, SprintPower);
                            } else if (PowerBash->conditions.IsTrue(player, player)) {
                                PlayIdleAnimation(player, PowerBash);
                            } else if (PowerH2H->conditions.IsTrue(player, player)) {
                                 PlayIdleAnimation(player, PowerH2H);
                            } else {
                                player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                PlayIdleAnimation(player, ComboAttackE);
                            }
                            
                        } else {
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PerformAction(PowerRight, player);
                        }
                        return RE::BSEventNotifyControl::kContinue;
                    } else if (SprintPower->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, SprintPower);
                    } else if (PowerBash->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerBash);
                    } else if (PowerH2H->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerH2H);
                    } else {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                        PlayIdleAnimation(player, ComboAttackE);
                    }
                    
                }
            } 
        } 

        if (buttonEvent->IsUp()) {
            if (isPowerAttackKeyPressed) {
                player->NotifyAnimationGraph("BFCOAttackstart_1");
            }
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}


RE::BSEventNotifyControl GlobalControl::AnimationEventHandler::ProcessEvent(
    const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    auto player = RE::PlayerCharacter::GetSingleton();

    if (a_event && a_event->holder && a_event->holder->IsPlayerRef()) {
        const std::string_view eventName = a_event->tag;
       
        if (eventName == "Bfco_AttackStartFX") {
            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);


        } else if (eventName == "bashStop") {
            if (Settings::_isCurrentlyBlocking) {
                player->NotifyAnimationGraph("blockStart");
            }
        }else if (eventName == "MCO_IsInRecovery") {
            logger::info("Animation Event: MCO_IsInRecovery received.");
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}
