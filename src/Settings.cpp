#include "Settings.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"

namespace BFCOMenu {

    // Caminho para o nosso arquivo de configurações
    const char* SETTINGS_PATH = "Data/SKSE/Plugins/BFCO_Settings.json";

    // Função auxiliar para criar um seletor de teclas (ComboBox)
    // Baseado no seu exemplo Events.cpp
    void RenderKeybind(const char* label,
        uint32_t* key_k, uint32_t* key_k_mod,
        uint32_t* key_m, uint32_t* key_m_mod,
        uint32_t* key_g, uint32_t* key_g_mod,
        bool mouse_only = false) {

        bool settings_changed = false;

        ImGui::Text("%s (PC)", label);
        ImGui::SameLine();

        // --- PC (Keyboard/Mouse) ---
        uint32_t current_pc_key = (*key_k != 0) ? *key_k : *key_m;
        uint32_t current_pc_mod = (*key_k != 0) ? *key_k_mod : *key_m_mod; // Pega o mod correspondente

        const char* current_key_name_pc = "[Nenhuma]";
        if (auto it = g_dx_to_name_map.find(current_pc_key); it != g_dx_to_name_map.end()) current_key_name_pc = it->second;

        // Combo Principal
        ImGui::PushID(label); // Evita colisão de IDs no ImGui
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##Key", current_key_name_pc)) {
            for (auto const& [key_code, key_name] : g_dx_to_name_map) {
                if (mouse_only && key_code < 256) continue;
                if (ImGui::Selectable(key_name, current_pc_key == key_code)) {
                    if (key_code < 256) {
                        *key_k = key_code; *key_m = 0;
                        // Reseta o mod do mouse se mudou para teclado
                        *key_m_mod = 0;
                    }
                    else {
                        *key_m = key_code; *key_k = 0;
                        // Reseta o mod do teclado se mudou para mouse
                        *key_k_mod = 0;
                    }
                    settings_changed = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::Text("+");
        ImGui::SameLine();

        // Combo Modificador
        const char* current_mod_name_pc = "[Nenhuma]";
        if (auto it = g_dx_to_name_map.find(current_pc_mod); it != g_dx_to_name_map.end()) current_mod_name_pc = it->second;

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##Mod", current_mod_name_pc)) {
            for (auto const& [key_code, key_name] : g_dx_to_name_map) {
                // Modificador pode ser qualquer tecla
                if (ImGui::Selectable(key_name, current_pc_mod == key_code)) {
                    if (*key_k != 0) *key_k_mod = key_code;
                    else if (*key_m != 0) *key_m_mod = key_code;
                    settings_changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();

        // --- GAMEPAD ---
        const char* current_key_name_g = "[Nenhuma]";
        if (auto it = g_gamepad_dx_to_name_map.find(*key_g); it != g_gamepad_dx_to_name_map.end()) current_key_name_g = it->second;

        const char* current_mod_name_g = "[Nenhuma]";
        if (auto it = g_gamepad_dx_to_name_map.find(*key_g_mod); it != g_gamepad_dx_to_name_map.end()) current_mod_name_g = it->second;

        ImGui::Text("%s (Gamepad)", label);
        ImGui::SameLine();

        ImGui::PushID((std::string(label) + "_g").c_str());
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##KeyG", current_key_name_g)) {
            for (auto const& [key_code, key_name] : g_gamepad_dx_to_name_map) {
                if (ImGui::Selectable(key_name, *key_g == key_code)) {
                    *key_g = key_code;
                    settings_changed = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::Text("+");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##ModG", current_mod_name_g)) {
            for (auto const& [key_code, key_name] : g_gamepad_dx_to_name_map) {
                if (ImGui::Selectable(key_name, *key_g_mod == key_code)) {
                    *key_g_mod = key_code;
                    settings_changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();

        if (settings_changed) {
            SaveSettings();
        }
    }

    // Função principal que desenha o conteúdo do menu (sem alterações necessárias aqui)
    void __stdcall Render() {
        bool settings_changed = false;

        ImGui::Text("Combat settings BFCO");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Checkbox("Enable combo attack", &Settings::bEnableComboAttack)) settings_changed = true;

        RenderKeybind("Combo Attack",
            &Settings::comboKey_k, &Settings::comboKey_k_mod,
            &Settings::comboKey_m, &Settings::comboKey_m_mod,
            &Settings::comboKey_g, &Settings::comboKey_g_mod);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Checkbox("Enable direcional power attack", &Settings::bEnableDirectionalAttack)) settings_changed = true;
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Recommend disable, only enable if you use native BFCO animations");
        }
        if (ImGui::Checkbox("Enable power attack key", &Settings::bEnablePowerAttack)) settings_changed = true;

        RenderKeybind("Power Attack",
            &Settings::PowerAttackKey_k, &Settings::PowerAttackKey_k_mod,
            &Settings::PowerAttackKey_m, &Settings::PowerAttackKey_m_mod,
            &Settings::PowerAttackKey_g, &Settings::PowerAttackKey_g_mod);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RenderKeybind("Block Key (Beta)",
            &Settings::BlockKey_k, &Settings::BlockKey_k_mod,
            &Settings::BlockKey_m, &Settings::BlockKey_m_mod,
            &Settings::BlockKey_g, &Settings::BlockKey_g_mod);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Light Attack"); // Nova Categoria
        ImGui::Spacing();

        // Dropdown para bPowerAttackLMB
        const char* items[] = { "Vanilla", "Disable PA", "Auto Mode" };
        const char* current_item = (Settings::bPowerAttackLMB >= 0 && Settings::bPowerAttackLMB < 3) ? items[Settings::bPowerAttackLMB] : items[0];

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("Light Attack Mode", current_item)) {
            for (int n = 0; n < sizeof(items) / sizeof(items[0]); n++) {
                bool is_selected = (Settings::bPowerAttackLMB == n);
                if (ImGui::Selectable(items[n], is_selected)) {
                    Settings::bPowerAttackLMB = n;
                    settings_changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Vanilla: Default behavior\nDisable PA: Disables Power Attack on click\nAuto Mode: Holds attack for combos automatically");
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Checkbox("Disable bash in block key (Beta)", &Settings::disableMStaBash)) settings_changed = true;
        if (ImGui::Checkbox("Disable block non weapon in left in block key", &Settings::disableDualblock)) settings_changed = true;
        if (ImGui::Checkbox("Disable jump attack (Beta)", &Settings::bDisableJumpingAttack)) settings_changed = true;

        if (settings_changed) {
            SaveSettings();
        }
    }

    // Função que aplica as configurações às Globals do jogo
    void UpdateGameGlobals() {
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("Falha ao obter TESDataHandler para atualizar Globals.");
            return;
        }
        const std::string bfco = "SCSI-ACTbfco-Main.esp";
        // Mapeia nossas variáveis C++ para os EditorIDs das Globals no .esp
        std::map<const char*, float> globalsToUpdate = {
            {"bfcoTG_KeyAttackComb", Settings::bEnableComboAttack ? 1.0f : 0.0f},
            {"bfcoINT_KeyAttackComb", Settings::bEnableComboAttack ? 2.0f : 0.0f},
            {"bfcoDebug_DisJumpAttack", Settings::bDisableJumpingAttack ? 1.0f : 0.0f},
            {"bfcoTG_DirPowerAttack", Settings::bEnableDirectionalAttack ? 1.0f : 0.0f},
            {"bfcoTG_LmbPowerAttackNUM", (Settings::bPowerAttackLMB == 0) ? 1.0f : 0.0f},
            {"bfcoTG_RmbPowerAttackNUM", Settings::PowerAttackKey_m == Settings::AttackKeyLeft_m ? 1.0f : 0.0f}
        };

        for (auto const& [editorID, value] : globalsToUpdate) {
            RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
            if (global) {
                global->value = value;
                SKSE::log::info("Global '{}' atualizada para o valor: {}", editorID, value);
            } else {
                SKSE::log::warn("Nao foi possivel encontrar a GlobalVariable: {}", editorID);
            }
        }
        

        //SKSE::log::info("Variaveis Globais do BFCO atualizadas com sucesso.");
    }

    // Salva as configurações em JSON
    void SaveSettings() {
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        doc.AddMember("bEnableComboAttack", Settings::bEnableComboAttack, allocator);
        doc.AddMember("comboKey_k", Settings::comboKey_k, allocator);
        doc.AddMember("comboKey_k_mod", Settings::comboKey_k_mod, allocator); // NOVO
        doc.AddMember("comboKey_m", Settings::comboKey_m, allocator);
        doc.AddMember("comboKey_m_mod", Settings::comboKey_m_mod, allocator); // NOVO
        doc.AddMember("comboKey_g", Settings::comboKey_g, allocator);
        doc.AddMember("comboKey_g_mod", Settings::comboKey_g_mod, allocator); // NOVO

        doc.AddMember("bEnableDirectionalAttack", Settings::bEnableDirectionalAttack, allocator);
        doc.AddMember("PowerAttackKey_k", Settings::PowerAttackKey_k, allocator);
        doc.AddMember("PowerAttackKey_k_mod", Settings::PowerAttackKey_k_mod, allocator); // NOVO
        doc.AddMember("PowerAttackKey_m", Settings::PowerAttackKey_m, allocator);
        doc.AddMember("PowerAttackKey_m_mod", Settings::PowerAttackKey_m_mod, allocator); // NOVO
        doc.AddMember("PowerAttackKey_g", Settings::PowerAttackKey_g, allocator);
        doc.AddMember("PowerAttackKey_g_mod", Settings::PowerAttackKey_g_mod, allocator); // NOVO

        doc.AddMember("BlockKey_k", Settings::BlockKey_k, allocator);
        doc.AddMember("BlockKey_k_mod", Settings::BlockKey_k_mod, allocator); // NOVO
        doc.AddMember("BlockKey_m", Settings::BlockKey_m, allocator);
        doc.AddMember("BlockKey_m_mod", Settings::BlockKey_m_mod, allocator); // NOVO
        doc.AddMember("BlockKey_g", Settings::BlockKey_g, allocator);
        doc.AddMember("BlockKey_g_mod", Settings::BlockKey_g_mod, allocator); // NOVO

        doc.AddMember("bEnablePowerAttack", Settings::bEnablePowerAttack, allocator);
        doc.AddMember("bDisableJumpingAttack", Settings::bDisableJumpingAttack, allocator);
        doc.AddMember("bPowerAttackLMB", Settings::bPowerAttackLMB, allocator);
        doc.AddMember("lockSprintAttack", Settings::lockSprintAttack, allocator);
        doc.AddMember("disableMStaBash", Settings::disableMStaBash, allocator);
        doc.AddMember("disableDualblock", Settings::disableDualblock, allocator);

        FILE* fp = nullptr;
        fopen_s(&fp, SETTINGS_PATH, "wb");
        if (fp) {
            char writeBuffer[65536];
            rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
            rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
            doc.Accept(writer);
            fclose(fp);
        }
        UpdateGameGlobals();
    }

    // Carrega as configurações do JSON
    void LoadSettings() {
        FILE* fp = nullptr;
        fopen_s(&fp, SETTINGS_PATH, "rb");
        if (fp) {
            char readBuffer[65536];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            rapidjson::Document doc;
            doc.ParseStream(is);
            fclose(fp);

            if (doc.IsObject()) {
                if (doc.HasMember("bEnableComboAttack")) Settings::bEnableComboAttack = doc["bEnableComboAttack"].GetBool();
                if (doc.HasMember("comboKey_k")) Settings::comboKey_k = doc["comboKey_k"].GetInt();
                if (doc.HasMember("comboKey_k_mod")) Settings::comboKey_k_mod = doc["comboKey_k_mod"].GetInt();
                if (doc.HasMember("comboKey_m")) Settings::comboKey_m = doc["comboKey_m"].GetInt();
                if (doc.HasMember("comboKey_m_mod")) Settings::comboKey_m_mod = doc["comboKey_m_mod"].GetInt();
                if (doc.HasMember("comboKey_g")) Settings::comboKey_g = doc["comboKey_g"].GetInt();
                if (doc.HasMember("comboKey_g_mod")) Settings::comboKey_g_mod = doc["comboKey_g_mod"].GetInt();

                if (doc.HasMember("bEnableDirectionalAttack")) Settings::bEnableDirectionalAttack = doc["bEnableDirectionalAttack"].GetBool();

                if (doc.HasMember("PowerAttackKey_k")) Settings::PowerAttackKey_k = doc["PowerAttackKey_k"].GetInt();
                if (doc.HasMember("PowerAttackKey_k_mod")) Settings::PowerAttackKey_k_mod = doc["PowerAttackKey_k_mod"].GetInt();
                if (doc.HasMember("PowerAttackKey_m")) Settings::PowerAttackKey_m = doc["PowerAttackKey_m"].GetInt();
                if (doc.HasMember("PowerAttackKey_m_mod")) Settings::PowerAttackKey_m_mod = doc["PowerAttackKey_m_mod"].GetInt();
                if (doc.HasMember("PowerAttackKey_g")) Settings::PowerAttackKey_g = doc["PowerAttackKey_g"].GetInt();
                if (doc.HasMember("PowerAttackKey_g_mod")) Settings::PowerAttackKey_g_mod = doc["PowerAttackKey_g_mod"].GetInt();

                if (doc.HasMember("BlockKey_k")) Settings::BlockKey_k = doc["BlockKey_k"].GetInt();
                if (doc.HasMember("BlockKey_k_mod")) Settings::BlockKey_k_mod = doc["BlockKey_k_mod"].GetInt();
                if (doc.HasMember("BlockKey_m")) Settings::BlockKey_m = doc["BlockKey_m"].GetInt();
                if (doc.HasMember("BlockKey_m_mod")) Settings::BlockKey_m_mod = doc["BlockKey_m_mod"].GetInt();
                if (doc.HasMember("BlockKey_g")) Settings::BlockKey_g = doc["BlockKey_g"].GetInt();
                if (doc.HasMember("BlockKey_g_mod")) Settings::BlockKey_g_mod = doc["BlockKey_g_mod"].GetInt();

                if (doc.HasMember("bEnablePowerAttack")) Settings::bEnablePowerAttack = doc["bEnablePowerAttack"].GetBool();
                if (doc.HasMember("bDisableJumpingAttack")) Settings::bDisableJumpingAttack = doc["bDisableJumpingAttack"].GetBool();
                if (doc.HasMember("bPowerAttackLMB")) Settings::bPowerAttackLMB = doc["bPowerAttackLMB"].GetInt();
                if (doc.HasMember("lockSprintAttack")) Settings::lockSprintAttack = doc["lockSprintAttack"].GetBool();
                if (doc.HasMember("disableMStaBash")) Settings::disableMStaBash = doc["disableMStaBash"].GetBool();
                if (doc.HasMember("disableDualblock")) Settings::disableDualblock = doc["disableDualblock"].GetBool();
            }
        }
        UpdateGameGlobals();
    }

    // Registra o menu
    void Register() {
        if (SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("SKSE Menu Framework encontrado. Registrando o menu BFCO.");

            SKSEMenuFramework::SetSection("BFCO");
            SKSEMenuFramework::AddSectionItem("Settings", Render);
        } else {
            SKSE::log::warn("SKSE Menu Framework nao encontrado. O menu BFCO nao sera registrado.");
        }
    }
}