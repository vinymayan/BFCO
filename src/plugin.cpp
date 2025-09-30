#include <Xinput.h>
#include <stddef.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "ClibUtil/singleton.hpp"
#include "Manager.h"

using namespace RE;
using namespace RE::BSScript;
using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

// --- Ponteiros Globais da Engine ---
Settings g_settings;
const MessagingInterface* g_message = nullptr;
const TaskInterface* g_task = nullptr;
REL::Relocation<float*> ptr_engineTime{RELOCATION_ID(517597, 404125)};
PlayerCharacter* p;
RE::UI* mm;
ControlMap* im;


// --- Ações e Sons do Jogo ---
BGSAction* actionRightPowerAttack;
BGSAction* actionRightAttack;


// --- Variáveis de Estado ---
TESEffectShader* notifyFX = nullptr;
BGSAction* queuePAAction = nullptr;
bool isAttacking = false;
bool g_isPowerComboWindowOpen = false;
bool attackWindow = false;
float paQueueTime = 0.f;
bool g_isComboWindowOpen = false;
float g_lmbDownTime = 0.0f;
float g_rmbDownTime = 0.0f;
const float HOLD_DURATION_FOR_POWER_ATTACK = 0.4f;

// --- Nomes de Eventos de Animação (Hardcoded) ---
const std::string MCOWinOpen = "MCO_WinOpen";
const std::string MCOWinClose = "MCO_WinClose";
const std::string MCOPowerWinOpen = "MCO_PowerWinOpen";
const std::string MCOPowerWinClose = "MCO_PowerWinClose";
const std::string BFCO_NextWinStart = "BFCO_NextWinStart";
const std::string BFCO_NextPowerWinStart = "BFCO_NextPowerWinStart";
const std::string BFCO_DIY_EndLoop = "BFCO_DIY_EndLoop";
const BSFixedString Bfco_AttackStartFX("Bfco_AttackStartFX");  // Evento que abre a janela de combo
const BSFixedString HitFrame("HitFrame"); 

constexpr std::string_view BFCO_Powerattack_ID = "bfcoTG_DirPowerAttack";
constexpr std::string_view BFCO_LmbPowerAttack_ID = "bfcoTG_LmbPowerAttackNUM";  // ID da imagem
constexpr std::string_view BFCO_RmbPowerAttack_ID = "bfcoTG_RmbPowerAttackNUM";  // ID da imagem
constexpr std::string_view BFCO_KeyAttackComb_ID = "bfcoTG_KeyAttackComb";       // ID da imagem
constexpr std::string_view BFCO_DisJumpAttack_ID = "bfcoDebug_DisJumpAttack";



// Função para obter um Form (efeito, som, etc.) de um plugin .esp/.esl
TESForm* GetFormFromMod(const std::string& modname, uint32_t formid) {
    if (modname.empty() || !formid) return nullptr;

    TESDataHandler* dh = TESDataHandler::GetSingleton();
    TESFile* modFile = nullptr;
    for (auto const& file : dh->files) {
        if (file && strcmp(file->fileName, modname.c_str()) == 0) {
            modFile = file;
            break;
        }
    }
    if (!modFile) return nullptr;

    uint32_t finalFormId = modFile->GetPartialIndex() << 24 | formid;
    if (modFile->IsLight()) {
        finalFormId = 0xFE000000 | modFile->GetSmallFileCompileIndex() << 12 | formid;
    }

    return TESForm::LookupByID(finalFormId);
}

bool IsRidingHorse(Actor* a) { return (a->AsActorState()->actorState1.sitSleepState == SIT_SLEEP_STATE::kRidingMount); }
bool IsInKillmove(Actor* a) { return a->GetActorRuntimeData().boolFlags.all(Actor::BOOL_FLAGS::kIsInKillMove); }
bool IsPAQueued() { return *ptr_engineTime - paQueueTime <= g_settings.queuePAExpire; }

// --- Função Principal de Ação (Envia comando para a Engine) ---
void PerformAction(BGSAction* action, Actor* a) {
    log::info("Dentro da tarefa agendada. Verificando ponteiros:");
    log::info("Ponteiro do Ator (a): {:#x}", (uintptr_t)a);
    log::info("Ponteiro da Ação (action): {:#x}", (uintptr_t)action);
    if (!a || !action) {
        log::error("CRASH PREVENIDO: Um dos ponteiros (Ator ou Ação) está nulo dentro da tarefa. Abortando.");
        return;
    }
    g_task->AddTask([action, a]() {
        std::unique_ptr<TESActionData> data(TESActionData::Create());
        if (data) {
            data->source = NiPointer<TESObjectREFR>(a);
            data->action = action;

            using func_t = bool(TESActionData*);
            REL::Relocation<func_t> func{RELOCATION_ID(40551, 41557)};
            bool success = func(data.get());

            if (!success && g_settings.queuePA) {
                paQueueTime = *ptr_engineTime;
                queuePAAction = action;
            } else if (success && IsPAQueued() && action == queuePAAction) {
                paQueueTime = 0.f;
            }
        }
    });
}

