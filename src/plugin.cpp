#include "Hooks.h"
#include "Events.h"
// --- NOVAS VARI�VEIS GLOBAIS ---
// Ponteiro para a interface de tarefas do SKSE
const SKSE::TaskInterface* g_task = nullptr;
// Ponteiro para a a��o de ataque de poder que vamos usar


// ------------------------------------

// MUDAN�A: A fun��o foi renomeada para refletir que lida com v�rias mensagens.
void OnMessage(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            IsCycleMovesetActive();
            AttackStateManager::GetSingleton()->Register();
            SKSE::log::info("Evento kDataLoaded recebido. Procurando BGSAction...");
            PowerRight = RE::TESForm::LookupByID<RE::BGSAction>(0x13383);
            PowerStanding = RE::TESForm::LookupByID<RE::BGSAction>(0x19B26);
            PowerLeft = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F6);
            PowerDual = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F7);
            NormalAttack = RE::TESForm::LookupByID<RE::BGSAction>(0x13005);
            PowerAttack = RE::TESForm::LookupByID<RE::BGSAction>(0xE8456);
            Bash = RE::TESForm::LookupByID<RE::BGSAction>(0x1B417);
            BFCOMenu::LoadSettings();
            BFCOMenu::Register();
            SKSE::log::info("Tentando registrar AnimationEventHandler...");
            auto* animationEventSource = RE::PlayerCharacter::GetSingleton();
            if (animationEventSource) {
                animationEventSource->AddAnimationGraphEventSink(AnimationEventHandler::GetSingleton());
                SKSE::log::info("AnimationEventHandler registrado com sucesso no PlayerCharacter.");
            } else {
                SKSE::log::error(
                    "Falha ao registrar AnimationEventHandler: PlayerCharacter n�o encontrado durante kDataLoaded.");
            }
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame:
            BFCOMenu::UpdateGameGlobals();
            GetAttackKeys();

            break;
        case SKSE::MessagingInterface::kNewGame: {
            BFCOMenu::UpdateGameGlobals();
            GetAttackKeys();
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SKSE::log::init();
    SKSE::log::info("BFCO SKSE Plugin carregado.");
    g_task = SKSE::GetTaskInterface();
    if (!g_task) {
        SKSE::log::critical("N�o foi poss�vel obter a Task Interface do SKSE.");
    }
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
