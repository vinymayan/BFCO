#include <Xinput.h>
#include <fstream> // Necessário para ler/escrever arquivos
#include <filesystem> // Necessário para gerenciar caminhos de arquivo
#include "SKSE/SKSE.h"
#include "RE/Skyrim.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h" // Para salvar
#include "rapidjson/prettywriter.h"   // Para salvar de forma legível
#include "ClibUtil/singleton.hpp"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Manager.h"

const std::map<ImGuiKey, int> g_imgui_to_dx_map = {{ImGuiKey_1, 2},
                                                   {ImGuiKey_2, 3},
                                                   {ImGuiKey_3, 4},
                                                   {ImGuiKey_4, 5},
                                                   {ImGuiKey_5, 6},
                                                   {ImGuiKey_6, 7},
                                                   {ImGuiKey_7, 8},
                                                   {ImGuiKey_8, 9},
                                                   {ImGuiKey_9, 10},
                                                   {ImGuiKey_0, 11},
                                                   {ImGuiKey_A, 30},
                                                   {ImGuiKey_B, 48},
                                                   {ImGuiKey_C, 46},
                                                   {ImGuiKey_D, 32},
                                                   {ImGuiKey_E, 18},
                                                   {ImGuiKey_F, 33},
                                                   {ImGuiKey_G, 34},
                                                   {ImGuiKey_H, 35},
                                                   {ImGuiKey_I, 23},
                                                   {ImGuiKey_J, 36},
                                                   {ImGuiKey_K, 37},
                                                   {ImGuiKey_L, 38},
                                                   {ImGuiKey_M, 50},
                                                   {ImGuiKey_N, 49},
                                                   {ImGuiKey_O, 24},
                                                   {ImGuiKey_P, 25},
                                                   {ImGuiKey_Q, 16},
                                                   {ImGuiKey_R, 19},
                                                   {ImGuiKey_S, 31},
                                                   {ImGuiKey_T, 20},
                                                   {ImGuiKey_U, 22},
                                                   {ImGuiKey_V, 47},
                                                   {ImGuiKey_W, 17},
                                                   {ImGuiKey_X, 45},
                                                   {ImGuiKey_Y, 21},
                                                   {ImGuiKey_Z, 44},
                                                   {ImGuiKey_F1, 59},
                                                   {ImGuiKey_F2, 60},
                                                   {ImGuiKey_F3, 61},
                                                   {ImGuiKey_F4, 62},
                                                   {ImGuiKey_F5, 63},
                                                   {ImGuiKey_F6, 64},
                                                   {ImGuiKey_F7, 65},
                                                   {ImGuiKey_F8, 66},
                                                   {ImGuiKey_F9, 67},
                                                   {ImGuiKey_F10, 68},
                                                   {ImGuiKey_F11, 87},
                                                   {ImGuiKey_F12, 88},
                                                   {ImGuiKey_Space, 57},
                                                   {ImGuiKey_Enter, 28},
                                                   {ImGuiKey_KeypadEnter, 156},
                                                   {ImGuiKey_Backspace, 14},
                                                   {ImGuiKey_Tab, 15},
                                                   {ImGuiKey_LeftCtrl, 29},
                                                   {ImGuiKey_RightCtrl, 157},
                                                   {ImGuiKey_LeftShift, 42},
                                                   {ImGuiKey_RightShift, 54},
                                                   {ImGuiKey_LeftAlt, 56},
                                                   {ImGuiKey_RightAlt, 184},
                                                   {ImGuiKey_Delete, 211},
                                                   {ImGuiKey_Insert, 210},
                                                   {ImGuiKey_Home, 199},
                                                   {ImGuiKey_End, 207},
                                                   {ImGuiKey_PageUp, 201},
                                                   {ImGuiKey_PageDown, 209},
                                                   {ImGuiKey_UpArrow, 200},
                                                   {ImGuiKey_DownArrow, 208},
                                                   {ImGuiKey_LeftArrow, 203},
                                                   {ImGuiKey_RightArrow, 205},
                                                   {ImGuiKey_MouseLeft, 300},
                                                   {ImGuiKey_MouseRight, 301},
                                                   {ImGuiKey_MouseMiddle, 302},
                                                   {ImGuiKey_MouseX1, 303},
                                                   {ImGuiKey_MouseX2, 304}};
