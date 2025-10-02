#include "Hooks.h"
#include "Settings.h"

// --- NOVAS VARIÁVEIS GLOBAIS ---
// Ponteiro para a interface de tarefas do SKSE
const SKSE::TaskInterface* g_task = nullptr;
// Ponteiro para a ação de ataque de poder que vamos usar


// ------------------------------------

// MUDANÇA: A função foi renomeada para refletir que lida com várias mensagens.
void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            AttackStateManager::GetSingleton()->Register();
            SKSE::log::info("Evento kDataLoaded recebido. Procurando BGSAction...");
            PowerRight = RE::TESForm::LookupByID<RE::BGSAction>(0x13383);
            PowerLeft = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F6);
            PowerDual = RE::TESForm::LookupByID<RE::BGSAction>(0x2E2F7);
            BFCOMenu::Register();
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame: {
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
        SKSE::log::critical("Não foi possível obter a Task Interface do SKSE.");
    }

    auto message = SKSE::GetMessagingInterface();
    if (message) {
        // MUDANÇA: Precisamos escutar pelo evento kDataLoaded também.
        message->RegisterListener("SKSE", OnSKSEMessage);
        SKSE::log::info("Listener de mensagens SKSE registrado com sucesso.");
    } else {
        SKSE::log::critical("Não foi possível obter a interface de mensagens do SKSE.");
    }

    return true;
}
