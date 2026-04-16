#include "Settings.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
namespace ImGui = ImGuiMCP;
namespace BFCOMenu {

    // Caminho para o nosso arquivo de configurações
    const char* SETTINGS_PATH = "Data/SKSE/Plugins/BFCO_Settings.json";
    const char* LANG_PATH = "Data/SKSE/Plugins/BFCO_Language.json";
    static std::unordered_map<std::string, std::string> LangMap;

    void LoadLanguage() {
        LangMap.clear();

        // Lê o arquivo usando fstream
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::warn("Nao foi possivel carregar BFCO_Language.json. Usando textos padroes.");
            return;
        }

        // Passa o conteúdo do arquivo para uma std::string
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        // Verifica e remove o BOM (Byte Order Mark) do UTF-8, se existir
        if (jsonStr.size() >= 3 &&
            (unsigned char)jsonStr[0] == 0xEF &&
            (unsigned char)jsonStr[1] == 0xBB &&
            (unsigned char)jsonStr[2] == 0xBF) {
            jsonStr.erase(0, 3);
        }

        // Agora faz o parse da string limpa
        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

        // Loga se houver erro de sintaxe no JSON (muito útil para debugar)
        if (doc.HasParseError()) {
            SKSE::log::error("Erro ao analisar BFCO_Language.json (Parse Error: {} no offset {})",
                (unsigned)doc.GetParseError(), doc.GetErrorOffset());
            return;
        }

