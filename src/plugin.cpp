#include "logger.h"
#include "Events.h"
#include"Settings.h"
#include "Manager.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == InputManagerAPI::kMessage_ProvideAPI) {
        InputManagerAPI::ReceiveAPI(message);
        logger::info("API do Input Manager recebida com sucesso via SKSE Message!");
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        AttackStateManager::GetSingleton()->Register();
        //BFCO::InstallHooks();
        BFCOIdles::InitIdles();
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(BFCO::Hooks::PC3DLoadEventHandler::GetSingleton());
        if (GetModuleHandleA("SCAR.dll")) {
            BFCO::scar = true;
            logger::info("SCAR.dll founded");
        }
        else {
            BFCO::scar = false;
            logger::info("SCAR.dll not found.");
        }
        if (GetModuleHandleA("CycleMovesets.dll")) {
            BFCO::CMF = true;
            logger::info("CycleMovesets.dll founded");
        }
        else {
            BFCO::CMF = false;
            logger::info("CycleMovesets.dll not found.");
        }
        Manager::GetSingleton()->PopulateAllLists();
    }
    if(message->type == SKSE::MessagingInterface::kPostLoad) {
        InputManagerAPI::RequestAPIDirect();

        if (InputManagerAPI::_API) {
            logger::info("API do Input Manager conectada via Windows DLL Export!");
        }
        else {
            InputManagerAPI::RequestAPI();
        }
        BFCOMenu::Register();
        if (InputManagerAPI::_API) {
            BFCOMenu::RegisterAllInputs();
            BFCOMenu::TweenPauseRegister();
        }
	}
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(BFCO::Hooks::NpcCombatTracker::GetSingleton());
        BFCO::Hooks::NpcCombatTracker::RegisterSinksForExistingCombatants();
        auto player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(BFCO::Hooks::NpcCycleSink::GetSingleton());
        BFCOMenu::UpdateGameGlobals();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
