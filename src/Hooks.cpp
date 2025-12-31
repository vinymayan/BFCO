#include "Hooks.h"
#include "Settings.h"


// Declaramos as variáveis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
extern const SKSE::TaskInterface* g_task;

const std::string pluginName = "SCSI-ACTbfco-Main.esp";
const std::string skyrim = "Skyrim.esm";
const std::string update = "Update.esm";
const std::string dragon = "PlayableDragon.esp";
const std::string bfco = "BFCO NG.esp";

RE::BGSAction* PowerRight = nullptr;
RE::BGSAction* Draw = nullptr;
RE::BGSAction* Seath = nullptr;
RE::BGSAction* PowerStanding = nullptr;
RE::BGSAction* PowerLeft = nullptr;
RE::BGSAction* PowerDual = nullptr;
RE::BGSAction* NormalAttack = nullptr;
RE::BGSAction* RightAttack = nullptr;
RE::BGSAction* PowerAttack = nullptr;
RE::BGSAction* Bash = nullptr;

RE::TESIdleForm* PowerNormal = nullptr;
RE::TESIdleForm* AutoAA = nullptr;
RE::TESIdleForm* AutoAABow = nullptr;
RE::TESIdleForm* PowerH2H = nullptr;
RE::TESIdleForm* PowerBash = nullptr;
RE::TESIdleForm* SprintPower = nullptr;
RE::TESIdleForm* ComboAttackE = nullptr;
RE::TESIdleForm* BlockStart = nullptr;
RE::TESIdleForm* BashStart = nullptr;
RE::TESIdleForm* BashRelease = nullptr;
RE::TESIdleForm* BlockRelease = nullptr;
RE::TESIdleForm* Dodge = nullptr;
RE::TESIdleForm* TailSmash = nullptr;


std::set<uint32_t> pressedKeys_K; // Teclado
std::set<uint32_t> pressedKeys_M; // Mouse
std::set<uint32_t> pressedKeys_G; // Gamepad

bool IsKeyPressed(uint32_t key) {
    // No teu código, definimos que teclas do rato são >= 256 (devido ao offset que adicionaste)
    if (key >= 256) {
        return pressedKeys_M.count(key) > 0;
    }
    // Teclas de Gamepad (se usares códigos muito altos para gamepad, ajusta aqui, mas geralmente são tratados à parte)
    // Assumindo que o resto é teclado:
    return pressedKeys_K.count(key) > 0;
}


bool CheckKeyCombination(uint32_t eventKeyCode, RE::INPUT_DEVICE device,
    uint32_t settingKey, uint32_t settingMod) {

    // Se a tecla principal da configuração for 0, está desativado
    if (settingKey == 0) return false;

    // Se NÃO houver modificador (ou for 0), é uma tecla simples
    if (settingMod == 0) {
        return eventKeyCode == settingKey;
    }

    // LÓGICA DE COMBINAÇÃO:

    // Caso 1: Acabaste de apertar a TECLA PRINCIPAL (ex: RMB)
    if (eventKeyCode == settingKey) {
        // Precisamos verificar se o MODIFICADOR (ex: Ctrl) já está pressionado.
        // Usamos IsKeyPressed para procurar o Ctrl no teclado, mesmo que o evento atual seja do mouse.
        return IsKeyPressed(settingMod);
    }

    // Caso 2: Acabaste de apertar o MODIFICADOR (ex: Ctrl)
    if (eventKeyCode == settingMod) {
        // Precisamos verificar se a TECLA PRINCIPAL (ex: RMB) já está pressionada (segurada).
        return IsKeyPressed(settingKey);
    }

    return false;
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

    Settings::AttackKeyLeft_k = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kKeyboard);
	logger::info("AttackKeyLeft_k: {}", Settings::AttackKeyLeft_k);
    Settings::AttackKeyLeft_m = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kMouse);
	logger::info("AttackKeyLeft_m: {}", Settings::AttackKeyLeft_m);
    Settings::AttackKeyLeft_m += 256;
    Settings::AttackKeyLeft_g = controlMap->GetMappedKey(userEvents->leftAttack, RE::INPUT_DEVICE::kGamepad);
	logger::info("AttackKeyLeft_g (before conversion): {}", Settings::AttackKeyLeft_g);
}

