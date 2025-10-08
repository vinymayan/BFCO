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
    void RenderKeybind(const char* label, uint32_t* key_k, uint32_t* key_m, uint32_t* key_g) {
        bool settings_changed = false;

        // --- DROPDOWN UNIFICADO PARA TECLADO E MOUSE ---
        ImGui::Text("%s (PC)", label);
        ImGui::SameLine();

        // 1. Descobre qual tecla (teclado ou mouse) está atualmente selecionada.
        uint32_t current_pc_key = (*key_k != 0) ? *key_k : *key_m;

        // 2. Busca o nome da tecla atual para exibir no dropdown fechado.
        const char* current_key_name_pc = "[Nenhuma]";
        auto it_pc = g_dx_to_name_map.find(current_pc_key);
        if (it_pc != g_dx_to_name_map.end()) {
            current_key_name_pc = it_pc->second;
        }

        // 3. Cria o ComboBox.
        if (ImGui::BeginCombo((std::string("##_pc_") + label).c_str(), current_key_name_pc)) {
            // Itera por TODAS as teclas (teclado e mouse) no mapa.
            for (auto const& [key_code, key_name] : g_dx_to_name_map) {
                const bool is_selected = (current_pc_key == key_code);
                if (ImGui::Selectable(key_name, is_selected)) {
                    // 4. LÓGICA DE EXCLUSÃO MÚTUA
                    // Se a tecla selecionada for do teclado (< 256)...
                    if (key_code < 256) {
                        *key_k = key_code;  // Define a tecla do teclado.
                        *key_m = 0;         // Limpa a tecla do mouse.
                    } else {                // Senão, é uma tecla do mouse.
                        *key_m = key_code;  // Define a tecla do mouse.
                        *key_k = 0;         // Limpa a tecla do teclado.
                    }
                    settings_changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // --- CONTROLE ---
        const char* current_key_name_g = "[Nenhuma]";  // Valor padrão
        auto it_g = g_gamepad_dx_to_name_map.find(*key_g);
        if (it_g != g_gamepad_dx_to_name_map.end()) {
            current_key_name_g = it_g->second;
        }

        ImGui::Text("%s (Gamepad)", label);
        ImGui::SameLine();
        if (ImGui::BeginCombo((std::string("##_g_") + label).c_str(), current_key_name_g)) {
            for (auto const& [key_code, key_name] : g_gamepad_dx_to_name_map) {
                const bool is_selected = (*key_g == key_code);
                if (ImGui::Selectable(key_name, is_selected)) {
                    *key_g = key_code;
                    settings_changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

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

        if (ImGui::Checkbox("Enable Combo attack", &Settings::bEnableComboAttack)) settings_changed = true;
        RenderKeybind("Combo Attack key", &Settings::comboKey_k, &Settings::comboKey_m, &Settings::comboKey_g);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Checkbox("Enable direcional power attack", &Settings::bEnableDirectionalAttack)) settings_changed = true;
        RenderKeybind("Power Attack Key", &Settings::PowerAttackKey_k, &Settings::PowerAttackKey_m,
                      &Settings::PowerAttackKey_g);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();


        RenderKeybind("Block Key (Beta)", &Settings::BlockKey_k, &Settings::BlockKey_m, &Settings::BlockKey_g);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        /*if (ImGui::Checkbox("Ativar Ataque Carregado (Clique Esquerdo)", &Settings::bEnableLmbPowerAttack))
            settings_changed = true;
        if (ImGui::Checkbox("Ativar Ataque Carregado (Clique Direito)", &Settings::bEnableRmbPowerAttack))
            settings_changed = true;*/
        if (ImGui::Checkbox("Disable jump attack (Beta)", &Settings::bDisableJumpingAttack)) settings_changed = true;
        if (ImGui::Checkbox("Disable power attack LMB (Beta)", &Settings::bPowerAttackLMB)) settings_changed = true;
        //if (ImGui::Checkbox("Lock sprint attack behind perk", &Settings::lockSprintAttack)) settings_changed = true;

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
            {"bfcoDebug_DisJumpAttack", Settings::bDisableJumpingAttack ? 1.0f : 0.0f},
            {"BFCO_StrongAttackIsOK", 1.0f },
            {"BFCO_NormalAttackIsOK", 1.0f },
            //{"BFCO_Global_ComboKey",static_cast<float>(Settings::comboKey_k)},  // Supondo que o script use o valor do teclado
            {"bfcoTG_DirPowerAttack", Settings::bEnableDirectionalAttack ? 1.0f : 0.0f},
            // Adicione outras Globals aqui conforme o MCM original
            {"bfcoTG_LmbPowerAttackNUM", Settings::bPowerAttackLMB ? 0.0f : 1.0f}
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
        doc.AddMember("comboKey_m", Settings::comboKey_m, allocator);
        doc.AddMember("comboKey_g", Settings::comboKey_g, allocator);

        doc.AddMember("bEnableDirectionalAttack", Settings::bEnableDirectionalAttack, allocator);
        doc.AddMember("PowerAttackKey_k", Settings::PowerAttackKey_k, allocator);
        doc.AddMember("PowerAttackKey_m", Settings::PowerAttackKey_m, allocator);
        doc.AddMember("PowerAttackKey_g", Settings::PowerAttackKey_g, allocator);

        doc.AddMember("BlockKey_k", Settings::BlockKey_k, allocator);
        doc.AddMember("BlockKey_m", Settings::BlockKey_m, allocator);
        doc.AddMember("BlockKey_g", Settings::BlockKey_g, allocator);

        //doc.AddMember("AttackKey_k", Settings::AttackKey_k, allocator);
        //doc.AddMember("AttackKey_m", Settings::AttackKey_m, allocator);
        //doc.AddMember("AttackKey_g", Settings::AttackKey_g, allocator);

        /*doc.AddMember("bEnableLmbPowerAttack", Settings::bEnableLmbPowerAttack, allocator);
        doc.AddMember("bEnableRmbPowerAttack", Settings::bEnableRmbPowerAttack, allocator);*/
        doc.AddMember("bDisableJumpingAttack", Settings::bDisableJumpingAttack, allocator);
        doc.AddMember("bPowerAttackLMB", Settings::bPowerAttackLMB, allocator);
        doc.AddMember("lockSprintAttack", Settings::lockSprintAttack, allocator);

        FILE* fp = nullptr;
        fopen_s(&fp, SETTINGS_PATH, "wb");
        if (fp) {
            char writeBuffer[65536];
            rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
            rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
            doc.Accept(writer);
            fclose(fp);
        }

        // Após salvar, sempre atualize as Globals do jogo
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
                if (doc.HasMember("bEnableComboAttack") && doc["bEnableComboAttack"].IsBool())
                    Settings::bEnableComboAttack = doc["bEnableComboAttack"].GetBool();
                if (doc.HasMember("comboKey_k") && doc["comboKey_k"].IsInt())
                    Settings::comboKey_k = doc["comboKey_k"].GetInt();
                if (doc.HasMember("comboKey_m") && doc["comboKey_m"].IsInt())
                    Settings::comboKey_m = doc["comboKey_m"].GetInt();
                if (doc.HasMember("comboKey_g") && doc["comboKey_g"].IsInt())
                    Settings::comboKey_g = doc["comboKey_g"].GetInt();

                if (doc.HasMember("bEnableDirectionalAttack") && doc["bEnableDirectionalAttack"].IsBool())
                    Settings::bEnableDirectionalAttack = doc["bEnableDirectionalAttack"].GetBool();

                if (doc.HasMember("PowerAttackKey_k") && doc["PowerAttackKey_k"].IsInt())
                    Settings::PowerAttackKey_k = doc["PowerAttackKey_k"].GetInt();
                if (doc.HasMember("PowerAttackKey_m") && doc["PowerAttackKey_m"].IsInt())
                    Settings::PowerAttackKey_m = doc["PowerAttackKey_m"].GetInt();
                if (doc.HasMember("PowerAttackKey_g") && doc["PowerAttackKey_g"].IsInt())
                    Settings::PowerAttackKey_g = doc["PowerAttackKey_g"].GetInt();

                if (doc.HasMember("BlockKey_k") && doc["BlockKey_k"].IsInt())
                    Settings::BlockKey_k = doc["BlockKey_k"].GetInt();
                if (doc.HasMember("BlockKey_m") && doc["BlockKey_m"].IsInt())
                    Settings::BlockKey_m = doc["BlockKey_m"].GetInt();
                if (doc.HasMember("BlockKey_g") && doc["BlockKey_g"].IsInt())
                    Settings::BlockKey_g = doc["BlockKey_g"].GetInt();

                //if (doc.HasMember("AttackKey_k") && doc["AttackKey_k"].IsInt())
                //    Settings::AttackKey_k = doc["AttackKey_k"].GetInt();
                //if (doc.HasMember("AttackKey_m") && doc["AttackKey_m"].IsInt())
                //    Settings::AttackKey_m = doc["AttackKey_m"].GetInt();
                //if (doc.HasMember("AttackKey_g") && doc["AttackKey_g"].IsInt())
                //    Settings::AttackKey_g = doc["AttackKey_g"].GetInt();

                /*if (doc.HasMember("bEnableLmbPowerAttack") && doc["bEnableLmbPowerAttack"].IsBool())
                    Settings::bEnableLmbPowerAttack = doc["bEnableLmbPowerAttack"].GetBool();
                if (doc.HasMember("bEnableRmbPowerAttack") && doc["bEnableRmbPowerAttack"].IsBool())
                    Settings::bEnableRmbPowerAttack = doc["bEnableRmbPowerAttack"].GetBool();*/
                if (doc.HasMember("bDisableJumpingAttack") && doc["bDisableJumpingAttack"].IsBool())
                    Settings::bDisableJumpingAttack = doc["bDisableJumpingAttack"].GetBool();
                if (doc.HasMember("bPowerAttackLMB") && doc["bPowerAttackLMB"].IsBool())
                    Settings::bPowerAttackLMB = doc["bPowerAttackLMB"].GetBool();
                if (doc.HasMember("lockSprintAttack") && doc["lockSprintAttack"].IsBool())
                    Settings::lockSprintAttack = doc["lockSprintAttack"].GetBool();
            }
        }
        // Após carregar, sempre atualize as Globals do jogo
        UpdateGameGlobals();
    }

    // Registra o menu
    void Register() {
        if (SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("SKSE Menu Framework encontrado. Registrando o menu BFCO.");

            // Carrega as configurações do arquivo ao iniciar
           

            SKSEMenuFramework::SetSection("BFCO");
            SKSEMenuFramework::AddSectionItem("Settings", Render);
        } else {
            SKSE::log::warn("SKSE Menu Framework nao encontrado. O menu BFCO nao sera registrado.");
        }
    }
}