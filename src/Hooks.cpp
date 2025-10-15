#include "Hooks.h"
#include "Settings.h"

// Declaramos as vari�veis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
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

            // SKSE::log::info("Tocando anima��o idle FormID 0x{:X}", a_idle->GetFormID());
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
    // Se este contador for maior que 0, algo est� for�ando o cursor a aparecer.
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

void AddPerk(RE::Actor* a_actor, RE::BGSPerk* a_perk) {
    // 1. Verifica��es de seguran�a para evitar crashes
    if (!a_actor || !a_perk) {
        SKSE::log::warn("AddPerk foi chamado com um ator ou perk nulo.");
        return;
    }

    // 2. A condi��o principal: O ator j� tem este perk?
    if (a_actor->HasPerk(a_perk)) {
        SKSE::log::info("O ator '{}' j� possui o perk '{}'. Nenhuma a��o necess�ria.", a_actor->GetName(),
                        a_perk->GetName());
        return;  // Sai da fun��o se o perk j� existe
    }

    // 3. Se o ator n�o tem o perk, n�s o adicionamos.
    //    Perks s�o adicionados ao 'ActorBase' (TESNPC), que � a "planta" do ator.
    auto actorBase = a_actor->GetActorBase();
    if (actorBase) {
        actorBase->AddPerk(a_perk, 1);  // O '1' � o rank do perk, geralmente 1.
        SKSE::log::info("Perk '{}' adicionado com sucesso ao ator '{}'.", a_perk->GetName(), a_actor->GetName());
    } else {
        SKSE::log::error("N�o foi poss�vel obter o ActorBase para o ator '{}'.", a_actor->GetName());
    }
}

RE::BGSPerk* GetPerkByEditorID(RE::FormID a_formID) {
    auto perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_formID);

    if (!perk) {
        // Este log ser� acionado se o FormID n�o for encontrado ou se o formul�rio encontrado n�o for um perk.
        // Usamos 0x{:X} para formatar o ID em hexadecimal, que � o padr�o para FormIDs.
        SKSE::log::warn("N�o foi poss�vel encontrar o perk com FormID: 0x{:X}", a_formID);
    }

    return perk;
}