void PowerAttack() {
    log::info("Função PowerAttack() chamada.");

    // LOG PRÉ-CRASH: Verificando os ponteiros momentos antes de usá-los.
    log::info("Verificando ponteiros antes de chamar PerformAction:");
    log::info("Ponteiro global do Jogador (p): {:#x}", (uintptr_t)p);
    log::info("Ponteiro global da Ação (actionRightPowerAttack): {:#x}", (uintptr_t)actionRightPowerAttack);

    if (!p || !actionRightPowerAttack) {
        log::error("CRASH PREVENIDO: 'p' ou 'actionRightPowerAttack' é nulo. Abortando PowerAttack.");
        return;
    }

    // [NOVO] Adicionada verificação para ataques pulando
    if (g_settings.disableJumpingAttack && p->IsInMidair()) {
        log::info("Power Attack bloqueado: disableJumpingAttack=true e o jogador está pulando.");
        return;
    }

    if (g_settings.onlyDuringAttack && !isAttacking) {
        log::info("PowerAttack bloqueado: onlyDuringAttack=true e o jogador não está atacando.");
        return;
    }

    

    PerformAction(actionRightPowerAttack, p);
}


// Hook para eventos de animação
class HookAnimGraphEvent {
public:
    typedef BSEventNotifyControl (HookAnimGraphEvent::*FnReceiveEvent)(BSAnimationGraphEvent*,
                                                                       BSTEventSource<BSAnimationGraphEvent>*);

    BSEventNotifyControl ReceiveEventHook(BSAnimationGraphEvent* evn, BSTEventSource<BSAnimationGraphEvent>* src) {
        if (evn && evn->holder) {
            auto a = evn->holder->As<Actor>();
            if (a && a->IsPlayerRef()) {
                // Sincroniza 'isAttacking' com o estado real do motor do jogo.
                ATTACK_STATE_ENUM currentState = (a->AsActorState()->actorState1.meleeAttackState);
                isAttacking = (currentState >= ATTACK_STATE_ENUM::kDraw && currentState <= ATTACK_STATE_ENUM::kBash);

                const char* eventTag = evn->tag.c_str();

                // --- LÓGICA DO "SENSOR" ATIVADA PELOS EVENTOS MCO ---
                if (strcmp(eventTag, MCOPowerWinOpen.c_str()) == 0) {
                    log::info("Evento 'MCO_PowerWinOpen' detectado. Sensor de combo de Power Attack LIGADO.");
                    g_isPowerComboWindowOpen = true;
                } else if (strcmp(eventTag, MCOPowerWinClose.c_str()) == 0 ||
                           strcmp(eventTag, MCOWinClose.c_str()) == 0) {
                    if (g_isPowerComboWindowOpen) {
                        log::info("Evento de fechamento detectado. Sensor de combo de Power Attack DESLIGADO.");
                        g_isPowerComboWindowOpen = false;
                    }
                }
            }
        }
        FnReceiveEvent fn = fnHash.at(*(uintptr_t*)this);
        return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
    }

    static void Hook() {
        REL::Relocation<uintptr_t> vtable{VTABLE_PlayerCharacter[2]};
        FnReceiveEvent fn =
            stl::unrestricted_cast<FnReceiveEvent>(vtable.write_vfunc(1, &HookAnimGraphEvent::ReceiveEventHook));
        fnHash.insert(std::pair<uintptr_t, FnReceiveEvent>(vtable.address(), fn));
        logger::info("HookAnimGraphEvent (com sensor de Power Combo) instalado.");
    }

private:
    static std::unordered_map<uintptr_t, FnReceiveEvent> fnHash;
};
std::unordered_map<uintptr_t, HookAnimGraphEvent::FnReceiveEvent> HookAnimGraphEvent::fnHash;