const std::map<int, const char*> g_dx_to_name_map = {{0, "[Nenhuma]"},
                                                     {1, "Escape"},
                                                     {2, "1"},
                                                     {3, "2"},
                                                     {4, "3"},
                                                     {5, "4"},
                                                     {6, "5"},
                                                     {7, "6"},
                                                     {8, "7"},
                                                     {9, "8"},
                                                     {10, "9"},
                                                     {11, "0"},
                                                     {12, "-"},
                                                     {13, "="},
                                                     {14, "Backspace"},
                                                     {15, "Tab"},
                                                     {16, "Q"},
                                                     {17, "W"},
                                                     {18, "E"},
                                                     {19, "R"},
                                                     {20, "T"},
                                                     {21, "Y"},
                                                     {22, "U"},
                                                     {23, "I"},
                                                     {24, "O"},
                                                     {25, "P"},
                                                     {28, "Enter"},
                                                     {29, "Left Ctrl"},
                                                     {30, "A"},
                                                     {31, "S"},
                                                     {32, "D"},
                                                     {33, "F"},
                                                     {34, "G"},
                                                     {35, "H"},
                                                     {36, "J"},
                                                     {37, "K"},
                                                     {38, "L"},
                                                     {39, ";"},
                                                     {42, "Left Shift"},
                                                     {43, "\\"},
                                                     {44, "Z"},
                                                     {45, "X"},
                                                     {46, "C"},
                                                     {47, "V"},
                                                     {48, "B"},
                                                     {49, "N"},
                                                     {50, "M"},
                                                     {51, ","},
                                                     {52, "."},
                                                     {53, "/"},
                                                     {54, "Right Shift"},
                                                     {56, "Left Alt"},
                                                     {57, "Spacebar"},
                                                     {59, "F1"},
                                                     {60, "F2"},
                                                     {61, "F3"},
                                                     {62, "F4"},
                                                     {63, "F5"},
                                                     {64, "F6"},
                                                     {65, "F7"},
                                                     {66, "F8"},
                                                     {67, "F9"},
                                                     {68, "F10"},
                                                     {87, "F11"},
                                                     {88, "F12"},
                                                     {156, "Keypad Enter"},
                                                     {157, "Right Ctrl"},
                                                     {184, "Right Alt"},
                                                     {199, "Home"},
                                                     {200, "Up Arrow"},
                                                     {201, "PgUp"},
                                                     {203, "Left Arrow"},
                                                     {205, "Right Arrow"},
                                                     {207, "End"},
                                                     {208, "Down Arrow"},
                                                     {209, "PgDown"},
                                                     {210, "Insert"},
                                                     {211, "Delete"},
                                                     {300, "Mouse Esq."},
                                                     {301, "Mouse Dir."},
                                                     {302, "Mouse Meio"},
                                                     {303, "Mouse 4"},
                                                     {304, "Mouse 5"}};
const std::map<int, const char*> g_gamepad_dx_to_name_map = {
    {0, "[Nenhum]"},  {4096, "A / X"},  {8192, "B / O"},  {16384, "X / Square"}, {32768, "Y / Triangle"},
    {256, "LB / L1"}, {512, "RB / R1"}, {9, "LT / L2"},   {10, "RT / R2"},       {64, "L3 / LS"},
    {128, "R3 / RS"}, {1, "DPad Up"},   {2, "DPad Down"}, {4, "DPad Left"},      {8, "DPad Right"},
    {16, "Start"},    {32, "Back"}};