bool IsTryingSprintAttack(RE::Actor* a_actor) {
    if (!a_actor) {
        return false;
    }

    // RE::ActorState cont�m flags mais detalhados sobre o estado do ator.
    // IsSprinting() � a verifica��o ideal para o ataque de corrida.
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
    Settings::AttackKey_m += 256;  // Ajusta o c�digo do mouse para corresponder �s configura��es
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
            // logger::info("menu aberto");
            return RE::BSEventNotifyControl::kContinue;
        }
        const auto* userEvents = RE::UserEvents::GetSingleton();

        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        // --- IN�CIO DA L�GICA REORDENADA ---
        const auto playerState = player->AsActorState();


        if (!(!player->IsInKillMove() && playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
              playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
              playerState->GetFlyState() == RE::FLY_STATE::kNone)) {
            isPlayerSprinting = false;
            return RE::BSEventNotifyControl::kContinue;
        }

        if (device == RE::INPUT_DEVICE::kMouse) {
            keyCode += 256;  // Ajusta o c�digo do mouse para corresponder �s configura��es
        }

        bool isAttackBtnPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::AttackKey_k) ||
                                  (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::AttackKey_m) ||
                                  (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::AttackKey_g);

        bool isPowerAttackKeyPressed =
            (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::PowerAttackKey_k) ||
            (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::PowerAttackKey_m) ||
            (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::PowerAttackKey_g);

        bool isBlockBtnPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::BlockKey_k) ||
                                 (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::BlockKey_m) ||
                                 (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::BlockKey_g);

        bool comboKeyPressed = (device == RE::INPUT_DEVICE::kKeyboard && keyCode == Settings::comboKey_k) ||
                                 (device == RE::INPUT_DEVICE::kMouse && keyCode == Settings::comboKey_m) ||
                                 (device == RE::INPUT_DEVICE::kGamepad && keyCode == Settings::comboKey_g);

           
        auto now = std::chrono::steady_clock::now();


        // --- L�GICA DE TESTE SIMPLIFICADA ---

        // EST�GIO 1: Gerenciar nosso estado de bloqueio
        if (isBlockBtnPressed) {
            if (buttonEvent->IsDown()) {
                _isCurrentlyBlocking = true;
                logger::info("[TESTE] Bot�o de Bloqueio PRESSIONADO. _isCurrentlyBlocking = {}", _isCurrentlyBlocking);
                // L�gica de anima��o de bloqueio aqui (opcional para o teste)
                player->NotifyAnimationGraph("blockStart");
                player->SetGraphVariableBool("IsBlocking", true);
                player->SetGraphVariableInt("BFCO_IsBlocking", 1);
            } else if (buttonEvent->IsUp()) {
                _isCurrentlyBlocking = false;
                logger::info("[TESTE] Bot�o de Bloqueio SOLTO. _isCurrentlyBlocking = {}", _isCurrentlyBlocking);
                // L�gica de anima��o para parar de bloquear aqui (opcional para o teste)
                player->SetGraphVariableInt("BFCO_IsBlocking", 0);
                player->SetGraphVariableBool("IsBlocking", false);
                player->NotifyAnimationGraph("blockStop");
            }
        }

        // L�gica de Timeout da Janela de Combo (permanece a mesma)
        if (_attackState == AttackButtonState::kPowerAttackReleased_ComboWindowOpen && now > _comboWindowExpireTime) {
            logger::info("Janela de combo expirou.");
            _attackState = AttackButtonState::kNone;
        }
        bool canCombo = false;
        bool canAttack = false;
        player->GetGraphVariableBool("NEW_BFCO_IsInComboWindow", canCombo);
        logger::info("canCombo: {}", canCombo);

            if (isAttackBtnPressed) {
                player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                player->GetGraphVariableBool("BFCO_IsPlayerInputOK", canAttack);
                logger::info("pode atacar? {}", canAttack);
                
                    if (buttonEvent->IsDown()) {
                        _isAttackButtonPressed = true;
                        _attackButtonPressTime = std::chrono::steady_clock::now();
                            logger::info("Iniciando ataque de combo!");
                            //player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
                        
                    } else if (buttonEvent->IsUp()) {
                        logger::info("Fechou aqui");
                        _isAttackButtonPressed = false;
                        std::chrono::duration<float> holdDuration =
                            std::chrono::steady_clock::now() - _attackButtonPressTime;

                        if (canAttack && holdDuration.count() >= _powerAttackHoldThreshold) {
                            // Foi um segurar longo -> ATAQUE FORTE
                            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                            //player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                        } else if ( holdDuration.count() <= _powerAttackHoldThreshold){
                            // Foi um clique r�pido -> ATAQUE NORMAL
                            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
                            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                        }else if ( holdDuration.count() >= _powerAttackHoldThreshold){
                            // Foi um clique r�pido -> ATAQUE NORMAL
                            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
                        }
                        

                    }else if (buttonEvent->IsHeld()) {
                        logger::info("quanto tempo ele pega isso?");
                            //player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);

                    }
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
                         // Este � o ataque para quando voc� est� PARADO e sem combo.
                         player->NotifyAnimationGraph("MCO_EndAnimation");
                         PerformAction(PowerRight, player);
                     }
                     return RE::BSEventNotifyControl::kContinue;
                 }
                 
                 else {
                     player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
                     player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
                     player->NotifyAnimationGraph("MCO_EndAnimation");
                     player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                     if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                         PlayIdleAnimation(player, idleToPlay);
                     }

                     isPlayerSprinting = false;
                     return RE::BSEventNotifyControl::kContinue;
                 }
             }

    }

    return RE::BSEventNotifyControl::kContinue;
}

void AttackStateManager::OpenComboWindow() {
    auto now = std::chrono::steady_clock::now();
    _attackState = AttackButtonState::kPowerAttackReleased_ComboWindowOpen;
    _comboWindowExpireTime = now + _comboWindowDuration;
    logger::info("Janela de Combo Aberta!");
}

RE::BSEventNotifyControl GlobalControl::AnimationEventHandler::ProcessEvent(
    const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    auto player = RE::PlayerCharacter::GetSingleton();

    if (a_event && a_event->holder && a_event->holder->IsPlayerRef()) {
        const std::string_view eventName = a_event->tag;
       
        if (eventName == "HitFrame") {
            SKSE::log::info("Evento 'HitFrame' recebido do jogador!");
            // Coloque aqui a sua l�gica para o HitFrame...

        } else if (eventName == "PowerAttack_Start_end") {
            player->SetGraphVariableBool("NEW_BFCO_IsInComboWindow", true);
            player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
            SKSE::log::info("Evento 'Bfco_AttackStartFX' recebido. Abrindo a janela de combo...");
            // Ex: GetGraphVariable("BFCO_IsInComboWindow")->SetBool(true);

        } else if (eventName == "MCO_PowerWinClose" || eventName == "MCO_WinClose") {
            player->SetGraphVariableBool("NEW_BFCO_IsInComboWindow", false);
            // player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            // player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
            SKSE::log::info("Evento 'BFCO_IsPlayerInputOK' recebido. Fechando a janela de combo...");

        } else if (eventName == "MCO_PowerWinOpen" || eventName == "MCO_WinOpen" ||
                   eventName == "BFCO_PlayerAttackStart") {
            player->SetGraphVariableBool("NEW_BFCO_IsInComboWindow", true);
           // player->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
            // player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            //player->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
            SKSE::log::info("Evento 'BFCO_IsPlayerInputOK' recebido. Fechando a janela de combo...");
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}