bool IsLeftHandNotWeapon(RE::Actor* a_actor) {
    if (!a_actor) {
        return true;
    }

    // O argumento 'true' em GetEquippedObject pega a mão esquerda.
    auto equippedObj = a_actor->GetEquippedObject(true);

    // Se não houver objeto equipado (nullptr), a mão está vazia, logo não é arma.
    if (!equippedObj) {
        return false;
    }

    // Verifica se o tipo do formulário é Weapon (Arma).
    // Se for Weapon, retorna false (porque queremos verificar se NÃO é uma arma).
    if (equippedObj->GetFormType() == RE::FormType::Weapon) {
        return false;
    }

    // Se chegou aqui, é porque é Escudo, Magia, Tocha ou outro item que não seja arma.
    return true;
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
        auto rawKeyCode = buttonEvent->GetIDCode();
        auto keyCode = rawKeyCode;
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

        if (buttonEvent->IsDown()) {
            if (device == RE::INPUT_DEVICE::kKeyboard) pressedKeys_K.insert(keyCode);
            else if (device == RE::INPUT_DEVICE::kMouse) pressedKeys_M.insert(keyCode);
            else if (device == RE::INPUT_DEVICE::kGamepad) pressedKeys_G.insert(keyCode);
        }
        else if (buttonEvent->IsUp()) {
            if (device == RE::INPUT_DEVICE::kKeyboard) pressedKeys_K.erase(keyCode);
            else if (device == RE::INPUT_DEVICE::kMouse) pressedKeys_M.erase(keyCode);
            else if (device == RE::INPUT_DEVICE::kGamepad) pressedKeys_G.erase(keyCode);
        }

        bool isBlockBtnPressed = false;
        bool isAttackBtnPressed = false;
        bool isLAttackBtnPressed = false;
        bool isPowerAttackKeyPressed = false;
        bool comboKeyPressed = false;

        if (CheckKeyCombination(keyCode, device, Settings::BlockKey_k, Settings::BlockKey_k_mod))
            isBlockBtnPressed = true;
        // Verifica Config Principal Mouse (se ainda não encontrou)
        if (!isBlockBtnPressed && CheckKeyCombination(keyCode, device, Settings::BlockKey_m, Settings::BlockKey_m_mod))
            isBlockBtnPressed = true;
        // Verifica Gamepad (apenas se o input atual for gamepad para evitar conflitos de ID)
        if (!isBlockBtnPressed && device == RE::INPUT_DEVICE::kGamepad) {
            if (CheckKeyCombination(keyCode, device, Settings::BlockKey_g, Settings::BlockKey_g_mod))
                isBlockBtnPressed = true;
        }

        // --- POWER ATTACK ---
        if (CheckKeyCombination(keyCode, device, Settings::PowerAttackKey_k, Settings::PowerAttackKey_k_mod))
            isPowerAttackKeyPressed = true;
        if (!isPowerAttackKeyPressed && CheckKeyCombination(keyCode, device, Settings::PowerAttackKey_m, Settings::PowerAttackKey_m_mod))
            isPowerAttackKeyPressed = true;
        if (!isPowerAttackKeyPressed && device == RE::INPUT_DEVICE::kGamepad) {
            if (CheckKeyCombination(keyCode, device, Settings::PowerAttackKey_g, Settings::PowerAttackKey_g_mod))
                isPowerAttackKeyPressed = true;
        }

        // --- COMBO ATTACK ---
        if (CheckKeyCombination(keyCode, device, Settings::comboKey_k, Settings::comboKey_k_mod))
            comboKeyPressed = true;
        if (!comboKeyPressed && CheckKeyCombination(keyCode, device, Settings::comboKey_m, Settings::comboKey_m_mod))
            comboKeyPressed = true;
        if (!comboKeyPressed && device == RE::INPUT_DEVICE::kGamepad) {
            if (CheckKeyCombination(keyCode, device, Settings::comboKey_g, Settings::comboKey_g_mod))
                comboKeyPressed = true;
        }

        // --- NORMAL ATTACK (Mantém lógica simples ou adapta se quiser mods também) ---
        
        if (device == RE::INPUT_DEVICE::kKeyboard) isAttackBtnPressed = (keyCode == Settings::AttackKey_k);
        else if (device == RE::INPUT_DEVICE::kMouse) isAttackBtnPressed = (keyCode == Settings::AttackKey_m);
        else if (device == RE::INPUT_DEVICE::kGamepad) isAttackBtnPressed = (keyCode == Settings::AttackKey_g);

        if (device == RE::INPUT_DEVICE::kKeyboard) isLAttackBtnPressed = (keyCode == Settings::AttackKeyLeft_k);
        else if (device == RE::INPUT_DEVICE::kMouse) isLAttackBtnPressed = (keyCode == Settings::AttackKeyLeft_m);
        else if (device == RE::INPUT_DEVICE::kGamepad) isLAttackBtnPressed = (keyCode == Settings::AttackKeyLeft_g);

       
       
           
        int isStrong = false;
        int canAttack = false;
        bool hasCombo = false;
        player->GetGraphVariableInt("BFCO_IsPlayerInputOK", canAttack);
        player->GetGraphVariableInt("ADTF_ShouldDelay", isStrong);
        float currentStamina = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
        int isDodging = 0;
        int revocery = 0;

        
     
         if (isBlockBtnPressed) {
             if (Settings::disableDualblock) {
                 if (IsLeftHandNotWeapon(player)) {
                     return RE::BSEventNotifyControl::kContinue;
                 }
             }
             
             if (buttonEvent->IsDown()) {
                    Settings::_isCurrentlyBlocking = true;
                    player->NotifyAnimationGraph("MCO_AnimStop");
                    player->NotifyAnimationGraph("MCO_EndAnimation");
                    player->SetGraphVariableInt("BFCONG_Block", 1);
                    player->SetGraphVariableBool("IsBlocking", true);
                    player->NotifyAnimationGraph("blockStart");
                    PlayIdleAnimation(player, BlockStart);
                    //PerformAction(ReleaseBlock, player);
             }
             else if (buttonEvent->IsHeld()) {
                 Settings::_isCurrentlyBlocking = true;
             }
             else if (buttonEvent->IsUp()) {
                 bool releasedBlockKey = false;
                 if (device == RE::INPUT_DEVICE::kKeyboard) {
                     if (keyCode == Settings::BlockKey_k || (Settings::BlockKey_k_mod != 0 && keyCode == Settings::BlockKey_k_mod)) releasedBlockKey = true;
                 }
                 else if (device == RE::INPUT_DEVICE::kMouse) {
                     if (keyCode == Settings::BlockKey_m || (Settings::BlockKey_m_mod != 0 && keyCode == Settings::BlockKey_m_mod)) releasedBlockKey = true;
                 }
                 else if (device == RE::INPUT_DEVICE::kGamepad) {
                     if (keyCode == Settings::BlockKey_g || (Settings::BlockKey_g_mod != 0 && keyCode == Settings::BlockKey_g_mod)) releasedBlockKey = true;
                 }

                 if (releasedBlockKey && Settings::_isCurrentlyBlocking) {
                     Settings::_isCurrentlyBlocking = false;
                     player->NotifyAnimationGraph("MCO_EndAnimation");
                     player->SetGraphVariableInt("BFCO_IsPlayerInputOK", 0);
                     player->SetGraphVariableInt("ADTF_ShouldDelay", 0);
                     player->SetGraphVariableInt("BFCONG_Block", 0);
                     player->SetGraphVariableBool("IsBlocking", false);
                     player->NotifyAnimationGraph("blockStop");
                     if (BlockRelease->conditions.IsTrue(player, player)) {
                        //logger::info("Aaaaaa 1held condition is true.");
                         PlayIdleAnimation(player, BlockRelease);
                     }
                     player->SetGraphVariableInt("BFCONG_Block", 1);
                     releaseBlock = true;

                 }
             }
         }

        if (buttonEvent->IsDown()) {
             player->GetGraphVariableInt("TDM_Dodge", isDodging);
             player->GetGraphVariableInt("MCO_IsInRecovery", revocery);
            if (isAttackBtnPressed) {

               player->NotifyAnimationGraph("MCO_EndAnimation");

               if (Settings::bPowerAttackLMB > 0) {
                   player->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 1);
               }
               else {
				   player->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 0);
               }
               if (Settings::_isCurrentlyBlocking) {
                   if (Settings::disableMStaBash) {
                       Settings::_isCurrentlyBlocking = false;
                       player->SetGraphVariableBool("IsBlocking", false);
                       player->SetGraphVariableInt("BFCONG_Block", 0);
                       player->NotifyAnimationGraph("blockStop");
                       PerformAction(NormalAttack, player);
                   }
                   else {
                       if (currentStamina > 0) {
                           PlayIdleAnimation(player, BashStart);
                       }
                       else {
                           player->NotifyAnimationGraph("bashFail");
                       }
                   }
               }
               if (canAttack == 1) {
                   player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
               }
               else if (isStrong == 1) {
                   if (Settings::bPowerAttackLMB > 0) {
                       player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                       player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
                       if (AutoAABow->conditions.IsTrue(player, player)) {
                           //logger::info("AutoAA triggered by 1held condition is true.");
                           player->NotifyAnimationGraph("MCO_EndAnimation");
                           PlayIdleAnimation(player, AutoAABow);
                       }
                       else if (AutoAA->conditions.IsTrue(player, player)) {
                           //logger::info("AutoAA condition is true.");
                           player->NotifyAnimationGraph("MCO_EndAnimation");
                           PlayIdleAnimation(player, AutoAA);
                       }
                   }
                   else {
                       player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                   }
                   
               }
               
                
            }
            if (Settings::bEnablePowerAttack) {
                if (isPowerAttackKeyPressed) {
                    if (currentStamina <= 0) {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    if (Settings::PowerAttackKey_m == Settings::AttackKeyLeft_m || Settings::PowerAttackKey_g == Settings::AttackKeyLeft_g || Settings::PowerAttackKey_k == Settings::AttackKeyLeft_k) {
                        if (IsLeftHandNotWeapon(player)) {
                            return RE::BSEventNotifyControl::kContinue;
                        }
                    }

                    bool isLeftHandInvolved = false;

                    if (device == RE::INPUT_DEVICE::kKeyboard) {
                        // Verifica se Left Attack é a tecla PA ou o Modificador PA no teclado
                        if (Settings::PowerAttackKey_k == Settings::AttackKeyLeft_k ||
                            (Settings::PowerAttackKey_k_mod != 0 && Settings::PowerAttackKey_k_mod == Settings::AttackKeyLeft_k)) {
                            isLeftHandInvolved = true;
                        }
                    }
                    else if (device == RE::INPUT_DEVICE::kMouse) {
                        // Verifica no mouse
                        if (Settings::PowerAttackKey_m == Settings::AttackKeyLeft_m ||
                            (Settings::PowerAttackKey_m_mod != 0 && Settings::PowerAttackKey_m_mod == Settings::AttackKeyLeft_m)) {
                            isLeftHandInvolved = true;
                        }
                    }
                    else if (device == RE::INPUT_DEVICE::kGamepad) {
                        // Verifica no gamepad
                        if (Settings::PowerAttackKey_g == Settings::AttackKeyLeft_g ||
                            (Settings::PowerAttackKey_g_mod != 0 && Settings::PowerAttackKey_g_mod == Settings::AttackKeyLeft_g)) {
                            isLeftHandInvolved = true;
                        }
                    }

                    // Se a mão esquerda estiver envolvida na combinação que disparou o PA:
                    if (isLeftHandInvolved) {
						//logger::info("Power Attack triggered with Left Hand involved.");
                        player->SetGraphVariableInt("BFCONG_PARMB", 1);
                    }

                    if (Settings::bPowerAttackLMB > 0 && isAttackBtnPressed && !isLeftHandInvolved) {
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    if (SprintPower->conditions.IsTrue(player, player)) {
                        //logger::info("SprintPower condition is true.");
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, SprintPower);
                    } else if (!Settings::disableMStaBash && PowerBash->conditions.IsTrue(player, player)) {
                        //logger::info("PowerBash condition is true.");
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerBash);
                    } else if (PowerH2H->conditions.IsTrue(player, player)) {
                        //logger::info("PowerH2H condition is true.");
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerH2H);
                    } else {
                        if (Dodge->conditions.IsTrue(player, player)) {
                            //logger::info("Dodge condition is true.");
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PlayIdleAnimation(player, Dodge);
                        }
                        //logger::info("strong {}, canAttack {}", isStrong, canAttack);
                        player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                        player->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 0);
                        
                            if (!Settings::DodgeCancel) {
                                player->NotifyAnimationGraph("MCO_EndAnimation");
                                Settings::DodgeCancel = false;
                            }
                            if (Settings::bEnableDirectionalAttack) {
                                PerformAction(PowerRight, player);
                            } else {
                                PlayIdleAnimation(player, PowerNormal);
                            }
                            //player->SetGraphVariableInt("BFCONG_PARMB", 0);
                    }
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
                            } else {
                                player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                PlayIdleAnimation(player, ComboAttackE);
                            }

                        } else {
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PerformAction(PowerRight, player);
                        }
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    else if(SprintPower->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, SprintPower);
                    } else if (PowerBash->conditions.IsTrue(player, player)) {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        PlayIdleAnimation(player, PowerBash);
                    }  else {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                        PlayIdleAnimation(player, ComboAttackE);
                    }
                    
                }
            } 
    //        if (isLAttackBtnPressed) {
    //            //player->NotifyAnimationGraph("attackStartTail");
				//logger::info("Tail Smash Attack Triggered");
				//PlayIdleAnimation(player, TailSmash);
    //        }
            
        } 

        if (buttonEvent->IsHeld()) {
            if (isAttackBtnPressed) {
                if (Settings::bPowerAttackLMB == 2) {
                    if (isStrong == 1) {
                        player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                        player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
                        if (AutoAA->conditions.IsTrue(player, player)) {
                            //logger::info("AutoAA triggered by held condition is true.");
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PlayIdleAnimation(player, AutoAA);
                        }
                    }
                    
                }
            }
            if (isLAttackBtnPressed) {
                if (Settings::bPowerAttackLMB) {
                    bool isRangedWeapon = false;
                    // 'false' pega a mão direita (Main Hand), onde o Arco é registrado
                    auto equippedObj = player->GetEquippedObject(false);
                    if (equippedObj && equippedObj->IsWeapon()) {
                        auto weapon = equippedObj->As<RE::TESObjectWEAP>();
                        if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow ||
                            weapon->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow) {
                            isRangedWeapon = true;
                        }
                    }
                    if (isRangedWeapon) {
                        if (AutoAABow->conditions.IsTrue(player, player)) {
                            //logger::info("AutoAA triggered by bow held condition is true.");
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PlayIdleAnimation(player, AutoAABow);
                        }
                    }
                }
            }
        
		}

        if (buttonEvent->IsUp()) {
            if (Settings::bEnablePowerAttack) {
                if (isPowerAttackKeyPressed) {
                    bool releasedPAKey = false;
                    if (device == RE::INPUT_DEVICE::kKeyboard) {
                        if (keyCode == Settings::PowerAttackKey_k || (Settings::PowerAttackKey_k_mod != 0 && keyCode == Settings::PowerAttackKey_k_mod)) releasedPAKey = true;
                    }
                    else if (device == RE::INPUT_DEVICE::kMouse) {
                        if (keyCode == Settings::PowerAttackKey_m || (Settings::PowerAttackKey_m_mod != 0 && keyCode == Settings::PowerAttackKey_m_mod)) releasedPAKey = true;
                    }
                    else if (device == RE::INPUT_DEVICE::kGamepad) {
                        if (keyCode == Settings::PowerAttackKey_g || (Settings::PowerAttackKey_g_mod != 0 && keyCode == Settings::PowerAttackKey_g_mod)) releasedPAKey = true;
                    }

                    if (releasedPAKey) {
                       // logger::info("Power Attack Key Released");
                        player->NotifyAnimationGraph("BFCOAttackstart_1");
                    }
                }
            }
        }

    }
    return RE::BSEventNotifyControl::kContinue;
}