constexpr const char* settings_path = "Data/SKSE/Plugins/BFCO_Settings.json";

    
void SettingsMenu::Save() {
        logger::info("Salvando configurações em {}", settings_path);

        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        // Adiciona cada configuração da struct g_settings ao documento JSON
        doc.AddMember("allowZeroStamina", g_settings.allowZeroStamina, allocator);
        doc.AddMember("onlyDuringAttack", g_settings.onlyDuringAttack, allocator);
        doc.AddMember("disableBlockDuringAttack", g_settings.disableBlockDuringAttack, allocator);
        doc.AddMember("queuePA", g_settings.queuePA, allocator);
        doc.AddMember("notifyWindow", g_settings.notifyWindow, allocator);
        doc.AddMember("notifyDuration", g_settings.notifyDuration, allocator);
        doc.AddMember("queuePAExpire", g_settings.queuePAExpire, allocator);
        doc.AddMember("enableComboKey", g_settings.enableComboKey, allocator);
        doc.AddMember("lightAttackToPowerAttack", g_settings.lightAttackToPowerAttack, allocator);
        doc.AddMember("blockToPowerAttack", g_settings.blockToPowerAttack, allocator);
        doc.AddMember("disableJumpingAttack", g_settings.disableJumpingAttack, allocator);
        doc.AddMember("debugPAPress", g_settings.debugPAPress, allocator);
        doc.AddMember("debugPAActivate", g_settings.debugPAActivate, allocator);

        // Garante que o diretório de plugins exista
        std::filesystem::path config_path(settings_path);
        std::filesystem::create_directories(config_path.parent_path());

        // Escreve o JSON no arquivo
        std::ofstream ofs(settings_path);
        if (ofs.is_open()) {
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);
            ofs.close();
            logger::info("Configurações salvas com sucesso.");
        } else {
            logger::error("Falha ao abrir o arquivo para salvar as configurações: {}", settings_path);
        }
    }

    // Função para carregar as configurações do arquivo JSON
    void SettingsMenu::Load() {
        logger::info("Carregando configurações de {}", settings_path);

        std::ifstream ifs(settings_path);
        if (!ifs.is_open()) {
            logger::warn("Arquivo de configurações não encontrado. Criando um novo com valores padrão.");
            Save();  // Se o arquivo não existe, cria um com os valores padrão
            return;
        }

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;
        doc.ParseStream(isw);
        ifs.close();

        if (doc.HasParseError() || !doc.IsObject()) {
            logger::error("Falha ao analisar o arquivo de configurações. Usando valores padrão.");
            return;
        }

        // Carrega cada valor do JSON para a struct g_settings, verificando o tipo
        if (doc.HasMember("allowZeroStamina") && doc["allowZeroStamina"].IsBool())
            g_settings.allowZeroStamina = doc["allowZeroStamina"].GetBool();
        if (doc.HasMember("onlyDuringAttack") && doc["onlyDuringAttack"].IsBool())
            g_settings.onlyDuringAttack = doc["onlyDuringAttack"].GetBool();
        if (doc.HasMember("disableBlockDuringAttack") && doc["disableBlockDuringAttack"].IsBool())
            g_settings.disableBlockDuringAttack = doc["disableBlockDuringAttack"].GetBool();
        if (doc.HasMember("queuePA") && doc["queuePA"].IsBool()) g_settings.queuePA = doc["queuePA"].GetBool();
        if (doc.HasMember("notifyWindow") && doc["notifyWindow"].IsBool())
            g_settings.notifyWindow = doc["notifyWindow"].GetBool();
        if (doc.HasMember("notifyDuration") && doc["notifyDuration"].IsFloat())
            g_settings.notifyDuration = doc["notifyDuration"].GetFloat();
        if (doc.HasMember("queuePAExpire") && doc["queuePAExpire"].IsFloat())
            g_settings.queuePAExpire = doc["queuePAExpire"].GetFloat();
        if (doc.HasMember("enableComboKey") && doc["enableComboKey"].IsBool())
            g_settings.enableComboKey = doc["enableComboKey"].GetBool();
        if (doc.HasMember("lightAttackToPowerAttack") && doc["lightAttackToPowerAttack"].IsBool())
            g_settings.lightAttackToPowerAttack = doc["lightAttackToPowerAttack"].GetBool();
        if (doc.HasMember("blockToPowerAttack") && doc["blockToPowerAttack"].IsBool())
            g_settings.blockToPowerAttack = doc["blockToPowerAttack"].GetBool();
        if (doc.HasMember("disableJumpingAttack") && doc["disableJumpingAttack"].IsBool())
            g_settings.disableJumpingAttack = doc["disableJumpingAttack"].GetBool();
        if (doc.HasMember("debugPAPress") && doc["debugPAPress"].IsBool())
            g_settings.debugPAPress = doc["debugPAPress"].GetBool();
        if (doc.HasMember("debugPAActivate") && doc["debugPAActivate"].IsBool())
            g_settings.debugPAActivate = doc["debugPAActivate"].GetBool();

        logger::info("Configurações carregadas com sucesso.");
    }


    