// --- Manipulador de Input (Onde a tecla 'V' é detectada) ---
class InputEventHandler : public BSTEventSink<InputEvent*> {
public:
    virtual BSEventNotifyControl ProcessEvent(InputEvent* const* evns,
                                              BSTEventSource<InputEvent*>* dispatcher) override {
        if (!evns || !p || !mm || !im) return BSEventNotifyControl::kContinue;

        if (IsRidingHorse(p) || IsInKillmove(p) || mm->numPausesGame > 0 || !mm->menuSystemVisible ||
            p->AsActorState()->actorState1.sitSleepState != SIT_SLEEP_STATE::kNormal)
            return BSEventNotifyControl::kContinue;

        if (g_settings.disableJumpingAttack && p->IsInMidair()) {
            return BSEventNotifyControl::kContinue;
        }


        if (!g_settings.allowZeroStamina && p->AsActorValueOwner()->GetActorValue(ActorValue::kStamina) <= 0)
            return BSEventNotifyControl::kContinue;

         

        for (InputEvent* e = *evns; e; e = e->next) {
            if (e->eventType != INPUT_EVENT_TYPE::kButton) continue;
            ButtonEvent* btn = e->AsButtonEvent();
            if (!btn || !btn->IsDown()) continue;

            auto device = btn->device.get();
            uint32_t id = btn->GetIDCode();

            // Lógica da Tecla de Atalho (usando as configurações)
            bool powerAttackPressed = (device == INPUT_DEVICE::kKeyboard && id == g_settings.powerAttackKey_k &&
                                       g_settings.powerAttackKey_k != 0) ||
                                      (device == INPUT_DEVICE::kGamepad && id == g_settings.powerAttackKey_g &&
                                       g_settings.powerAttackKey_g != 0);

            if (powerAttackPressed) {
                // --- AQUI ESTÁ O "SWITCH" INTELIGENTE ---
                if (g_isPowerComboWindowOpen) {
                    // Se o sensor está LIGADO, significa que estamos no meio de um combo.
                    // O gráfico de animação já sabe o que fazer, só precisa do gatilho.
                    log::info("Sensor LIGADO. Continuando combo de Power Attack (PA2, PA3...).");
                    PerformAction(actionRightPowerAttack, p);

                    // "Consumimos" a janela de combo para evitar ataques múltiplos com um só clique.
                    g_isPowerComboWindowOpen = false;
                } else {
                    // Se o sensor está DESLIGADO, este é o primeiro ataque.
                    log::info("Sensor DESLIGADO. Iniciando novo combo de Power Attack (PA1).");
                    PowerAttack();  // Usamos a função completa para as checagens de segurança.
                }
                return BSEventNotifyControl::kStop;  // Consome o input em ambos os casos.
            }

            // A lógica de "Segurar para atacar" e de "Desativar bloqueio" pode ser adicionada aqui se necessário.
        }
        return BSEventNotifyControl::kContinue;
    }
    TES_HEAP_REDEFINE_NEW();
};

// --- Carga do Plugin (Ponto de entrada do SKSE) ---
SKSEPluginLoad(const LoadInterface* skse) {
    auto* plugin = PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    log::info("{} {} is loading...", plugin->GetName(), version);

    Init(skse);
    g_task = GetTaskInterface();
    g_message = GetMessagingInterface();
    g_message->RegisterListener([](MessagingInterface::Message* msg) -> void {
        if (msg->type == MessagingInterface::kDataLoaded) {
            DirPowerAttack = RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_Powerattack_ID);
            Global_LmbPowerAttack = RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_LmbPowerAttack_ID);
            Global_RmbPowerAttack = RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_RmbPowerAttack_ID);
            Global_KeyAttackComb = RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_KeyAttackComb_ID);
            Global_DisJumpAttack = RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_DisJumpAttack_ID);

            logger::info("Variaveis globais carregadas.");
            p = PlayerCharacter::GetSingleton();
            mm = RE::UI::GetSingleton();
            im = ControlMap::GetSingleton();

            // Pega as ações do jogo
            actionRightAttack = TESForm::LookupByID<BGSAction>(0x13005);
            actionRightPowerAttack = TESForm::LookupByID<BGSAction>(0x13383);

            if (!p || !actionRightPowerAttack) {
                log::critical(
                    "FALHA CRÍTICA: Não foi possível obter o ponteiro do jogador ou da ação de power attack. O mod não "
                    "funcionará.");
                return;
            }
            SettingsMenu::Load();
            SettingsMenu::Register();

            // Registra os hooks e handlers
            auto* inputMgr = BSInputDeviceManager::GetSingleton();
            if (inputMgr) {
                inputMgr->AddEventSink(new InputEventHandler());
            }
            HookAnimGraphEvent::Hook();

            log::info("Plugin carregado e pronto.");
        }
    });

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}