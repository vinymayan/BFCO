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

constexpr std::string_view ESP_NAME = "SCSI-ACTbfco-Main.esp";
constexpr std::string_view BFCO_Powerattack = "bfcoTG_DirPowerAttack";
constexpr std::string_view BFCO_jumpattack = "bfcoDebug_DisJumpAttack";
inline RE::TESGlobal* DirPowerAttack = nullptr;
inline RE::TESGlobal* DisableJumpAttack = nullptr;  

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
    BSEventNotifyControl ReceiveEventHook(BSAnimationGraphEvent* evn, BSTEventSource<BSAnimationGraphEvent>* src) {
        if (evn && evn->holder) {
            DirPowerAttack->value = 0.0f;
            const Actor* a = evn->holder->As<Actor>();
            if (a && a->IsPlayerRef()) {
                ATTACK_STATE_ENUM currentState = (a->AsActorState()->actorState1.meleeAttackState);
                isAttacking = (currentState >= ATTACK_STATE_ENUM::kDraw && currentState <= ATTACK_STATE_ENUM::kBash);
                const char* eventTag = evn->tag.c_str();
                if (strcmp(eventTag, Bfco_AttackStartFX.c_str()) == 0) {  // A lógica do Bfco_AttackStartFX permanece
                    log::info("Evento 'Bfco_AttackStartFX' detectado. Janela de combo ABERTA.");
                    g_isComboWindowOpen = true;
                } else if (strcmp(eventTag, MCOWinOpen.c_str()) == 0 ||
                           strcmp(eventTag, BFCO_NextWinStart.c_str()) == 0) {
                    attackWindow = true;
                    if (g_settings.notifyWindow && notifyFX) {
                        using InstantiateHitShader_t = RE::ShaderReferenceEffect*(
                            RE::TESObjectREFR*, RE::TESEffectShader*, float, bool, bool, RE::NiAVObject*);
                        REL::Relocation<InstantiateHitShader_t> func{RELOCATION_ID(33271, 34029)};
                        func(p, notifyFX, g_settings.notifyDuration, false, false, nullptr);
                    }
                }
                // [ALTERADO] Verifica tanto pelo evento do MCO quanto do BFCO para FECHAR a janela
                else if (strcmp(eventTag, MCOWinClose.c_str()) == 0 ||
                         strcmp(eventTag, BFCO_DIY_EndLoop.c_str()) == 0) {
                    attackWindow = false;
                    if (g_isComboWindowOpen) {
                        log::info("Janela de ataque fechada. Janela de combo FECHADA.");
                        g_isComboWindowOpen = false;
                    }
                }
                // [ALTERADO] Lógica para o ataque poderoso enfileirado
                else if (IsPAQueued()) {
                    // Verifica se o evento é uma janela de abertura OU fechamento de ataque poderoso de QUALQUER um dos
                    // mods
                    bool isPowerWinOpen = (strcmp(eventTag, MCOPowerWinOpen.c_str()) == 0 ||
                                           strcmp(eventTag, BFCO_NextPowerWinStart.c_str()) == 0);
                    bool isPowerWinClose =
                        (strcmp(eventTag, MCOPowerWinClose.c_str()) == 0 ||
                         strcmp(eventTag, BFCO_DIY_EndLoop.c_str()) == 0);  // BFCO usa o mesmo evento para fechar

                    if (isPowerWinOpen || isPowerWinClose) {
                        PerformAction(queuePAAction, p);
                    }
                }
            }
        }

        REL::Relocation<uintptr_t> vtable{VTABLE_PlayerCharacter[2]};
        return (this->*fnHash.at(vtable.address()))(evn, src);
    }
    static void Hook() {
        REL::Relocation<uintptr_t> vtable{VTABLE_PlayerCharacter[2]};
        auto original_function = vtable.write_vfunc(1, &HookAnimGraphEvent::ReceiveEventHook);
        fnHash[vtable.address()] =
            stl::unrestricted_cast<decltype(&HookAnimGraphEvent::ReceiveEventHook)>(original_function);
    }

