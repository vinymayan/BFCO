#include "Settings.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include "Manager.h"


namespace ImGui = ImGuiMCP;

namespace BFCOMenu {

    const char* SETTINGS_PATH = "Data/SKSE/Plugins/BFCO_Settings.json";
    const char* LANG_PATH = "Data/SKSE/Plugins/BFCO_Language.json";
    static std::unordered_map<std::string, std::string> LangMap;

    void LoadLanguage() {
        LangMap.clear();
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::warn("Nao foi possivel carregar BFCO_Language.json. Usando textos padroes.");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        if (jsonStr.size() >= 3 &&
            (unsigned char)jsonStr[0] == 0xEF &&
            (unsigned char)jsonStr[1] == 0xBB &&
            (unsigned char)jsonStr[2] == 0xBF) {
            jsonStr.erase(0, 3);
        }

        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

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

        if (ImGui::BeginCombo(label, preview_value)) {
            static char searchBuf[128] = "";
            if (ImGui::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGui::SetKeyboardFocusHere();
            }
            std::string searchLabel = std::string(GetLoc("common.search_placeholder", "Filter...")) + "##Search";
            ImGui::InputText(searchLabel.c_str(), searchBuf, sizeof(searchBuf));
            ImGui::Separator();

            std::string searchLower = ToLower(searchBuf);

            for (int i = 0; i < items_count; i++) {
                if (searchLower.empty() || ToLower(items[i]).find(searchLower) != std::string::npos) {
                    bool is_selected = (*current_item == i);
                    if (ImGui::Selectable(items[i], is_selected)) {
                        *current_item = i;
                        changed = true;
                    }
                    if (is_selected && ImGui::IsWindowAppearing()) {
                        ImGui::SetScrollHereY();
                    }
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // ==========================================
    // UI DROPDOWN (Adaptado para RE::FormID)
    // ==========================================
    bool DrawDropdown(const char* label, const std::string& category, RE::FormID& current_form_id, float customWidth = -1.0f) {
        bool changed = false;
        const auto& fullList = Manager::GetSingleton()->GetList(category);
        if (fullList.empty()) return false;

        std::vector<const char*> comboItems;
        std::vector<int> mapToFull;

        comboItems.push_back(GetLoc("text.none", "None"));
        mapToFull.push_back(-1);

        int localSelection = 0;
        for (size_t i = 0; i < fullList.size(); ++i) {
            comboItems.push_back(fullList[i].cachedDisplayName.c_str());
            mapToFull.push_back(static_cast<int>(i));
            if (fullList[i].formID == current_form_id) {
                localSelection = static_cast<int>(i) + 1; // +1 porque o 0 é o "None"
            }
        }

        ImGui::PushID(label);
        std::string displayLabel = label;
        size_t hashPos = displayLabel.find("##");
        if (hashPos != std::string::npos) displayLabel = displayLabel.substr(0, hashPos);

        ImGui::Text("%s:", displayLabel.c_str());
        ImGui::SameLine();

        if (customWidth > 0.0f) ImGui::SetNextItemWidth(customWidth);
        const char* previewValue = comboItems[localSelection];

        if (ImGui::BeginCombo("##drop", previewValue)) {
            static std::map<std::string, std::string> searchBuffers;
            char searchBuf[256] = "";
            if (searchBuffers.contains(label)) strcpy_s(searchBuf, searchBuffers[label].c_str());

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##busca", searchBuf, sizeof(searchBuf))) {
                searchBuffers[label] = searchBuf;
            }
            ImGui::Separator();

            std::string searchStr = searchBuf;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), [](unsigned char c) { return std::tolower(c); });

            ImGui::BeginChild("##scroll", ImGui::ImVec2(0, 200), false);
            for (int i = 0; i < comboItems.size(); i++) {
                std::string itemStr = comboItems[i];
                std::string itemLower = itemStr;
                std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), [](unsigned char c) { return std::tolower(c); });

                if (searchStr.empty() || itemLower.find(searchStr) != std::string::npos) {
                    bool isSelected = (localSelection == i);
                    if (ImGui::Selectable(comboItems[i], isSelected)) {
                        localSelection = i;
                        int originalIndex = mapToFull[localSelection];

                        if (originalIndex == -1) current_form_id = 0;
                        else current_form_id = fullList[originalIndex].formID;

                        searchBuffers[label] = "";
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndChild();
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return changed;
    }


    // Helpers para Leitura e Escrita de Vetores Dinâmicos em JSON
    void ReadJSON(rapidjson::Document& doc, const char* nome, std::vector<int>& lista) {
        if (doc.HasMember(nome) && doc[nome].IsArray()) {
            lista.clear();
            for (auto& v : doc[nome].GetArray()) {
                if (v.IsInt()) lista.push_back(v.GetInt());
            }
        }
    }

    void WriteJSON(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* nome, const std::vector<int>& lista) {
        rapidjson::Value array(rapidjson::kArrayType);
        for (int id : lista) {
            array.PushBack(id, alloc);
        }
        rapidjson::Value chave;
        chave.SetString(nome, alloc);
        doc.AddMember(chave, array, alloc);
    }

    // Gerenciador de Input de Múltiplos Vetores (Actions e Motions)
    void EditInputVectors(const char* label, const std::string& actionIdStr, std::vector<int>& actionsRef, std::vector<int>& motionsRef) {
        ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", label);

        if (!InputManagerAPI::_API) {
            ImGui::TextDisabled("[Input Manager not found in memory]");
            return;
        }

        bool changed = false;
        static int editingActionId = -1;
        static InputManagerAPI::ActionInfo editStagingInfo{};
        static bool showEditError = false;

        bool openEditPopup = false;
        std::string editPopupId = "EditActionPopup_" + actionIdStr;

        auto DrawSelectedList = [&](std::vector<int>& list, int type, const char* typeName) {
            for (size_t i = 0; i < list.size(); i++) {
                ImGui::PushID((std::string(typeName) + "_" + std::to_string(i) + actionIdStr).c_str());

                const char* name = InputManagerAPI::_API->GetInputName(type, list[i]);
                std::string displayName = std::string("[") + typeName + "] [" + std::to_string(list[i]) + "] " + (name ? name : "Unnamed");

                ImGui::Text("%s", displayName.c_str());

                if (type == 0) {
                    ImGui::SameLine();
                    if (ImGui::Button(GetLoc("common.edit", "Edit"))) {
                        editingActionId = list[i];
                        editStagingInfo = InputManagerAPI::_API->GetActionInfo(editingActionId);
                        showEditError = false;
                        openEditPopup = true;
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    list.erase(list.begin() + i);
                    changed = true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            };

        DrawSelectedList(actionsRef, 0, "Action");
        DrawSelectedList(motionsRef, 1, "Motion");

        if (openEditPopup) ImGui::OpenPopup(editPopupId.c_str());

        // POPUP EDIÇÃO DE AÇÃO
        if (ImGui::BeginPopup(editPopupId.c_str())) {
            if (editingActionId != -1 && editStagingInfo.isValid) {
                ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s: %s", GetLoc("menu.editing_action", "Editing Action"), editStagingInfo.name ? editStagingInfo.name : "Unnamed");
                ImGui::Separator();

                int pcKeySize = sizeof(pcKeyIDs) / sizeof(pcKeyIDs[0]);
                int padKeySize = sizeof(gamepadKeyIDs) / sizeof(gamepadKeyIDs[0]);

                auto DrawMainActionCombo = [](const char* label, int& current_action) {
                    if (ImGui::BeginCombo(label, actionStateNames[current_action])) {
                        for (int n = 0; n < 5; n++) {
                            if (n == 3) continue;
                            bool is_selected = (current_action == n);
                            if (ImGui::Selectable(actionStateNames[n], is_selected)) current_action = n;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    };

                auto DrawModActionCombo = [](const char* label, int& current_action, int main_action) {
                    if (ImGui::BeginCombo(label, actionStateNames[current_action])) {
                        for (int n = 0; n < 5; n++) {
                            if (n == 3 && main_action != 2 && main_action != 4) continue;
                            bool is_selected = (current_action == n);
                            if (ImGui::Selectable(actionStateNames[n], is_selected)) current_action = n;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    };

                auto DrawGestureCombo = [](const char* label, uint32_t& current_gesture) {
                    size_t gestCount = InputManagerAPI::_API->GetInputCount(2);
                    int current = static_cast<int>(current_gesture);
                    const char* preview = (current >= 0 && current < gestCount) ? InputManagerAPI::_API->GetInputName(2, current) : "None";

                    if (ImGui::BeginCombo(label, preview)) {
                        for (int i = 0; i < gestCount; ++i) {
                            bool is_selected = (current == i);
                            if (ImGui::Selectable(InputManagerAPI::_API->GetInputName(2, i), is_selected)) {
                                current_gesture = static_cast<uint32_t>(i);
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    };

                auto DrawStickCombo = [](const char* label, int& current_stick) {
                    const char* sticks[] = { "Left Stick", "Right Stick" };
                    const char* preview = (current_stick >= 0 && current_stick < 2) ? sticks[current_stick] : "Unknown";
                    if (ImGui::BeginCombo(label, preview)) {
                        for (int i = 0; i < 2; i++) {
                            bool is_selected = (current_stick == i);
                            if (ImGui::Selectable(sticks[i], is_selected)) current_stick = i;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    };

                // PC
                ImGui::TextColored({ 0.7f, 0.7f, 1.0f, 1.0f }, "--- PC Settings ---");
                int pcMainIdx = GetIndexFromID(editStagingInfo.pcMainKey, pcKeyIDs, pcKeySize);
                if (SearchableCombo("PC Main Key", &pcMainIdx, pcKeyNames, pcKeySize)) editStagingInfo.pcMainKey = pcKeyIDs[pcMainIdx];
                DrawMainActionCombo("PC Main Action", editStagingInfo.pcMainAction);
                if (editStagingInfo.pcMainAction == 1) ImGui::InputInt("PC Main Taps", &editStagingInfo.pcMainTapCount);

                if (editStagingInfo.pcModAction == 3 && editStagingInfo.pcMainAction != 2 && editStagingInfo.pcMainAction != 4) editStagingInfo.pcModAction = 0;

                DrawModActionCombo("PC Mod Action", editStagingInfo.pcModAction, editStagingInfo.pcMainAction);
                if (editStagingInfo.pcModAction == 3) {
                    DrawGestureCombo("PC Gesture", editStagingInfo.pcModifierKey);
                }
                else {
                    int pcModIdx = GetIndexFromID(editStagingInfo.pcModifierKey, pcKeyIDs, pcKeySize);
                    if (SearchableCombo("PC Mod Key", &pcModIdx, pcKeyNames, pcKeySize)) editStagingInfo.pcModifierKey = pcKeyIDs[pcModIdx];
                    if (editStagingInfo.pcModAction == 1) ImGui::InputInt("PC Mod Taps", &editStagingInfo.pcModTapCount);
                }

                // GAMEPAD
                ImGui::Spacing();
                ImGui::TextColored({ 0.7f, 1.0f, 0.7f, 1.0f }, "--- Gamepad Settings ---");
                int padMainIdx = GetIndexFromID(editStagingInfo.gamepadMainKey, gamepadKeyIDs, padKeySize);
                if (SearchableCombo("Pad Main Key", &padMainIdx, gamepadKeyNames, padKeySize)) editStagingInfo.gamepadMainKey = gamepadKeyIDs[padMainIdx];
                DrawMainActionCombo("Pad Main Action", editStagingInfo.gamepadMainAction);
                if (editStagingInfo.gamepadMainAction == 1) ImGui::InputInt("Pad Main Taps", &editStagingInfo.gamepadMainTapCount);

                if (editStagingInfo.gamepadModAction == 3 && editStagingInfo.gamepadMainAction != 2 && editStagingInfo.gamepadMainAction != 4) editStagingInfo.gamepadModAction = 0;

                DrawModActionCombo("Pad Mod Action", editStagingInfo.gamepadModAction, editStagingInfo.gamepadMainAction);
                if (editStagingInfo.gamepadModAction == 3) {
                    DrawGestureCombo("Pad Gesture", editStagingInfo.gamepadModifierKey);
                    DrawStickCombo("Gesture Stick", editStagingInfo.gamepadGestureStick);
                }
                else {
                    int padModIdx = GetIndexFromID(editStagingInfo.gamepadModifierKey, gamepadKeyIDs, padKeySize);
                    if (SearchableCombo("Pad Mod Key", &padModIdx, gamepadKeyNames, padKeySize)) editStagingInfo.gamepadModifierKey = gamepadKeyIDs[padModIdx];
                    if (editStagingInfo.gamepadModAction == 1) ImGui::InputInt("Pad Mod Taps", &editStagingInfo.gamepadModTapCount);
                }

                ImGui::Separator();
                if (showEditError) {
                    ImGui::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "%s", GetLoc("menu.save_error", "Error: Conflict detected or invalid input!"));
                }

                if (ImGui::Button(GetLoc("common.save", "Save"), { 120, 0 })) {
                    bool success = InputManagerAPI::_API->UpdateActionMapping(editingActionId, editStagingInfo);
                    if (success) {
                        ImGui::CloseCurrentPopup();
                        editingActionId = -1;
                    }
                    else {
                        showEditError = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(GetLoc("common.cancel", "Cancel"), { 120, 0 })) {
                    ImGui::CloseCurrentPopup();
                    editingActionId = -1;
                }
            }
            ImGui::EndPopup();
        }

        std::string popupId = "AddInputPopup_" + actionIdStr;
        if (ImGui::Button(("+ Add Input##" + actionIdStr).c_str())) {
            ImGui::OpenPopup(popupId.c_str());
        }

        // POPUP ADD INPUT
        if (ImGui::BeginPopup(popupId.c_str())) {
            static char searchBuf[128] = "";
            if (ImGui::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::BeginTabBar(("InputTabs_" + actionIdStr).c_str())) {
                int selectedType = -1;
                if (ImGui::BeginTabItem(GetLoc("common.actions", "Actions"))) {
                    selectedType = 0;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(GetLoc("common.motions", "Motions"))) {
                    selectedType = 1;
                    ImGui::EndTabItem();
                }

                if (selectedType != -1) {
                    std::string searchLabel = std::string(GetLoc("common.search_placeholder", "Filter...")) + "##SearchInput";
                    ImGui::InputText(searchLabel.c_str(), searchBuf, sizeof(searchBuf));
                    ImGui::Separator();

                    std::string searchLower = ToLower(searchBuf);

                    ImGui::BeginChild(("ChildList_" + actionIdStr).c_str(), { 300, 200 }, true);
                    size_t count = InputManagerAPI::_API->GetInputCount(selectedType);

                    for (int i = 0; i < count; i++) {
                        const char* name = InputManagerAPI::_API->GetInputName(selectedType, i);
                        std::string itemLabel = "[" + std::to_string(i) + "] " + (name ? name : "Unnamed");

                        bool matches = searchLower.empty();
                        if (!matches) matches = (ToLower(itemLabel).find(searchLower) != std::string::npos);

                        if (matches) {
                            if (ImGui::Selectable(itemLabel.c_str(), false)) {
                                if (selectedType == 0) {
                                    if (std::find(actionsRef.begin(), actionsRef.end(), i) == actionsRef.end()) {
                                        actionsRef.push_back(i);
                                        changed = true;
                                    }
                                }
                                else {
                                    if (std::find(motionsRef.begin(), motionsRef.end(), i) == motionsRef.end()) {
                                        motionsRef.push_back(i);
                                        changed = true;
                                    }
                                }
                                ImGui::CloseCurrentPopup();
                            }

                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.input_details", "Input Details"));
                                ImGui::Separator();

                                const char* idLoc = GetLoc("common.id", "ID");
                                const char* nameLoc = GetLoc("common.name", "Name");
                                const char* unnamedLoc = GetLoc("common.unnamed", "Unnamed");
                                const char* actionLoc = GetLoc("common.action", "Action");
                                const char* noInfoLoc = GetLoc("menu.no_info", "No information available.");

                                auto getActionName = [](int actionId) -> const char* {
                                    if (actionId >= 0 && actionId < 5) return actionStateNames[actionId];
                                    return "Unknown";
                                    };

                                auto formatAction = [&](int actionId, int tapCount) -> std::string {
                                    std::string n = getActionName(actionId);
                                    if (actionId == 1) { // 1 = Tap
                                        n += " x" + std::to_string(tapCount);
                                    }
                                    return n;
                                    };

                                auto getPcKeyName = [](int keyId) -> const char* {
                                    int size = sizeof(pcKeyIDs) / sizeof(pcKeyIDs[0]);
                                    int idx = GetIndexFromID(keyId, pcKeyIDs, size);
                                    return pcKeyNames[idx];
                                    };

                                auto getPadKeyName = [](int keyId) -> const char* {
                                    int size = sizeof(gamepadKeyIDs) / sizeof(gamepadKeyIDs[0]);
                                    int idx = GetIndexFromID(keyId, gamepadKeyIDs, size);
                                    return gamepadKeyNames[idx];
                                    };

                                if (selectedType == 0) { // Action
                                    auto info = InputManagerAPI::_API->GetActionInfo(i);

                                    if (info.isValid) {
                                        ImGui::Text("%s: %d | %s: %s", idLoc, info.id, nameLoc, info.name ? info.name : unnamedLoc);

                                        ImGui::Text("%s: %s (%s: %s)", GetLoc("menu.pc_main_key", "PC Main Key"),
                                            getPcKeyName(info.pcMainKey), actionLoc, formatAction(info.pcMainAction, info.pcMainTapCount).c_str());

                                        if (info.pcModifierKey != 0) {
                                            ImGui::Text("%s: %s (%s: %s)", GetLoc("menu.pc_mod_key", "PC Mod Key"),
                                                (info.pcModAction == 3) ? InputManagerAPI::_API->GetInputName(2, info.pcModifierKey) : getPcKeyName(info.pcModifierKey),
                                                actionLoc, formatAction(info.pcModAction, info.pcModTapCount).c_str());
                                        }

                                        ImGui::Text("%s: %s (%s: %s)", GetLoc("menu.pad_main_key", "Gamepad Main Key"),
                                            getPadKeyName(info.gamepadMainKey), actionLoc, formatAction(info.gamepadMainAction, info.gamepadMainTapCount).c_str());

                                        if (info.gamepadModifierKey != 0) {
                                            ImGui::Text("%s: %s (%s: %s)", GetLoc("menu.pad_mod_key", "Gamepad Mod Key"),
                                                (info.gamepadModAction == 3) ? InputManagerAPI::_API->GetInputName(2, info.gamepadModifierKey) : getPadKeyName(info.gamepadModifierKey),
                                                actionLoc, formatAction(info.gamepadModAction, info.gamepadModTapCount).c_str());
                                        }

                                        if (info.useCustomTimings) {
                                            ImGui::TextColored({ 0.8f, 0.8f, 0.4f, 1.0f }, "%s - %s: %.2fs | %s: %.2fs",
                                                GetLoc("menu.custom_timings", "Custom Timings"),
                                                GetLoc("menu.tap_window", "Tap Window"), info.tapWindow,
                                                GetLoc("menu.hold", "Hold"), info.holdDuration);
                                        }
                                    }
                                    else {
                                        ImGui::TextDisabled("%s", noInfoLoc);
                                    }
                                }
                                else { // Motion
                                    auto info = InputManagerAPI::_API->GetMotionInfo(i);

                                    if (info.isValid) {
                                        ImGui::Text("%s: %d | %s: %s", idLoc, info.id, nameLoc, info.name ? info.name : unnamedLoc);
                                        ImGui::Text("%s: %.2fs", GetLoc("menu.time_window", "Time Window"), info.timeWindow);

                                        std::string pcSeq = "";
                                        for (int k = 0; k < info.pcSequenceLength; k++) {
                                            if (k > 0) pcSeq += ", ";
                                            pcSeq += getPcKeyName(info.pcSequence[k]);
                                        }
                                        ImGui::Text("%s: %d [%s]", GetLoc("menu.pc_seq_size", "PC Sequence"), info.pcSequenceLength, pcSeq.c_str());

                                        std::string padSeq = "";
                                        for (int k = 0; k < info.padSequenceLength; k++) {
                                            if (k > 0) padSeq += ", ";
                                            padSeq += getPadKeyName(info.padSequence[k]);
                                        }
                                        ImGui::Text("%s: %d [%s]", GetLoc("menu.pad_seq_size", "Gamepad Sequence"), info.padSequenceLength, padSeq.c_str());
                                    }
                                    else {
                                        ImGui::TextDisabled("%s", noInfoLoc);
                                    }
                                }
                                ImGui::EndTooltip();
                            }
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndPopup();
        }

        if (changed) {
            UnregisterInputCategory(actionIdStr);
            RegisterAllInputs();
            SaveSettings();
            TweenPauseRegister();
        }
    }

    // Loop Principal do Menu ImGui
    void __stdcall Render() {
        bool settings_changed = false;
        ImGui::Text("%s", GetLoc("menu.title", "Combat settings BFCO"));
        ImGui::Separator(); ImGui::Spacing();

        // 1. Combo Attack
        if (ImGui::Checkbox(GetLoc("menu.enable_combo", "Enable combo attack"), &Settings::bEnableComboAttack)) settings_changed = true;
        if (Settings::bEnableComboAttack) {
            if (ImGui::Checkbox(GetLoc("menu.lock_combo_perk", "Lock Behind Perk##Combo"), &Settings::bLockComboPerk)) settings_changed = true;
            if (Settings::bLockComboPerk) {
                if (DrawDropdown(GetLoc("menu.combo_perk_label", "Combo Perk##ComboDrop"), "Perk", Settings::comboPerkID, 300.0f)) settings_changed = true;
            }
            EditInputVectors(GetLoc("menu.combo_config", "Combo Attack Config"), "ComboAttack", Settings::ComboActionIDs, Settings::ComboMotionIDs);
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // 2. Power Attack
        if (ImGui::Checkbox(GetLoc("menu.enable_dir_power", "Enable direcional power attack"), &Settings::bEnableDirectionalAttack)) settings_changed = true;
        if (ImGui::Checkbox(GetLoc("menu.enable_power_key", "Enable power attack key"), &Settings::bEnablePowerAttack)) settings_changed = true;
        if (Settings::bEnablePowerAttack) {
            if (ImGui::Checkbox(GetLoc("menu.disable_dir_power_custom", "Disable directional power attack in PA custom input"), &Settings::bDisableDirPowerCustomInput)) settings_changed = true;
            if (ImGui::Checkbox(GetLoc("menu.lock_power_perk", "Lock Behind Perk##Power"), &Settings::bLockPowerPerk)) settings_changed = true;
            if (Settings::bLockPowerPerk) {
                if (DrawDropdown(GetLoc("menu.power_perk_label", "Power Perk##PowerDrop"), "Perk", Settings::powerPerkID, 300.0f)) settings_changed = true;
            }
            EditInputVectors(GetLoc("menu.power_config", "Power Attack Config"), "PowerAttack", Settings::PowerAttackActionIDs, Settings::PowerAttackMotionIDs);
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // 3. Special Attack
        if (ImGui::Checkbox(GetLoc("menu.enable_special", "Enable Special Attack"), &Settings::bEnableSpecialAttack)) settings_changed = true;
        if (ImGui::Checkbox(GetLoc("menu.disable_vanilla_special", "Disable Vanilla Special Attack"), &Settings::bDisableSpecialAttack)) settings_changed = true;
        if (Settings::bEnableSpecialAttack) {
            if (ImGui::Checkbox(GetLoc("menu.lock_special_perk", "Lock Behind Perk##Special"), &Settings::bLockSpecialPerk)) settings_changed = true;
            if (Settings::bLockSpecialPerk) {
                if (DrawDropdown(GetLoc("menu.special_perk_label", "Special Perk##SpecialDrop"), "Perk", Settings::specialPerkID, 300.0f)) settings_changed = true;
            }
            EditInputVectors(GetLoc("menu.special_config", "Special Attack Config"), "SpecialAttack", Settings::SpecialAttackActionIDs, Settings::SpecialAttackMotionIDs);
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // 4. Special Power Attack
        if (ImGui::Checkbox(GetLoc("menu.enable_special_power", "Enable Special Power Attack"), &Settings::bEnableSpecialPowerAttack)) settings_changed = true;
        if (ImGui::Checkbox(GetLoc("menu.disable_power_special", "Disable Vanilla Power Special Attack"), &Settings::bDisablePowerSpecialAttack)) settings_changed = true;
        if (Settings::bEnableSpecialPowerAttack) {
            if (ImGui::Checkbox(GetLoc("menu.lock_power_special_perk", "Lock Behind Perk##PowerSpecial"), &Settings::bLockPowerSpecialPerk)) settings_changed = true;
            if (Settings::bLockPowerSpecialPerk) {
                if (DrawDropdown(GetLoc("menu.power_special_perk_label", "Power Special Perk##PowerSpecialDrop"), "Perk", Settings::powerSpecialPerkID, 300.0f)) settings_changed = true;
            }
            EditInputVectors(GetLoc("menu.special_power_config", "Special Power Attack Config"), "PowerSpecialAttack", Settings::PowerSpecialAttackActionIDs, Settings::PowerSpecialAttackMotionIDs);
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox(GetLoc("menu.instant_block", "Instant Block (Cancel attacks to block)"), &Settings::bInstantBlock)) settings_changed = true;
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // --- SEÇÃO: ANIMAÇÃO (ANIMATION TYPE) ---
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

        // --- SEÇÃO: MODO DE ATAQUE LEVE (LIGHT ATTACK MODE) ---
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

        // --- SEÇÃO: DESATIVAR ATAQUE PULANDO (DISABLE JUMP ATTACK) ---
        if (ImGui::Checkbox(GetLoc("menu.disable_jump", "Disable jump attack"), &Settings::bDisableJumpingAttack)) settings_changed = true;
        if (settings_changed) SaveSettings();
    }

    // Registro das Categorias e Desvinculação Dinâmica de Inputs
    void UnregisterInputCategory(const std::string& actionId) {
        if (!InputManagerAPI::_API) return;
        if (actionId == "ComboAttack") {
            for (int id : Settings::ComboActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.combo_label", "Combo Attack"), false);
            for (int id : Settings::ComboMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.combo_label", "Combo Attack"), false);
        }
        else if (actionId == "PowerAttack") {
            for (int id : Settings::PowerAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.power_label", "Power Attack"), false);
            for (int id : Settings::PowerAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.power_label", "Power Attack"), false);
        }
        else if (actionId == "SpecialAttack") {
            for (int id : Settings::SpecialAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.special_label", "Special Attack"), false);
            for (int id : Settings::SpecialAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.special_label", "Special Attack"), false);
        }
        else if (actionId == "PowerSpecialAttack") {
            for (int id : Settings::PowerSpecialAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.special_power_label", "Power Special Attack"), false);
            for (int id : Settings::PowerSpecialAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.special_power_label", "Power Special Attack"), false);
        }
    }

    void RegisterAllInputs() {
        if (!InputManagerAPI::_API) return;
        for (int id : Settings::ComboActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.combo_label", "Combo Attack"), true);
        for (int id : Settings::ComboMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.combo_label", "Combo Attack"), true);

        for (int id : Settings::PowerAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.power_label", "Power Attack"), true);
        for (int id : Settings::PowerAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.power_label", "Power Attack"), true);

        for (int id : Settings::SpecialAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.special_label", "Special Attack"), true);
        for (int id : Settings::SpecialAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.special_label", "Special Attack"), true);

        for (int id : Settings::PowerSpecialAttackActionIDs) InputManagerAPI::_API->UpdateListener(0, id, "BFCO", GetLoc("menu.special_power_label", "Power Special Attack"), true);
        for (int id : Settings::PowerSpecialAttackMotionIDs) InputManagerAPI::_API->UpdateListener(1, id, "BFCO", GetLoc("menu.special_power_label", "Power Special Attack"), true);
    }

    // Despacho de Eventos e Payload Estruturado para Integração Direta com o Tween Menu
    void TweenPauseRegister() {
        auto dispatcher = SKSE::GetModCallbackEventSource();
        if (!dispatcher) return;

        auto EnviarPayload = [&](const char* actionIdStr, const char* actionLabelStr, const std::vector<int>& actions, const std::vector<int>& motions) {
            rapidjson::Document doc; doc.SetObject();
            auto& alloc = doc.GetAllocator();

            doc.AddMember("tabId", rapidjson::StringRef("gameplay"), alloc);
            doc.AddMember("tabLabel", rapidjson::StringRef("Mods"), alloc);
            doc.AddMember("categoryId", rapidjson::StringRef("combat"), alloc);
            doc.AddMember("categoryLabel", rapidjson::StringRef("BFCO"), alloc);

            rapidjson::Value actionIdVal; actionIdVal.SetString(actionIdStr, alloc);
            doc.AddMember("actionId", actionIdVal, alloc);
            rapidjson::Value actionLabelVal; actionLabelVal.SetString(actionLabelStr, alloc);
            doc.AddMember("actionLabel", actionLabelVal, alloc);
            doc.AddMember("acceptsMotion", true, alloc);

            rapidjson::Value mappedIds(rapidjson::kArrayType);
            for (int id : actions) {
                rapidjson::Value bind(rapidjson::kObjectType); bind.AddMember("id", id, alloc); bind.AddMember("type", rapidjson::StringRef("action"), alloc); mappedIds.PushBack(bind, alloc);
            }
            for (int id : motions) {
                rapidjson::Value bind(rapidjson::kObjectType); bind.AddMember("id", id, alloc); bind.AddMember("type", rapidjson::StringRef("motion"), alloc); mappedIds.PushBack(bind, alloc);
            }
            doc.AddMember("mappedIds", mappedIds, alloc);

            rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); doc.Accept(writer);
            SKSE::ModCallbackEvent modEvent{ "TweenPause_RegisterControl", RE::BSFixedString(buffer.GetString()), 0.0f, nullptr };
            dispatcher->SendEvent(&modEvent);
            };

        EnviarPayload("ComboAttack", GetLoc("menu.combo_label", "Combo Attack"), Settings::ComboActionIDs, Settings::ComboMotionIDs);
        EnviarPayload("PowerAttack", GetLoc("menu.power_label", "Power Attack"), Settings::PowerAttackActionIDs, Settings::PowerAttackMotionIDs);
        EnviarPayload("SpecialAttack", GetLoc("menu.special_label", "Special Attack"), Settings::SpecialAttackActionIDs, Settings::SpecialAttackMotionIDs);
        EnviarPayload("PowerSpecialAttack", GetLoc("menu.special_power_label", "Special Power Attack"), Settings::PowerSpecialAttackActionIDs, Settings::PowerSpecialAttackMotionIDs);
    }

    void SaveSettings() {
        rapidjson::Document doc; doc.SetObject();
        auto& allocator = doc.GetAllocator();

        doc.AddMember("bEnableComboAttack", Settings::bEnableComboAttack, allocator);
        doc.AddMember("bEnableDirectionalAttack", Settings::bEnableDirectionalAttack, allocator);
        doc.AddMember("bEnablePowerAttack", Settings::bEnablePowerAttack, allocator);
        doc.AddMember("bEnableSpecialAttack", Settings::bEnableSpecialAttack, allocator);
        doc.AddMember("bEnableSpecialPowerAttack", Settings::bEnableSpecialPowerAttack, allocator);
        doc.AddMember("bDisableJumpingAttack", Settings::bDisableJumpingAttack, allocator);
        doc.AddMember("bDisableVanillaSpecialAttack", Settings::bDisableSpecialAttack, allocator);
        doc.AddMember("bDisablePowerSpecialAttack", Settings::bDisablePowerSpecialAttack, allocator);
        doc.AddMember("bPowerAttackLMB", Settings::bPowerAttackLMB, allocator);
        doc.AddMember("bInstantBlock", Settings::bInstantBlock, allocator);
        doc.AddMember("AnimationType", Settings::AnimationType, allocator);

        // Salvando booleanos de travas por perk
        doc.AddMember("bLockComboPerk", Settings::bLockComboPerk, allocator);
        doc.AddMember("bLockPowerPerk", Settings::bLockPowerPerk, allocator);
        doc.AddMember("bLockSpecialPerk", Settings::bLockSpecialPerk, allocator);
        doc.AddMember("bLockPowerSpecialPerk", Settings::bLockPowerSpecialPerk, allocator);
        doc.AddMember("bDisableDirPowerCustomInput", Settings::bDisableDirPowerCustomInput, allocator);

        // Convertendo e salvando FormIDs utilizando FormUtil
        auto formCombo = RE::TESForm::LookupByID(Settings::comboPerkID);
        doc.AddMember("comboPerkID", rapidjson::Value(formCombo ? FormUtil::NormalizeFormID(formCombo).c_str() : "", allocator).Move(), allocator);

        auto formPower = RE::TESForm::LookupByID(Settings::powerPerkID);
        doc.AddMember("powerPerkID", rapidjson::Value(formPower ? FormUtil::NormalizeFormID(formPower).c_str() : "", allocator).Move(), allocator);

        auto formSpecial = RE::TESForm::LookupByID(Settings::specialPerkID);
        doc.AddMember("specialPerkID", rapidjson::Value(formSpecial ? FormUtil::NormalizeFormID(formSpecial).c_str() : "", allocator).Move(), allocator);

        auto formPowerSpecial = RE::TESForm::LookupByID(Settings::powerSpecialPerkID);
        doc.AddMember("powerSpecialPerkID", rapidjson::Value(formPowerSpecial ? FormUtil::NormalizeFormID(formPowerSpecial).c_str() : "", allocator).Move(), allocator);

        WriteJSON(doc, allocator, "ComboActionIDs", Settings::ComboActionIDs);
        WriteJSON(doc, allocator, "ComboMotionIDs", Settings::ComboMotionIDs);
        WriteJSON(doc, allocator, "PowerAttackActionIDs", Settings::PowerAttackActionIDs);
        WriteJSON(doc, allocator, "PowerAttackMotionIDs", Settings::PowerAttackMotionIDs);
        WriteJSON(doc, allocator, "SpecialAttackActionIDs", Settings::SpecialAttackActionIDs);
        WriteJSON(doc, allocator, "SpecialAttackMotionIDs", Settings::SpecialAttackMotionIDs);
        WriteJSON(doc, allocator, "PowerSpecialAttackActionIDs", Settings::PowerSpecialAttackActionIDs);
        WriteJSON(doc, allocator, "PowerSpecialAttackMotionIDs", Settings::PowerSpecialAttackMotionIDs);

        FILE* fp = nullptr; fopen_s(&fp, SETTINGS_PATH, "wb");
        if (fp) {
            char writeBuffer[65536]; rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
            rapidjson::Writer<rapidjson::FileWriteStream> writer(os); doc.Accept(writer); fclose(fp);
        }
        UpdateGameGlobals();
    }

    void LoadSettings() {
        FILE* fp = nullptr; fopen_s(&fp, SETTINGS_PATH, "rb");
        if (fp) {
            char readBuffer[65536]; rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            rapidjson::Document doc; doc.ParseStream(is); fclose(fp);

            if (doc.IsObject()) {
                if (doc.HasMember("bEnableComboAttack")) Settings::bEnableComboAttack = doc["bEnableComboAttack"].GetBool();
                if (doc.HasMember("bEnableDirectionalAttack")) Settings::bEnableDirectionalAttack = doc["bEnableDirectionalAttack"].GetBool();
                if (doc.HasMember("bEnablePowerAttack")) Settings::bEnablePowerAttack = doc["bEnablePowerAttack"].GetBool();
                if (doc.HasMember("bEnableSpecialAttack")) Settings::bEnableSpecialAttack = doc["bEnableSpecialAttack"].GetBool();
                if (doc.HasMember("bEnableSpecialPowerAttack")) Settings::bEnableSpecialPowerAttack = doc["bEnableSpecialPowerAttack"].GetBool();
                if (doc.HasMember("bDisableJumpingAttack")) Settings::bDisableJumpingAttack = doc["bDisableJumpingAttack"].GetBool();
                if (doc.HasMember("bPowerAttackLMB")) Settings::bPowerAttackLMB = doc["bPowerAttackLMB"].GetInt();
                if (doc.HasMember("bDisableVanillaSpecialAttack")) Settings::bDisableSpecialAttack = doc["bDisableVanillaSpecialAttack"].GetBool();
                if (doc.HasMember("bDisablePowerSpecialAttack")) Settings::bDisablePowerSpecialAttack = doc["bDisablePowerSpecialAttack"].GetBool();
                if (doc.HasMember("bInstantBlock")) Settings::bInstantBlock = doc["bInstantBlock"].GetBool();
                if (doc.HasMember("AnimationType")) Settings::AnimationType = doc["AnimationType"].GetInt();

                // Lendo booleanos de travas por perk
                if (doc.HasMember("bLockComboPerk")) Settings::bLockComboPerk = doc["bLockComboPerk"].GetBool();
                if (doc.HasMember("bLockPowerPerk")) Settings::bLockPowerPerk = doc["bLockPowerPerk"].GetBool();
                if (doc.HasMember("bLockSpecialPerk")) Settings::bLockSpecialPerk = doc["bLockSpecialPerk"].GetBool();
                if (doc.HasMember("bLockPowerSpecialPerk")) Settings::bLockPowerSpecialPerk = doc["bLockPowerSpecialPerk"].GetBool();
                if (doc.HasMember("bDisableDirPowerCustomInput")) Settings::bDisableDirPowerCustomInput = doc["bDisableDirPowerCustomInput"].GetBool();

                // Convertendo de volta e aplicando FormIDs utilizando FormUtil
                if (doc.HasMember("comboPerkID") && doc["comboPerkID"].IsString()) Settings::comboPerkID = FormUtil::FormIDFromString(doc["comboPerkID"].GetString());
                if (doc.HasMember("powerPerkID") && doc["powerPerkID"].IsString()) Settings::powerPerkID = FormUtil::FormIDFromString(doc["powerPerkID"].GetString());
                if (doc.HasMember("specialPerkID") && doc["specialPerkID"].IsString()) Settings::specialPerkID = FormUtil::FormIDFromString(doc["specialPerkID"].GetString());
                if (doc.HasMember("powerSpecialPerkID") && doc["powerSpecialPerkID"].IsString()) Settings::powerSpecialPerkID = FormUtil::FormIDFromString(doc["powerSpecialPerkID"].GetString());

                ReadJSON(doc, "ComboActionIDs", Settings::ComboActionIDs);
                ReadJSON(doc, "ComboMotionIDs", Settings::ComboMotionIDs);
                ReadJSON(doc, "PowerAttackActionIDs", Settings::PowerAttackActionIDs);
                ReadJSON(doc, "PowerAttackMotionIDs", Settings::PowerAttackMotionIDs);
                ReadJSON(doc, "SpecialAttackActionIDs", Settings::SpecialAttackActionIDs);
                ReadJSON(doc, "SpecialAttackMotionIDs", Settings::SpecialAttackMotionIDs);
                ReadJSON(doc, "PowerSpecialAttackActionIDs", Settings::PowerSpecialAttackActionIDs);
                ReadJSON(doc, "PowerSpecialAttackMotionIDs", Settings::PowerSpecialAttackMotionIDs);
            }
        }
        UpdateGameGlobals();
    }

    void UpdateGameGlobals() {
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;

        std::map<const char*, float> globalsToUpdate = {
            {"bfcoTG_KeyAttackComb", Settings::bEnableComboAttack ? 1.0f : 0.0f},
            {"bfcoINT_KeyAttackComb", Settings::bEnableComboAttack ? 2.0f : 0.0f},
            {"bfcoTG_JumpAttack", Settings::bDisableJumpingAttack ? 0.0f : 1.0f},
            {"bfcoTG_DirPowerAttack", Settings::bEnableDirectionalAttack ? 1.0f : 0.0f},
            {"bfcoTG_InputType", static_cast<float>(Settings::bPowerAttackLMB)},
        };

        for (auto const& [editorID, value] : globalsToUpdate) {
            RE::TESGlobal* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
            if (global) global->value = value;
        }

        auto iniCollection = RE::INISettingCollection::GetSingleton();
        if (iniCollection) {
            RE::Setting* setting = iniCollection->GetSetting("fSubsequentPowerAttackDelay:Controls");
            if (setting) {
                setting->data.f = (Settings::bPowerAttackLMB == 2) ? 0.3f : 2.0f;
            }
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            player->SetGraphVariableInt("BFCO_VanillaAnimationType", Settings::AnimationType);
            player->SetGraphVariableBool("BFCO_InstantBlock", Settings::bInstantBlock);
            player->SetGraphVariableInt("BFCO_SAtkEnable", Settings::bDisableSpecialAttack ? 0 : 1);
            player->SetGraphVariableInt("BFCO_PSAtkEnable", Settings::bDisablePowerSpecialAttack ? 0 : 1);
        }
    }

    void Register() {
        if (SKSEMenuFramework::IsInstalled()) {
            LoadLanguage();
            LoadSettings();
            RegisterAllInputs();
            TweenPauseRegister();
            SKSEMenuFramework::SetSection("BFCO");
            SKSEMenuFramework::AddSectionItem("Settings", Render);
        }
    }
}