        if (doc.IsObject()) {
            for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
                if (itr->value.IsObject()) {
                    std::string category = itr->name.GetString();
                    for (auto jtr = itr->value.MemberBegin(); jtr != itr->value.MemberEnd(); ++jtr) {
                        if (jtr->value.IsString()) {
                            // Cria chaves no formato "categoria.chave" (ex: "common.save")
                            LangMap[category + "." + jtr->name.GetString()] = jtr->value.GetString();
                        }
                    }
                }
                else if (itr->value.IsString()) {
                    LangMap[itr->name.GetString()] = itr->value.GetString();
                }
            }
        }

        SKSE::log::info("Idioma carregado com {} entradas.", LangMap.size());
    }

    // Função de resgate de String Traduzida
    const char* GetLoc(const std::string& key, const char* defaultVal) {
        auto it = LangMap.find(key);
        if (it != LangMap.end()) {
            return it->second.c_str();
        }
        return defaultVal;
    }

    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    inline int GetIndexFromID(int id, const int* idArray, int arraySize) {
        for (int i = 0; i < arraySize; i++) {
            if (idArray[i] == id) return i;
        }
        return 0;
    }

    inline bool SearchableCombo(const char* label, int* current_item, const char* const items[], int items_count) {
        bool changed = false;
        const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : GetLoc("common.none", "None");

        if (ImGuiMCP::BeginCombo(label, preview_value)) {
            static char searchBuf[128] = "";
            if (ImGuiMCP::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGuiMCP::SetKeyboardFocusHere();
            }
            std::string searchLabel = std::string(GetLoc("common.search_placeholder", "Filter...")) + "##Search";
            ImGuiMCP::InputText(searchLabel.c_str(), searchBuf, sizeof(searchBuf));
            ImGuiMCP::Separator();

            std::string searchLower = ToLower(searchBuf);

            for (int i = 0; i < items_count; i++) {
                if (searchLower.empty() || ToLower(items[i]).find(searchLower) != std::string::npos) {
                    bool is_selected = (*current_item == i);
                    if (ImGuiMCP::Selectable(items[i], is_selected)) {
                        *current_item = i;
                        changed = true;
                    }
                    if (is_selected && ImGuiMCP::IsWindowAppearing()) {
                        ImGuiMCP::SetScrollHereY();
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }
        return changed;
    }

    // Variáveis de Estado Temporário de Edição
    struct EditState {
        int current_edit_action_id = -1;
        InputManagerAPI::ActionInfo edit_info;
        char edit_nameBuf[64] = "";
        int ui_pcMainIdx = 0, ui_pcModIdx = 0, current_pcModAct = 0;
        int ui_padMainIdx = 0, ui_padModIdx = 0, current_padModAct = 0;
        std::string updateStatusMsg = "";
        bool updateSuccess = false;
    };

    static std::map<std::string, EditState> editStates;

    bool RenderInputSelector(const char* label, const char* purpose, int& inputType, int& actionID, int& motionID) {
        bool changed = false;

        // Pega ou cria o estado isolado para este seletor específico
        EditState& state = editStates[label];

        ImGui::Text("%s", label);

        if (!InputManagerAPI::_API) {
            ImGui::TextDisabled("%s", GetLoc("input.no_input_manager", "[Input Manager nao detectado]"));
            return false;
        }

        // --- 1. Dropdown de Tipo (Action ou Motion) ---
        const char* typeItems[] = {
            GetLoc("input.type_action", "Action (Normal Input)"),
            GetLoc("input.type_motion", "Motion (Combo Sequence)")
        };

        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo((std::string(GetLoc("input.input_type", "Input Type")) + "##" + label).c_str(), typeItems[inputType])) {
            for (int i = 0; i < 2; i++) {
                if (ImGui::Selectable(typeItems[i], inputType == i)) {
                    if (inputType == 0 && actionID != -1) InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, false);
                    if (inputType == 1 && motionID != -1) InputManagerAPI::_API->UpdateListener(1, motionID, "BFCO", purpose, false);

                    inputType = i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        // --- 2. Lógica para Action (Tipo 0) ---
        if (inputType == 0) {
            size_t actionCount = InputManagerAPI::_API->GetInputCount(0);
            std::string previewValue = GetLoc("input.no_action", "[Nenhuma Acao Selecionada]");

            if (actionID >= 0 && actionID < actionCount) {
                auto info = InputManagerAPI::_API->GetActionInfo(actionID);
                previewValue = "[" + std::to_string(actionID) + "] " + (info.name ? std::string(info.name) : GetLoc("common.unnamed", "Unnamed"));
            }

            ImGui::SetNextItemWidth(250);
            if (ImGui::BeginCombo((std::string(GetLoc("input.select_action", "Select Action")) + "##" + label).c_str(), previewValue.c_str())) {
                if (ImGui::Selectable(GetLoc("common.disabled", "[Desativado]"), actionID == -1)) {
                    if (actionID != -1) InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, false);
                    actionID = -1;
                    changed = true;
                }
                for (int i = 0; i < actionCount; ++i) {
                    auto info = InputManagerAPI::_API->GetActionInfo(i);
                    std::string itemLabel = "[" + std::to_string(i) + "] " + (info.name ? std::string(info.name) : GetLoc("common.unnamed", "Unnamed"));
                    if (ImGui::Selectable(itemLabel.c_str(), actionID == i)) {
                        if (actionID != -1) InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, false);
                        actionID = i;
                        InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, true);
                        changed = true;
                        state.current_edit_action_id = -1; // Força recarregar os dados na UI
                    }
                }
                ImGui::EndCombo();
            }

            // ================== EDITOR DE ACTION ==================
            if (actionID != -1) {
                if (ImGui::TreeNode((std::string(GetLoc("input.edit_keys", "Edit Keys for ")) + label).c_str())) {

                    if (state.current_edit_action_id != actionID) {
                        state.edit_info = InputManagerAPI::_API->GetActionInfo(actionID);
                        state.current_edit_action_id = actionID;

                        strncpy_s(state.edit_nameBuf, state.edit_info.name ? state.edit_info.name : GetLoc("input.unnamed_action", "Unnamed Action"), sizeof(state.edit_nameBuf) - 1);
                        state.edit_nameBuf[sizeof(state.edit_nameBuf) - 1] = '\0';

                        state.ui_pcMainIdx = GetIndexFromID(state.edit_info.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
                        state.current_pcModAct = state.edit_info.pcModAction;
                        state.ui_pcModIdx = (state.current_pcModAct == 3) ? 0 : GetIndexFromID(state.edit_info.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));

                        state.ui_padMainIdx = GetIndexFromID(state.edit_info.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                        state.current_padModAct = state.edit_info.gamepadModAction;
                        state.ui_padModIdx = (state.current_padModAct == 3) ? 0 : GetIndexFromID(state.edit_info.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                        state.updateStatusMsg = "";
                    }

                    ImGui::InputText(GetLoc("input.input_name", "Input Name"), state.edit_nameBuf, sizeof(state.edit_nameBuf));
                    ImGui::Separator();

                    // --- PC ---
                    ImGui::TextColored({ 0.4f, 0.8f, 1.0f, 1.0f }, "%s", GetLoc("input.pc_header", "Keyboard and Mouse"));
                    if (SearchableCombo(GetLoc("input.pc_main_key", "PC Main Key"), &state.ui_pcMainIdx, pcKeyNames, std::size(pcKeyNames))) {
                        state.edit_info.pcMainKey = pcKeyIDs[state.ui_pcMainIdx];
                    }

                    if (ImGui::Combo(GetLoc("input.pc_main_action", "PC Main Action"), &state.edit_info.pcMainAction, actionStateNames, 3)) {
                        if (state.edit_info.pcMainAction != 2 && state.current_pcModAct == 3) {
                            state.current_pcModAct = 0;
                        }
                    }

                    if (state.edit_info.pcMainAction == 1) {
                        if (state.edit_info.pcMainTapCount < 1) state.edit_info.pcMainTapCount = 1;
                        ImGui::SliderInt(GetLoc("input.pc_main_tap", "PC Main Tap Amount"), &state.edit_info.pcMainTapCount, 1, 5);
                    }

                    if (ImGui::BeginCombo(GetLoc("input.pc_mod_action", "PC Mod Action"), actionStateNames[state.current_pcModAct])) {
                        for (int i = 0; i < std::size(actionStateNames); i++) {
                            if (i == 3 && state.edit_info.pcMainAction != 2) continue;

                            bool is_selected = (state.current_pcModAct == i);
                            if (ImGui::Selectable(actionStateNames[i], is_selected)) {
                                state.current_pcModAct = i;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (state.current_pcModAct != state.edit_info.pcModAction) {
                        if (state.current_pcModAct == 3) {
                            state.edit_info.pcModifierKey = 0;
                        }
                        else if (state.current_pcModAct != 0) {
                            state.ui_pcModIdx = 0;
                            state.edit_info.pcModifierKey = pcKeyIDs[state.ui_pcModIdx];
                        }
                        else {
                            state.edit_info.pcModifierKey = 0;
                        }
                        state.edit_info.pcModAction = state.current_pcModAct;
                    }

                    if (state.edit_info.pcModAction == 3) {
                        int gestIdx = state.edit_info.pcModifierKey;
                        std::string gesturePreview = (gestIdx >= 0 && gestIdx < InputManagerAPI::_API->GetInputCount(2))
                            ? InputManagerAPI::_API->GetInputName(2, gestIdx) : GetLoc("input.no_gesture", "[ No Gesture ]");

                        if (ImGui::BeginCombo(GetLoc("input.pc_gesture", "PC Gesture"), gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < InputManagerAPI::_API->GetInputCount(2); ++gIdx) {
                                if (ImGui::Selectable(InputManagerAPI::_API->GetInputName(2, (int)gIdx), gestIdx == (int)gIdx)) {
                                    state.edit_info.pcModifierKey = (int)gIdx;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else if (state.edit_info.pcModAction != 0) {
                        if (SearchableCombo(GetLoc("input.pc_mod_key", "PC Mod Key"), &state.ui_pcModIdx, pcKeyNames, std::size(pcKeyNames))) {
                            state.edit_info.pcModifierKey = pcKeyIDs[state.ui_pcModIdx];
                        }

                        if (state.edit_info.pcModAction == 1) {
                            if (state.edit_info.pcModTapCount < 1) state.edit_info.pcModTapCount = 1;
                            ImGui::SliderInt(GetLoc("input.pc_mod_tap", "PC Mod Tap Amount"), &state.edit_info.pcModTapCount, 1, 5);
                        }
                    }

                    // --- GAMEPAD ---
                    ImGui::Separator();
                    ImGui::TextColored({ 0.4f, 0.8f, 1.0f, 1.0f }, "%s", GetLoc("input.pad_header", "Gamepad"));
                    if (SearchableCombo(GetLoc("input.pad_main_key", "Pad Main Key"), &state.ui_padMainIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                        state.edit_info.gamepadMainKey = gamepadKeyIDs[state.ui_padMainIdx];
                    }

                    if (ImGui::Combo(GetLoc("input.pad_main_action", "Pad Main Action"), &state.edit_info.gamepadMainAction, actionStateNames, 3)) {
                        if (state.edit_info.gamepadMainAction != 2 && state.current_padModAct == 3) {
                            state.current_padModAct = 0;
                        }
                    }

                    if (state.edit_info.gamepadMainAction == 1) {
                        if (state.edit_info.gamepadMainTapCount < 1) state.edit_info.gamepadMainTapCount = 1;
                        ImGui::SliderInt(GetLoc("input.pad_main_tap", "Pad Main Tap Amount"), &state.edit_info.gamepadMainTapCount, 1, 5);
                    }

                    if (ImGui::BeginCombo(GetLoc("input.pad_mod_action", "Pad Mod Action"), actionStateNames[state.current_padModAct])) {
                        for (int i = 0; i < std::size(actionStateNames); i++) {
                            if (i == 3 && state.edit_info.gamepadMainAction != 2) continue;

                            bool is_selected = (state.current_padModAct == i);
                            if (ImGui::Selectable(actionStateNames[i], is_selected)) {
                                state.current_padModAct = i;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (state.current_padModAct != state.edit_info.gamepadModAction) {
                        if (state.current_padModAct == 3) {
                            state.edit_info.gamepadModifierKey = 0;
                        }
                        else if (state.current_padModAct != 0) {
                            state.ui_padModIdx = 0;
                            state.edit_info.gamepadModifierKey = gamepadKeyIDs[state.ui_padModIdx];
                        }
                        else {
                            state.edit_info.gamepadModifierKey = 0;
                        }
                        state.edit_info.gamepadModAction = state.current_padModAct;
                    }

                    if (state.edit_info.gamepadModAction == 3) {
                        int gestIdx = state.edit_info.gamepadModifierKey;
                        std::string gesturePreview = (gestIdx >= 0 && gestIdx < InputManagerAPI::_API->GetInputCount(2))
                            ? InputManagerAPI::_API->GetInputName(2, gestIdx) : GetLoc("input.no_gesture", "[ No Gesture ]");

                        if (ImGui::BeginCombo(GetLoc("input.pad_gesture", "Pad Gesture"), gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < InputManagerAPI::_API->GetInputCount(2); ++gIdx) {
                                if (ImGui::Selectable(InputManagerAPI::_API->GetInputName(2, (int)gIdx), gestIdx == (int)gIdx)) {
                                    state.edit_info.gamepadModifierKey = (int)gIdx;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else if (state.edit_info.gamepadModAction != 0) {
                        if (SearchableCombo(GetLoc("input.pad_mod_key", "Pad Mod Key"), &state.ui_padModIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                            state.edit_info.gamepadModifierKey = gamepadKeyIDs[state.ui_padModIdx];
                        }

                        if (state.edit_info.gamepadModAction == 1) {
                            if (state.edit_info.gamepadModTapCount < 1) state.edit_info.gamepadModTapCount = 1;
                            ImGui::SliderInt(GetLoc("input.pad_mod_tap", "Pad Mod Tap Amount"), &state.edit_info.gamepadModTapCount, 1, 5);
                        }
                    }

                    ImGui::Spacing();
                    if (ImGui::Button(GetLoc("input.update_btn", "Update Mapping and Save"))) {
                        state.edit_info.name = state.edit_nameBuf;
                        state.edit_info.useCustomTimings = false;

                        bool success = InputManagerAPI::_API->UpdateActionMapping(actionID, state.edit_info);
                        if (success) {
                            state.updateStatusMsg = GetLoc("input.update_success", "Mapping updated successfully.");
                            state.updateSuccess = true;
                        }
                        else {
                            state.updateStatusMsg = GetLoc("input.update_error", "ERROR! Duplicate name or Combo already registered.");
                            state.updateSuccess = false;
                        }
                    }

                    if (!state.updateStatusMsg.empty()) {
                        ImGui::TextColored(state.updateSuccess ? ImGui::ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImGui::ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", state.updateStatusMsg.c_str());
                    }
                    ImGui::TreePop();
                }
                else {
                    // Reseta o status de edição quando a sub-árvore do menu fecha
                    state.current_edit_action_id = -1;
                }
            }
        }
        // --- 3. Lógica para Motion (Tipo 1) ---
        else if (inputType == 1) {
            size_t motionCount = InputManagerAPI::_API->GetInputCount(1);
            std::string previewValue = GetLoc("input.no_motion", "[Nenhum Motion Selecionado]");

            if (motionID >= 0 && motionID < motionCount) {
                auto info = InputManagerAPI::_API->GetMotionInfo(motionID);
                previewValue = "[" + std::to_string(motionID) + "] " + (info.name ? std::string(info.name) : GetLoc("common.unnamed", "Unnamed"));
            }

            ImGui::SetNextItemWidth(250);
            if (ImGui::BeginCombo((std::string(GetLoc("input.select_motion", "Select Motion")) + "##" + label).c_str(), previewValue.c_str())) {
                if (ImGui::Selectable(GetLoc("common.disabled", "[Desativado]"), motionID == -1)) {
                    if (motionID != -1) InputManagerAPI::_API->UpdateListener(1, motionID, "BFCO", purpose, false);
                    motionID = -1;
                    changed = true;
                }
                for (int i = 0; i < motionCount; ++i) {
                    auto info = InputManagerAPI::_API->GetMotionInfo(i);
                    std::string itemLabel = "[" + std::to_string(i) + "] " + (info.name ? std::string(info.name) : GetLoc("common.unnamed", "Unnamed"));
                    if (ImGui::Selectable(itemLabel.c_str(), motionID == i)) {
                        if (motionID != -1) InputManagerAPI::_API->UpdateListener(1, motionID, "BFCO", purpose, false);
                        motionID = i;
                        InputManagerAPI::_API->UpdateListener(1, motionID, "BFCO", purpose, true);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        return changed;
    }

    // =========================================================================
    // MAIN RENDER LOOP
    // =========================================================================
    void __stdcall Render() {
        bool settings_changed = false;

        ImGui::Text("%s", GetLoc("menu.title", "Combat settings BFCO"));
        ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox(GetLoc("menu.enable_combo", "Enable combo attack"), &Settings::bEnableComboAttack)) settings_changed = true;
        if (Settings::bEnableComboAttack) {
            if (RenderInputSelector(GetLoc("menu.combo_config", "Combo Attack Config"), "Combo Attack", Settings::ComboInputType, Settings::ComboActionID, Settings::ComboMotionID)) {
                settings_changed = true;
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox(GetLoc("menu.enable_dir_power", "Enable direcional power attack"), &Settings::bEnableDirectionalAttack)) settings_changed = true;
        if (ImGui::Checkbox(GetLoc("menu.enable_power_key", "Enable power attack key"), &Settings::bEnablePowerAttack)) settings_changed = true;

        if (Settings::bEnablePowerAttack) {
            if (RenderInputSelector(GetLoc("menu.power_config", "Power Attack Config"), "Power Attack", Settings::PowerAttackInputType, Settings::PowerAttackActionID, Settings::PowerAttackMotionID)) {
                settings_changed = true;
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox(GetLoc("menu.instant_block", "Instant Block (Cancel attacks to block)"), &Settings::bInstantBlock)) settings_changed = true;

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::Text("%s", GetLoc("menu.anim_type", "Animation Type"));

        const char* animItems[] = {
            GetLoc("menu.anim_bfco_all", "BFCO All Attacks"),
            GetLoc("menu.anim_vanilla_light", "Vanilla Light Attacks")
        };
        const char* animTooltips[] = {
            GetLoc("menu.tt_anim_bfco", "All attacks use BFCO animations"),
            GetLoc("menu.tt_anim_vanilla", "Player light attacks use vanilla animations")
        };

        const char* current_animItem = (Settings::AnimationType >= 0 && Settings::AnimationType < 2) ? animItems[Settings::AnimationType] : animItems[0];

        ImGui::SetNextItemWidth(250);
        if (ImGui::BeginCombo("##AnimationType", current_animItem)) {
            for (int n = 0; n < 2; n++) {
                bool is_selected = (Settings::AnimationType == n);
                if (ImGui::Selectable(animItems[n], is_selected)) {
                    Settings::AnimationType = n;
                    settings_changed = true;
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", animTooltips[n]);
                }

                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();


        ImGui::Text("%s", GetLoc("menu.light_attack_mode", "Light Attack Mode"));
        const char* lmbItems[] = {
            GetLoc("menu.la_vanilla", "Vanilla"),
            GetLoc("menu.la_modern", "Modern"),
            GetLoc("menu.la_modern_auto", "Modern + AutoNA")
        };
        const char* lmbTooltips[] = {
            GetLoc("menu.tt_la_vanilla", "Tap = Light, Hold = Power"),
            GetLoc("menu.tt_la_modern", "LMB = Light only"),
            GetLoc("menu.tt_la_modern_auto", "Hold = Auto Light")
        };
        const char* current_lmbItem = (Settings::bPowerAttackLMB >= 0 && Settings::bPowerAttackLMB < 3) ? lmbItems[Settings::bPowerAttackLMB] : lmbItems[0];

        ImGui::SetNextItemWidth(250);
        if (ImGui::BeginCombo("##LightAttackMode", current_lmbItem)) {
            for (int n = 0; n < 3; n++) {
                bool is_selected = (Settings::bPowerAttackLMB == n);
                if (ImGui::Selectable(lmbItems[n], is_selected)) {
                    Settings::bPowerAttackLMB = n;
                    settings_changed = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", lmbTooltips[n]);
                }

                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing(); ImGui::Separator();
        if (ImGui::Checkbox(GetLoc("menu.disable_jump", "Disable jump attack"), &Settings::bDisableJumpingAttack)) settings_changed = true;

        if (settings_changed) {
            SaveSettings();
        }
    }

    void SaveSettings() {
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        doc.AddMember("bEnableComboAttack", Settings::bEnableComboAttack, allocator);
        doc.AddMember("bEnableDirectionalAttack", Settings::bEnableDirectionalAttack, allocator);
        doc.AddMember("bEnablePowerAttack", Settings::bEnablePowerAttack, allocator);
        doc.AddMember("bDisableJumpingAttack", Settings::bDisableJumpingAttack, allocator);
        doc.AddMember("bPowerAttackLMB", Settings::bPowerAttackLMB, allocator);
        doc.AddMember("bInstantBlock", Settings::bInstantBlock, allocator);
        doc.AddMember("AnimationType", Settings::AnimationType, allocator);

        // Inputs
        doc.AddMember("ComboInputType", Settings::ComboInputType, allocator);
        doc.AddMember("ComboActionID", Settings::ComboActionID, allocator);
        doc.AddMember("ComboMotionID", Settings::ComboMotionID, allocator);

        doc.AddMember("PowerAttackInputType", Settings::PowerAttackInputType, allocator);
        doc.AddMember("PowerAttackActionID", Settings::PowerAttackActionID, allocator);
        doc.AddMember("PowerAttackMotionID", Settings::PowerAttackMotionID, allocator);

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
                if (doc.HasMember("bEnableDirectionalAttack")) Settings::bEnableDirectionalAttack = doc["bEnableDirectionalAttack"].GetBool();
                if (doc.HasMember("bEnablePowerAttack")) Settings::bEnablePowerAttack = doc["bEnablePowerAttack"].GetBool();
                if (doc.HasMember("bDisableJumpingAttack")) Settings::bDisableJumpingAttack = doc["bDisableJumpingAttack"].GetBool();
                if (doc.HasMember("bPowerAttackLMB")) Settings::bPowerAttackLMB = doc["bPowerAttackLMB"].GetInt();

                if (doc.HasMember("bInstantBlock")) Settings::bInstantBlock = doc["bInstantBlock"].GetBool();
                if (doc.HasMember("AnimationType")) Settings::AnimationType = doc["AnimationType"].GetInt();

                // Inputs
                if (doc.HasMember("ComboInputType")) Settings::ComboInputType = doc["ComboInputType"].GetInt();
                if (doc.HasMember("ComboActionID")) Settings::ComboActionID = doc["ComboActionID"].GetInt();
                if (doc.HasMember("ComboMotionID")) Settings::ComboMotionID = doc["ComboMotionID"].GetInt();

                if (doc.HasMember("PowerAttackInputType")) Settings::PowerAttackInputType = doc["PowerAttackInputType"].GetInt();
                if (doc.HasMember("PowerAttackActionID")) Settings::PowerAttackActionID = doc["PowerAttackActionID"].GetInt();
                if (doc.HasMember("PowerAttackMotionID")) Settings::PowerAttackMotionID = doc["PowerAttackMotionID"].GetInt();
            }
        }
        UpdateGameGlobals();
    }

    // Função que aplica as configurações às Globals do jogo
    void UpdateGameGlobals() {
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SKSE::log::error("Falha ao obter TESDataHandler para atualizar Globals.");
            return;
        }
        const std::string bfco = "SCSI-ACTbfco-Main.esp";
		auto player = RE::PlayerCharacter::GetSingleton();
        // Mapeia nossas variáveis C++ para os EditorIDs das Globals no .esp
        std::map<const char*, float> globalsToUpdate = {
            {"bfcoTG_KeyAttackComb", Settings::bEnableComboAttack ? 1.0f : 0.0f},
            {"bfcoINT_KeyAttackComb", Settings::bEnableComboAttack ? 2.0f : 0.0f},
            {"bfcoTG_JumpAttack", Settings::bDisableJumpingAttack ? 0.0f : 1.0f},
            {"bfcoTG_DirPowerAttack", Settings::bEnableDirectionalAttack ? 1.0f : 0.0f},
            {"bfcoTG_InputType", static_cast<float>(Settings::bPowerAttackLMB)},            
        };
        player->SetGraphVariableInt("BFCO_VanillaAnimationType", Settings::AnimationType);
		logger::info("Player Graph Variable 'BFCO_VanillaAnimationType' set to: {}", Settings::AnimationType);
		player->SetGraphVariableBool("BFCO_InstantBlock", Settings::bInstantBlock);
		logger::info("Player Graph Variable 'BFCO_InstantBlock' set to: {}", Settings::bInstantBlock);
        for (auto const& [editorID, value] : globalsToUpdate) {
            RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
            if (global) {
                global->value = value;
                SKSE::log::info("Global '{}' atualizada para o valor: {}", editorID, value);
            }
            else {
                SKSE::log::warn("Nao foi possivel encontrar a GlobalVariable: {}", editorID);
            }
        }

        auto iniCollection = RE::INISettingCollection::GetSingleton();
        if (iniCollection) {
            RE::Setting* setting = iniCollection->GetSetting("fSubsequentPowerAttackDelay:Controls");
            if (setting) {
                float delayValue = (Settings::bPowerAttackLMB == 2) ? 0.3f : 2.0f;
                setting->data.f = delayValue;
                SKSE::log::info("INI 'fSubsequentPowerAttackDelay:Controls' alterado para: {}", delayValue);
            }
            else {
                SKSE::log::warn("Aviso: Configuração INI 'fSubsequentPowerAttackDelay:Controls' nao encontrada.");
            }
        }
    }
    // Registra o menu
    void Register() {
        if (SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("SKSE Menu Framework encontrado. Registrando o menu BFCO.");
            LoadLanguage();
            LoadSettings();
            SKSEMenuFramework::SetSection("BFCO");
            SKSEMenuFramework::AddSectionItem("Settings", Render);
        }
        else {
            SKSE::log::warn("SKSE Menu Framework nao encontrado. O menu BFCO nao sera registrado.");
        }
    }
}