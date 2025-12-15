#include "Hooks.h"
#include "Events.h"
// --- NOVAS VARIÁVEIS GLOBAIS ---
// Ponteiro para a interface de tarefas do SKSE
const SKSE::TaskInterface* g_task = nullptr;
// Ponteiro para a ação de ataque de poder que vamos usar


// ------------------------------------

// MUDANÇA: A função foi renomeada para refletir que lida com várias mensagens.
void OnMessage(SKSE::MessagingInterface::Message* msg) {
       if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            IsCycleMovesetActive();
            AttackStateManager::GetSingleton()->Register();
            SKSE::log::info("Evento kDataLoaded recebido. Procurando BGSAction...");
            PowerRight = RE::TESForm::LookupByID<RE::BGSAction>(0x13383);
            Draw = RE::TESForm::LookupByID<RE::BGSAction>(0x132AF);
            Seath = RE::TESForm::LookupByID<RE::BGSAction>(0x46BAF);
            PowerStanding = RE::TESForm::LookupByID<RE::BGSAction>(0x19B26);
            PowerLeft = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F6);
            PowerDual = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F7);
            NormalAttack = RE::TESForm::LookupByID<RE::BGSAction>(0x13005);
            RightAttack = RE::TESForm::LookupByID<RE::BGSAction>(0x13004);
            PowerAttack = RE::TESForm::LookupByID<RE::BGSAction>(0xE8456);
            Bash = RE::TESForm::LookupByID<RE::BGSAction>(0x1B417);
            BFCOMenu::LoadSettings();
            BFCOMenu::Register();
       }
    if (msg->type == SKSE::MessagingInterface::kNewGame || msg->type == SKSE::MessagingInterface::kPostLoadGame) {
        BFCOMenu::UpdateGameGlobals();
        GetAttackKeys();
        auto* player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(GlobalControl::AnimationEventHandler::GetSingleton());
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(GlobalControl::PC3DLoadEventHandler::GetSingleton());
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SKSE::log::init();
    SKSE::log::info("BFCO SKSE Plugin carregado.");
    g_task = SKSE::GetTaskInterface();
    if (!g_task) {
        SKSE::log::critical("Não foi possível obter a Task Interface do SKSE.");
    }
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