void __stdcall SettingsMenu::Render() {
    bool settings_changed = false;

    ImGui::Text("Configurações de Combate e Ataque Poderoso");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::BeginTabItem("Geral")) {
            ImGui::Spacing();

            if (ImGui::Checkbox("Permitir ataque poderoso com estamina zero", &g_settings.allowZeroStamina))
                settings_changed = true;
            if (ImGui::Checkbox("Tecla 'V' só funciona durante um ataque", &g_settings.onlyDuringAttack))
                settings_changed = true;
            if (ImGui::Checkbox("Desativar bloqueio durante ataque", &g_settings.disableBlockDuringAttack))
                settings_changed = true;
            if (ImGui::Checkbox("Impedir ataque durante o pulo", &g_settings.disableJumpingAttack))
                settings_changed = true;

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Combos e Fila")) {
            ImGui::Spacing();

            if (ImGui::Checkbox("Habilitar tecla de combo (Clique Direito)", &g_settings.enableComboKey))
                settings_changed = true;
            if (ImGui::Checkbox("Enfileirar ataque poderoso se a animação não estiver pronta", &g_settings.queuePA))
                settings_changed = true;
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("Tempo de expiração da fila (s)", &g_settings.queuePAExpire, 0.1f, 1.0f, "%.2f s"))
                settings_changed = true;

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Ataque Poderoso ao Segurar Botão");
            if (ImGui::Checkbox("Segurar Ataque Leve para Ataque Poderoso", &g_settings.lightAttackToPowerAttack))
                settings_changed = true;
            if (ImGui::Checkbox("Segurar Bloqueio para Ataque Poderoso (Vanilla)", &g_settings.blockToPowerAttack))
                settings_changed = true;

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Visual e Debug")) {
            ImGui::Spacing();

            if (ImGui::Checkbox("Mostrar efeito visual na janela de ataque", &g_settings.notifyWindow))
                settings_changed = true;
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("Duração do efeito visual (s)", &g_settings.notifyDuration, 0.01f, 0.5f, "%.2f s"))
                settings_changed = true;

            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Checkbox("[Debug] Tocar som ao pressionar tecla 'V'", &g_settings.debugPAPress))
                settings_changed = true;
            if (ImGui::Checkbox("[Debug] Tocar som na ativação do ataque poderoso", &g_settings.debugPAActivate))
                settings_changed = true;

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Se qualquer configuração mudou, salva no arquivo
    if (settings_changed) {
        Save();
    }
}

// Função para registrar o menu no SKSE Menu Framework
void SettingsMenu::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        logger::warn("SKSE Menu Framework não foi encontrado. O menu de configurações não estará disponível.");
        return;
    }

    SKSEMenuFramework::SetSection("BFCO Power Attack");
    SKSEMenuFramework::AddSectionItem("Configurações", Render);
    logger::info("Menu de configurações registrado com sucesso.");
}