#include "Hooks.h"
#include "Settings.h"

// Declaramos as vari�veis globais que definimos no main.cpp para que este arquivo saiba que elas existem.
extern const SKSE::TaskInterface* g_task;


const std::string pluginName = "SCSI-ACTbfco-Main.esp";

RE::BGSAction* PowerRight = nullptr;
RE::BGSAction* PowerStanding = nullptr;
RE::BGSAction* PowerLeft = nullptr;
RE::BGSAction* PowerDual = nullptr;
RE::BGSAction* NormalAttack = nullptr;

constexpr uint32_t MOUSE_BLOCK_BUTTON = 1;  // Bot�o direito do mouse antes do ajuste (+256)
constexpr uint32_t MOUSE_ATTACK_BUTTON = 0;
constexpr uint32_t GAMEPAD_BLOCK_BUTTON = 9;
constexpr uint32_t GAMEPAD_ATTACK_BUTTON = 10;  // kLeftHand, geralmente Gatilho Esquerdo

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

            //SKSE::log::info("Tocando anima��o idle FormID 0x{:X}", a_idle->GetFormID());
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

inline std::array blockedMenus = {
    RE::DialogueMenu::MENU_NAME,    RE::JournalMenu::MENU_NAME,    RE::MapMenu::MENU_NAME,
    RE::StatsMenu::MENU_NAME,       RE::ContainerMenu::MENU_NAME,  RE::InventoryMenu::MENU_NAME,
    RE::TweenMenu::MENU_NAME,       RE::TrainingMenu::MENU_NAME,   RE::TutorialMenu::MENU_NAME,
    RE::LockpickingMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME,  RE::LevelUpMenu::MENU_NAME,
    RE::Console::MENU_NAME,         RE::BookMenu::MENU_NAME,       RE::CreditsMenu::MENU_NAME,
    RE::LoadingMenu::MENU_NAME,     RE::MessageBoxMenu::MENU_NAME, RE::MainMenu::MENU_NAME,
    RE::RaceSexMenu::MENU_NAME,     RE::FavoritesMenu::MENU_NAME,  std::string_view("LootMenu"),  
    std::string_view("LootMenuIE") };

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
        if (!buttonEvent->IsDown() && !buttonEvent->IsUp() ) {
            continue;
        }

        if (IsAnyMenuOpen()) {
        logger::info("menu aberto");
            return RE::BSEventNotifyControl::kContinue;
        }

        auto device = buttonEvent->GetDevice();
        auto keyCode = buttonEvent->GetIDCode();
        // --- IN�CIO DA L�GICA REORDENADA ---
        const auto playerState = player->AsActorState();
        auto controlMap = RE::ControlMap::GetSingleton();
        auto sprintKey = controlMap->GetMappedKey("Sprint", device);
        auto attackKey = controlMap->GetMappedKey("Block", device);
        auto blockKey = controlMap->GetMappedKey("Attack", device);

        if (keyCode == sprintKey) {
            if (buttonEvent->IsDown()) {
                isPlayerSprinting = true;
                logger::info("O jogador come�ou a correr.");
            }
        }

        
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

        
        bool isBlockButtonPressed = (device == RE::INPUT_DEVICE::kMouse && keyCode == blockKey) ||
                                    (device == RE::INPUT_DEVICE::kGamepad && keyCode == blockKey ||
                                     device == device == RE::INPUT_DEVICE::kKeyboard && keyCode == blockKey);
        bool isAttackButtonPressed = (device == RE::INPUT_DEVICE::kMouse && keyCode == attackKey) ||
                                     (device == RE::INPUT_DEVICE::kGamepad && keyCode == attackKey ||
                                      device == device == RE::INPUT_DEVICE::kKeyboard && keyCode == attackKey);


        if (device == RE::INPUT_DEVICE::kMouse) {
            keyCode += 256;  // Ajusta o c�digo do mouse para corresponder �s configura��es
        }

        if (keyCode == Settings::BlockKey_g || keyCode == Settings::BlockKey_m || keyCode == Settings::BlockKey_k) {
            if (buttonEvent->IsDown()) {
                player->NotifyAnimationGraph("blockStart");
                player->SetGraphVariableBool("IsBlocking", true);
                return RE::BSEventNotifyControl::kStop;  // Consome o input
            } else if (buttonEvent->IsUp()) {
                player->SetGraphVariableBool("IsBlocking", false);
                player->NotifyAnimationGraph("blockStop");
                return RE::BSEventNotifyControl::kStop;  // Consome o input
            }
        }
        '
        // 1. VERIFICAR A��ES CUSTOMIZADAS PRIMEIRO (APENAS QUANDO O BOT�O � PRESSIONADO)
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

                if (Settings::lockSprintAttack && isPlayerSprinting) {
                    if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 0) {
                        isPlayerSprinting = false;             // Corrige o estado se a stamina acabar
                        return RE::BSEventNotifyControl::kContinue;  // N�o faz o ataque de sprint
                    }
                        switch (currentState) {
                            case WeaponState::DualWield:
                            case WeaponState::OneHanded:
                            case WeaponState::Invalid:
                                if (player->HasPerk(CriticalCharge)) {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    if (player->IsMoving()) {
                                        // Se o jogador estiver se movendo, faz um ataque normal.
                                        PerformAction(PowerRight, player);
                                    } else {
                                        // Se o jogador estiver parado, faz um ataque de poder.
                                        player->NotifyAnimationGraph("PowerAttack_Start_End");
                                        if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                            PlayIdleAnimation(player, teste);
                                        }
                                    }
                                } else {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    PerformAction(NormalAttack, player);
                                }
                                break;
                            case WeaponState::TwoHanded:
                                if (player->HasPerk(GreatCriticalCharge)) {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    if (player->IsMoving()) {
                                        // Se o jogador estiver se movendo, faz um ataque normal.
                                        PerformAction(PowerDual, player);
                                    } else {
                                        // Se o jogador estiver parado, faz um ataque de poder.
                                        player->NotifyAnimationGraph("PowerAttack_Start_End");
                                        if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                            PlayIdleAnimation(player, teste);
                                        }
                                    }
                                } else {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    PerformAction(NormalAttack, player);
                                }
                                break;
                            default:  // Unarmed ou outros casos
                                player->NotifyAnimationGraph("MCO_EndAnimation");
                                if (player->IsMoving()) {
                                    // Se o jogador estiver se movendo, faz um ataque normal.
                                    PerformAction(PowerRight, player);
                                } else {
                                    // Se o jogador estiver parado, faz um ataque de poder.
                                    player->NotifyAnimationGraph("PowerAttack_Start_End");
                                    if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                        PlayIdleAnimation(player, teste);
                                    }
                                }
                                break;
                        
                        
                        } 
                        isPlayerSprinting = false;
                        return RE::BSEventNotifyControl::kStop;  // A��o executada, consome o input.
                } else {
                    switch (currentState) {
                        case WeaponState::OneHanded:
                        case WeaponState::Invalid:
                        case WeaponState::TwoHanded:
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            if (player->IsMoving()) {
                                // Se o jogador estiver se movendo, faz um ataque normal.
                                PerformAction(PowerRight, player);
                            } else {
                                // Se o jogador estiver parado, faz um ataque de poder.
                                player->NotifyAnimationGraph("PowerAttack_Start_End");
                                if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                    PlayIdleAnimation(player, teste);
                                }
                            }
                            break;
                        case WeaponState::DualWield:
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            if (player->IsMoving()) {
                                // Se o jogador estiver se movendo, faz um ataque normal.
                                PerformAction(PowerDual, player);
                            } else {
                                // Se o jogador estiver parado, faz um ataque de poder.
                                player->NotifyAnimationGraph("PowerAttack_Start_End");
                                if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                    PlayIdleAnimation(player, teste);
                                }
                            }
                            break;
                        default:  // Unarmed ou outros casos
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            if (player->IsMoving()) {
                                // Se o jogador estiver se movendo, faz um ataque normal.
                                PerformAction(PowerRight, player);
                            } else {
                                // Se o jogador estiver parado, faz um ataque de poder.
                                player->NotifyAnimationGraph("PowerAttack_Start_End");
                                if (auto* teste = GetIdleByFormID(0x8C5, pluginName)) {
                                    PlayIdleAnimation(player, teste);
                                }
                            }
                            break;
                    }
                }
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

                    // --- NOVA L�GICA ---
                    // 1. PRIMEIRO, verificar se � um ataque de SPRINT TRAVADO.
                    if (Settings::lockSprintAttack && isPlayerSprinting) {
                        // Se for um ataque de sprint E TIVER combo, checa o perk.
                        if (hasCombo) {
                            switch (currentState) {
                                case WeaponState::DualWield:
                                case WeaponState::OneHanded:
                                case WeaponState::Invalid:
                                    if (player->HasPerk(CriticalCharge)) {
                                        player->NotifyAnimationGraph("MCO_EndAnimation");
                                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                        if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                            PlayIdleAnimation(player, idleToPlay);
                                        }
                                    } else {
                                        player->NotifyAnimationGraph("MCO_EndAnimation");
                                        PerformAction(NormalAttack, player);
                                    }
                                    break;
                                case WeaponState::TwoHanded:
                                    if (player->HasPerk(GreatCriticalCharge)) {
                                        player->NotifyAnimationGraph("MCO_EndAnimation");
                                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                        if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                            PlayIdleAnimation(player, idleToPlay);
                                        }
                                    } else {
                                        player->NotifyAnimationGraph("MCO_EndAnimation");
                                        PerformAction(NormalAttack, player);
                                    }
                                    break;
                                default:
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                        PlayIdleAnimation(player, idleToPlay);
                                    }
                                    break;
                            }
                        } else {
                            // <-- CORRE��O PRINCIPAL AQUI
                            // Se for um ataque de sprint mas N�O TIVER combo, faz um ataque normal.
                            player->NotifyAnimationGraph("MCO_EndAnimation");
                            PerformAction(NormalAttack, player);
                        }

                        isPlayerSprinting = false;
                        return RE::BSEventNotifyControl::kStop;

                        // 2. SE N�O FOR UM ATAQUE DE SPRINT, usa a l�gica de combo normal.
                    } else {
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
                        isPlayerSprinting = false;
                        return RE::BSEventNotifyControl::kStop;
                    }

                } else {
                    // A l�gica para "sem CMF" j� estava correta e n�o precisa de grandes mudan�as.
                    if (Settings::lockSprintAttack && isPlayerSprinting) {
                        switch (currentState) {
                            case WeaponState::DualWield:
                            case WeaponState::OneHanded:
                            case WeaponState::Invalid:
                                if (player->HasPerk(CriticalCharge)) {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                        PlayIdleAnimation(player, idleToPlay);
                                    }
                                } else {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    PerformAction(NormalAttack, player);
                                }
                                break;
                            case WeaponState::TwoHanded:
                                if (player->HasPerk(GreatCriticalCharge)) {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                    if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                        PlayIdleAnimation(player, idleToPlay);
                                    }
                                } else {
                                    player->NotifyAnimationGraph("MCO_EndAnimation");
                                    PerformAction(NormalAttack, player);
                                }
                                break;
                            default:
                                player->NotifyAnimationGraph("MCO_EndAnimation");
                                player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                                if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                                    PlayIdleAnimation(player, idleToPlay);
                                }
                                break;
                        }
                    } else {
                        player->NotifyAnimationGraph("MCO_EndAnimation");
                        player->NotifyAnimationGraph("BFCOAttackStart_Comb");
                        if (auto* idleToPlay = GetIdleByFormID(0x8BF, pluginName)) {
                            PlayIdleAnimation(player, idleToPlay);
                        }
                    }
                    isPlayerSprinting = false;
                    return RE::BSEventNotifyControl::kStop;
                }
            }
        }

        // 2. SE O BOT�O DE BLOQUEIO FOI ACIONADO (MOUSE OU CONTROLE)...
        if (isBlockButtonPressed && RE::PlayerCamera::GetSingleton()->IsInThirdPerson()) {
            // --- VERIFICA��O DE ARMA/MAGIA ---
            bool canBlock = true;
            auto rightHandItem = player->GetEquippedObject(false);  // false para m�o direita
            auto leftHandItem = player->GetEquippedObject(true);    // true para m�o esquerda

            // Checa a m�o direita
            if (rightHandItem) {
                // Se for uma magia...
                if (rightHandItem->Is(RE::FormType::Spell)) {
                    canBlock = false;
                }
                // Ou se for uma arma do tipo cajado...
                else if (auto weapon = rightHandItem->As<RE::TESObjectWEAP>(); weapon && weapon->IsStaff()) {
                    canBlock = false;
                }
            }

            // Checa a m�o esquerda (s� se a direita j� n�o tiver invalidado o bloqueio)
            if (canBlock && leftHandItem) {
                // Se for uma magia...
                if (leftHandItem->Is(RE::FormType::Spell)) {
                    canBlock = false;
                }
                // Ou se for uma arma do tipo cajado...
                else if (auto weapon = leftHandItem->As<RE::TESObjectWEAP>(); weapon && weapon->IsStaff()) {
                    canBlock = false;
                }
            }
            // --- FIM DA VERIFICA��O ---

            // Se, ap�s todas as checagens, o jogador PODE bloquear...
            if (canBlock) {
                if (buttonEvent->IsDown()) {
                    player->NotifyAnimationGraph("blockStart");
                    player->SetGraphVariableBool("IsBlocking", true);
                    return RE::BSEventNotifyControl::kStop;  // Consome o input
                } else if (buttonEvent->IsUp()) {
                    player->SetGraphVariableBool("IsBlocking", false);
                    player->NotifyAnimationGraph("blockStop");
                    return RE::BSEventNotifyControl::kStop;  // Consome o input
                }
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