private:
    static std::unordered_map<uintptr_t, decltype(&HookAnimGraphEvent::ReceiveEventHook)> fnHash;
};
std::unordered_map<uintptr_t, decltype(&HookAnimGraphEvent::ReceiveEventHook)> HookAnimGraphEvent::fnHash;

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
            if (e->eventType == INPUT_EVENT_TYPE::kButton) {
                ButtonEvent* btn = e->AsButtonEvent();
                if (!btn) continue;

                // --- [NOVO] Lógica para Ataque Poderoso segurando o mouse ---
                // Verifica se um dos modos de ataque poderoso pelo mouse está ativo.
                if (g_settings.lightAttackToPowerAttack || g_settings.blockToPowerAttack) {
                    // Botão Esquerdo (Ataque Leve)
                    if (btn->device.get() == INPUT_DEVICE::kMouse && btn->GetIDCode() == 0 &&
                        g_settings.lightAttackToPowerAttack) {
                        if (btn->IsDown()) {
                            g_lmbDownTime = *ptr_engineTime;  // Armazena o tempo que o botão foi pressionado
                        } else if (btn->IsUp() && g_lmbDownTime > 0) {
                            // Se o botão foi solto antes do tempo de "hold", realiza um ataque normal.
                            if (*ptr_engineTime - g_lmbDownTime < HOLD_DURATION_FOR_POWER_ATTACK) {
                                PerformAction(actionRightAttack, p);
                            }
                            g_lmbDownTime = 0.0f;  // Reseta o tempo
                        }
                    }
                    // Botão Direito (Bloqueio)
                    if (btn->device.get() == INPUT_DEVICE::kMouse && btn->GetIDCode() == 1 &&
                        g_settings.blockToPowerAttack) {
                        if (btn->IsDown()) {
                            g_rmbDownTime = *ptr_engineTime;
                        } else if (btn->IsUp()) {
                            // A lógica de soltar o botão direito é complexa (pode ser bloqueio),
                            // então o ataque poderoso só é ativado no "Update" abaixo.
                            g_rmbDownTime = 0.0f;
                        }
                    }
                }
                // Checagem contínua para ver se o tempo de "hold" foi atingido
                if (g_lmbDownTime > 0 && (*ptr_engineTime - g_lmbDownTime >= HOLD_DURATION_FOR_POWER_ATTACK)) {
                    log::info("Ataque poderoso ativado pelo botão esquerdo do mouse (hold).");
                    PowerAttack();
                    g_lmbDownTime = 0.0f;                // Reseta para não disparar de novo
                    return BSEventNotifyControl::kStop;  // Consome o input
                }
                if (g_rmbDownTime > 0 && (*ptr_engineTime - g_rmbDownTime >= HOLD_DURATION_FOR_POWER_ATTACK)) {
                    log::info("Ataque poderoso ativado pelo botão direito do mouse (hold).");
                    PowerAttack();
                    g_rmbDownTime = 0.0f;
                    return BSEventNotifyControl::kStop;
                }

                // --- Lógica Original + Checagem das Configurações ---
                if (btn->IsDown()) {
                    // PRIORIDADE 1: Power Attack com a tecla 'V'
                    if (btn->device.get() == INPUT_DEVICE::kKeyboard && btn->GetIDCode() == 47) {  // Tecla V
                        log::info("--- Tecla V Pressionada ---");
                        PowerAttack();
                        return BSEventNotifyControl::kStop;
                    }
                    // PRIORIDADE 2: Lógica de Combo com o botão direito
                    // [ALTERADO] Adicionada verificação da configuração 'enableComboKey'
                    else if (g_settings.enableComboKey && btn->device.get() == INPUT_DEVICE::kMouse &&
                             btn->GetIDCode() == 1 && g_isComboWindowOpen) {
                        log::info("Input de combo bem-sucedido! (Clique Direito)");

                        p->SetGraphVariableBool("BFCO_ComboSuccess", true);
                        g_isComboWindowOpen = false;

                        return BSEventNotifyControl::kStop;
                    }
                }
            }
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
            DirPowerAttack =
                RE::TESForm::LookupByEditorID<RE::TESGlobal>(BFCO_Powerattack);
            if (!DirPowerAttack) {
                SKSE::log::critical("Nao foi possivel encontrar a Variavel Global com EditorID: {}",
                                    BFCO_Powerattack);
                return;
            }
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