RE::BSEventNotifyControl GlobalControl::AnimationEventHandler::ProcessEvent(
    const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    auto player = RE::PlayerCharacter::GetSingleton();
    const std::string_view eventName = a_event->tag;
    
    if (a_event && a_event->holder && a_event->holder->IsPlayerRef()) {
		bool relBlock = false;
        if (eventName == "Bfco_AttackStartFX") {
            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
            //player->SetGraphVariableInt("BFCONG_PARMB", 0);
            player->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 0);

        } else if (eventName == "bashStop") {
            if (Settings::_isCurrentlyBlocking) {
                player->NotifyAnimationGraph("blockStart");
            }
        } else if (eventName == "MCO_DodgeOpen") {
            Settings::DodgeCancel = true;
        } else if (eventName == "weaponSwing" || eventName == "weaponLeftSwing" ||
            eventName == "h2hAttack" || eventName == "PowerAttack_Start_end") {

            player->SetGraphVariableInt("BFCONG_PARMB", 0);

          }
        else if (eventName == "attackStop") {
            relBlock = true;
        }

        if (!Settings::_isCurrentlyBlocking && relBlock && releaseBlock) {
           player->SetGraphVariableInt("BFCONG_Block", 0);
           player->NotifyAnimationGraph("blockStop");
           PlayIdleAnimation(player, BlockRelease);
           
           releaseBlock = false;
           relBlock = false;
		   player->SetGraphVariableInt("BFCONG_Block", 1);
        }

        /*else if(eventName == "CastStop" && !Settings::_isCurrentlyBlocking) {
            player->NotifyAnimationGraph("tailCombatIdle");
		} */  
        //else if (eventName == "MCO_WindowOpen" || eventName == "BFCO_NextWinStart") {
        //    // Verifica se o modo automático deve estar ativo
        //    // (Lógica: Se bPowerAttackLMB for false, assume-se comportamento automático no LMB)
        //    if (Settings::bPowerAttackLMB) {

        //        bool isAttackHeld = false;

        //        // Verifica se a tecla de ataque está sendo segurada no Teclado, Mouse ou Gamepad
        //        if (IsKeyPressed(Settings::AttackKey_k) || IsKeyPressed(Settings::AttackKey_m)) {
        //            isAttackHeld = true;
        //        }
        //        // Se quiser suporte a gamepad, adicione a verificação do pressedKeys_G aqui também

        //        if (isAttackHeld) {
        //            SKSE::log::info("Auto Attack Triggered by Hold"); // Debug se necessário
        //            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
        //            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
        //            if (AutoAA->conditions.IsTrue(player, player)) {
        //                logger::info("AutoAA triggered by window condition is true.");
        //                player->NotifyAnimationGraph("MCO_EndAnimation");
        //                PlayIdleAnimation(player, AutoAA);
        //            }

        //        }
        //    }
        //}
        
    }
    
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl GlobalControl::PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {
    if (!a_event || !a_event->loaded) return RE::BSEventNotifyControl::kContinue;

    // Filtra EXTRITAMENTE para o Player (FormID 0x14)
    if (a_event->formID == 0x14) {
        // Evita spam de requests se já estiver processando
        if (isProcessingRegistration) return RE::BSEventNotifyControl::kContinue;
        isProcessingRegistration = true;

        // Não precisamos de std::thread + sleep aqui necessariamente se usarmos a Task Interface
        // O jogo acabou de carregar o 3D, mas o Graph pode demorar alguns frames.
        // Usamos uma task simples para rodar no próximo frame lógico.
        SKSE::GetTaskInterface()->AddTask([]() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                // Verifica se o Graph Manager existe antes de prosseguir
                RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
                player->GetAnimationGraphManager(graphManager);

                if (graphManager) {
                    player->RemoveAnimationGraphEventSink(AnimationEventHandler::GetSingleton());
                    player->AddAnimationGraphEventSink(AnimationEventHandler::GetSingleton());
                }
                else {
                    // Se falhar, aí sim tentamos agendar uma nova verificação (fail-safe)
                    // Mas na maioria dos casos, no LoadedEvent o graph já está inicializando
                    isProcessingRegistration = false;
                }
            }
            else {
                isProcessingRegistration = false;
            }
            });
    }
    return RE::BSEventNotifyControl::kContinue;
}
