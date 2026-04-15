#include "logger.h"
#include "Events.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        BFCO::InstallHooks();
        BFCOIdles::InitIdles();
        MenuWatcher::GetSingleton()->Register();
        AttackStateManager::GetSingleton()->Register();
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(BFCO::Hooks::PC3DLoadEventHandler::GetSingleton());
        if (GetModuleHandleA("SCAR.dll")) {
            BFCO::scar = true;
            logger::info("SCAR.dll founded");
        }
        else {
            BFCO::scar = false;
            logger::info("SCAR.dll not found.");
        }

    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(BFCO::Hooks::NpcCombatTracker::GetSingleton());
        BFCO::Hooks::NpcCombatTracker::RegisterSinksForExistingCombatants();
        auto player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(BFCO::Hooks::NpcCycleSink::GetSingleton());
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Settings::Load();
            }).detach();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
