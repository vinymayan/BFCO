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
        const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "None";

        if (ImGuiMCP::BeginCombo(label, preview_value)) {
            static char searchBuf[128] = "";
            if (ImGuiMCP::IsWindowAppearing()) {
                searchBuf[0] = '\0';
                ImGuiMCP::SetKeyboardFocusHere();
            }
            ImGuiMCP::InputText("Filter...##Search", searchBuf, sizeof(searchBuf));
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
    static int current_edit_action_id = -1;
    static const char* current_edit_label = nullptr;
    static InputManagerAPI::ActionInfo edit_info;
    static char edit_nameBuf[64] = "";
    static int ui_pcMainIdx = 0, ui_pcModIdx = 0, current_pcModAct = 0;
    static int ui_padMainIdx = 0, ui_padModIdx = 0, current_padModAct = 0;
    static std::string updateStatusMsg = "";
    static bool updateSuccess = false;

    bool RenderInputSelector(const char* label, const char* purpose, int& inputType, int& actionID, int& motionID) {
        bool changed = false;

        ImGui::Text("%s", label);

        if (!InputManagerAPI::_API) {
            ImGui::TextDisabled("[Input Manager nao detectado]");
            return false;
        }

        // --- 1. Dropdown de Tipo (Action ou Motion) ---
        const char* typeItems[] = { "Action (Normal Input)", "Motion (Combo Sequence)" };
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo((std::string("Input Type##") + label).c_str(), typeItems[inputType])) {
            for (int i = 0; i < 2; i++) {
                if (ImGui::Selectable(typeItems[i], inputType == i)) {
                    // Limpa listeners antigos ao trocar o tipo
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
            std::string previewValue = "[Nenhuma Acao Selecionada]";

            if (actionID >= 0 && actionID < actionCount) {
                auto info = InputManagerAPI::_API->GetActionInfo(actionID);
                previewValue = "[" + std::to_string(actionID) + "] " + (info.name ? std::string(info.name) : "Unnamed");
            }

            ImGui::SetNextItemWidth(250);
            if (ImGui::BeginCombo((std::string("Select Action##") + label).c_str(), previewValue.c_str())) {
                if (ImGui::Selectable("[Desativado]", actionID == -1)) {
                    if (actionID != -1) InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, false);
                    actionID = -1;
                    changed = true;
                }
                for (int i = 0; i < actionCount; ++i) {
                    auto info = InputManagerAPI::_API->GetActionInfo(i);
                    std::string itemLabel = "[" + std::to_string(i) + "] " + (info.name ? std::string(info.name) : "Unnamed");
                    if (ImGui::Selectable(itemLabel.c_str(), actionID == i)) {
                        if (actionID != -1) InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, false);
                        actionID = i;
                        InputManagerAPI::_API->UpdateListener(0, actionID, "BFCO", purpose, true);
                        changed = true;
                        current_edit_action_id = -1; // Reset do editor UI
                    }
                }
                ImGui::EndCombo();
            }

            // ================== EDITOR DE ACTION (Com nova Lógica) ==================
            if (actionID != -1) {
                if (ImGui::TreeNode((std::string("Edit Keys for ") + label).c_str())) {

                    if (current_edit_action_id != actionID || current_edit_label != label) {
                        edit_info = InputManagerAPI::_API->GetActionInfo(actionID);
                        current_edit_action_id = actionID;
                        current_edit_label = label;

                        strncpy_s(edit_nameBuf, edit_info.name ? edit_info.name : "Unnamed Action", sizeof(edit_nameBuf) - 1);
                        edit_nameBuf[sizeof(edit_nameBuf) - 1] = '\0';

                        ui_pcMainIdx = GetIndexFromID(edit_info.pcMainKey, pcKeyIDs, std::size(pcKeyIDs));
                        current_pcModAct = edit_info.pcModAction;
                        ui_pcModIdx = (current_pcModAct == 3) ? 0 : GetIndexFromID(edit_info.pcModifierKey, pcKeyIDs, std::size(pcKeyIDs));

                        ui_padMainIdx = GetIndexFromID(edit_info.gamepadMainKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                        current_padModAct = edit_info.gamepadModAction;
                        ui_padModIdx = (current_padModAct == 3) ? 0 : GetIndexFromID(edit_info.gamepadModifierKey, gamepadKeyIDs, std::size(gamepadKeyIDs));
                        updateStatusMsg = "";
                    }

                    ImGui::InputText("Input Name", edit_nameBuf, sizeof(edit_nameBuf));
                    ImGui::Separator();

                    // --- PC ---
                    ImGui::TextColored({ 0.4f, 0.8f, 1.0f, 1.0f }, "Keyboard and Mouse");
                    if (SearchableCombo("PC Main Key", &ui_pcMainIdx, pcKeyNames, std::size(pcKeyNames))) {
                        edit_info.pcMainKey = pcKeyIDs[ui_pcMainIdx];
                    }

                    // PC Main Action com verificação para resetar Mod Action se não for Hold (2)
                    if (ImGui::Combo("PC Main Action", &edit_info.pcMainAction, actionStateNames, std::size(actionStateNames))) {
                        if (edit_info.pcMainAction != 2 && current_pcModAct == 3) {
                            current_pcModAct = 0; // Reseta se sair de Hold e o Mod for Gesto
                        }
                    }

                    // AJUSTE 2: PC Main Tap Count
                    if (edit_info.pcMainAction == 1) { // 1 = Tap
                        if (edit_info.pcMainTapCount < 1) edit_info.pcMainTapCount = 1;
                        ImGui::SliderInt("PC Main Tap Amount", &edit_info.pcMainTapCount, 1, 5);
                    }

                    // AJUSTE 1: PC Mod Action (Combo customizado para esconder Gesture)
                    if (ImGui::BeginCombo("PC Mod Action", actionStateNames[current_pcModAct])) {
                        for (int i = 0; i < std::size(actionStateNames); i++) {
                            // Pula a opção Gesture (3) se a Main Action não for Hold (2)
                            if (i == 3 && edit_info.pcMainAction != 2) continue;

                            bool is_selected = (current_pcModAct == i);
                            if (ImGui::Selectable(actionStateNames[i], is_selected)) {
                                current_pcModAct = i;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (current_pcModAct != edit_info.pcModAction) {
                        if (current_pcModAct == 3) {
                            edit_info.pcModifierKey = 0;
                        }
                        else if (current_pcModAct != 0) {
                            ui_pcModIdx = 0;
                            edit_info.pcModifierKey = pcKeyIDs[ui_pcModIdx];
                        }
                        else {
                            edit_info.pcModifierKey = 0;
                        }
                        edit_info.pcModAction = current_pcModAct;
                    }

                    if (edit_info.pcModAction == 3) { // 3 = Gesto
                        int gestIdx = edit_info.pcModifierKey;
                        std::string gesturePreview = (gestIdx >= 0 && gestIdx < InputManagerAPI::_API->GetInputCount(2))
                            ? InputManagerAPI::_API->GetInputName(2, gestIdx) : "[ No Gesture ]";

                        if (ImGui::BeginCombo("PC Gesture", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < InputManagerAPI::_API->GetInputCount(2); ++gIdx) {
                                if (ImGui::Selectable(InputManagerAPI::_API->GetInputName(2, (int)gIdx), gestIdx == (int)gIdx)) {
                                    edit_info.pcModifierKey = (int)gIdx;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else if (edit_info.pcModAction != 0) {
                        if (SearchableCombo("PC Mod Key", &ui_pcModIdx, pcKeyNames, std::size(pcKeyNames))) {
                            edit_info.pcModifierKey = pcKeyIDs[ui_pcModIdx];
                        }

                        if (edit_info.pcModAction == 1) { // 1 = Tap
                            if (edit_info.pcModTapCount < 1) edit_info.pcModTapCount = 1;
                            ImGui::SliderInt("PC Mod Tap Amount", &edit_info.pcModTapCount, 1, 5);
                        }
                    }

                    // --- GAMEPAD ---
                    ImGui::Separator();
                    ImGui::TextColored({ 0.4f, 0.8f, 1.0f, 1.0f }, "Gamepad");
                    if (SearchableCombo("Pad Main Key", &ui_padMainIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                        edit_info.gamepadMainKey = gamepadKeyIDs[ui_padMainIdx];
                    }

                    // Pad Main Action com verificação para resetar Mod Action se não for Hold (2)
                    if (ImGui::Combo("Pad Main Action", &edit_info.gamepadMainAction, actionStateNames, std::size(actionStateNames))) {
                        if (edit_info.gamepadMainAction != 2 && current_padModAct == 3) {
                            current_padModAct = 0; // Reseta se sair de Hold e o Mod for Gesto
                        }
                    }

                    // AJUSTE 2: Pad Main Tap Count
                    if (edit_info.gamepadMainAction == 1) { // 1 = Tap
                        if (edit_info.gamepadMainTapCount < 1) edit_info.gamepadMainTapCount = 1;
                        ImGui::SliderInt("Pad Main Tap Amount", &edit_info.gamepadMainTapCount, 1, 5);
                    }

                    // AJUSTE 1: Pad Mod Action (Combo customizado para esconder Gesture)
                    if (ImGui::BeginCombo("Pad Mod Action", actionStateNames[current_padModAct])) {
                        for (int i = 0; i < std::size(actionStateNames); i++) {
                            // Pula a opção Gesture (3) se a Main Action não for Hold (2)
                            if (i == 3 && edit_info.gamepadMainAction != 2) continue;

                            bool is_selected = (current_padModAct == i);
                            if (ImGui::Selectable(actionStateNames[i], is_selected)) {
                                current_padModAct = i;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (current_padModAct != edit_info.gamepadModAction) {
                        if (current_padModAct == 3) {
                            edit_info.gamepadModifierKey = 0;
                        }
                        else if (current_padModAct != 0) {
                            ui_padModIdx = 0;
                            edit_info.gamepadModifierKey = gamepadKeyIDs[ui_padModIdx];
                        }
                        else {
                            edit_info.gamepadModifierKey = 0;
                        }
                        edit_info.gamepadModAction = current_padModAct;
                    }

                    if (edit_info.gamepadModAction == 3) { // 3 = Gesto
                        int gestIdx = edit_info.gamepadModifierKey;
                        std::string gesturePreview = (gestIdx >= 0 && gestIdx < InputManagerAPI::_API->GetInputCount(2))
                            ? InputManagerAPI::_API->GetInputName(2, gestIdx) : "[ No Gesture ]";

                        if (ImGui::BeginCombo("Pad Gesture", gesturePreview.c_str())) {
                            for (size_t gIdx = 0; gIdx < InputManagerAPI::_API->GetInputCount(2); ++gIdx) {
                                if (ImGui::Selectable(InputManagerAPI::_API->GetInputName(2, (int)gIdx), gestIdx == (int)gIdx)) {
                                    edit_info.gamepadModifierKey = (int)gIdx;
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else if (edit_info.gamepadModAction != 0) {
                        if (SearchableCombo("Pad Mod Key", &ui_padModIdx, gamepadKeyNames, std::size(gamepadKeyNames))) {
                            edit_info.gamepadModifierKey = gamepadKeyIDs[ui_padModIdx];
                        }

                        if (edit_info.gamepadModAction == 1) { // 1 = Tap
                            if (edit_info.gamepadModTapCount < 1) edit_info.gamepadModTapCount = 1;
                            ImGui::SliderInt("Pad Mod Tap Amount", &edit_info.gamepadModTapCount, 1, 5);
                        }
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Update Mapping and Save")) {
                        edit_info.name = edit_nameBuf;
                        // Conforme solicitado: sem mexer com timming (deixando falso para usar o padrao da API)
                        edit_info.useCustomTimings = false;

                        bool success = InputManagerAPI::_API->UpdateActionMapping(actionID, edit_info);
                        if (success) {
                            updateStatusMsg = "Mapping updated successfully.";
                            updateSuccess = true;
                        }
                        else {
                            updateStatusMsg = "ERROR! Duplicate name or Combo already registered.";
                            updateSuccess = false;
                        }
                    }

                    if (!updateStatusMsg.empty()) {
                        ImGui::TextColored(updateSuccess ? ImGui::ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImGui::ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", updateStatusMsg.c_str());
                    }
                    ImGui::TreePop();
                }
                else {
                    if (current_edit_label == label) {
                        current_edit_action_id = -1;
                        current_edit_label = nullptr;
                    }
                }
            }
        }
        // --- 3. Lógica para Motion (Tipo 1) ---
        else if (inputType == 1) {
            size_t motionCount = InputManagerAPI::_API->GetInputCount(1);
            std::string previewValue = "[Nenhum Motion Selecionado]";

            if (motionID >= 0 && motionID < motionCount) {
                auto info = InputManagerAPI::_API->GetMotionInfo(motionID);
                previewValue = "[" + std::to_string(motionID) + "] " + (info.name ? std::string(info.name) : "Unnamed");
            }

            ImGui::SetNextItemWidth(250);
            if (ImGui::BeginCombo((std::string("Select Motion##") + label).c_str(), previewValue.c_str())) {
                if (ImGui::Selectable("[Desativado]", motionID == -1)) {
                    if (motionID != -1) InputManagerAPI::_API->UpdateListener(1, motionID, "BFCO", purpose, false);
                    motionID = -1;
                    changed = true;
                }
                for (int i = 0; i < motionCount; ++i) {
                    auto info = InputManagerAPI::_API->GetMotionInfo(i);
                    std::string itemLabel = "[" + std::to_string(i) + "] " + (info.name ? std::string(info.name) : "Unnamed");
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

        ImGui::Text("Combat settings BFCO");
        ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox("Enable combo attack", &Settings::bEnableComboAttack)) settings_changed = true;
        if (Settings::bEnableComboAttack) {
            if (RenderInputSelector("Combo Attack Config", "Combo Attack", Settings::ComboInputType, Settings::ComboActionID, Settings::ComboMotionID)) {
                settings_changed = true;
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox("Enable direcional power attack", &Settings::bEnableDirectionalAttack)) settings_changed = true;
        if (ImGui::Checkbox("Enable power attack key", &Settings::bEnablePowerAttack)) settings_changed = true;

        if (Settings::bEnablePowerAttack) {
            if (RenderInputSelector("Power Attack Config", "Power Attack", Settings::PowerAttackInputType, Settings::PowerAttackActionID, Settings::PowerAttackMotionID)) {
                settings_changed = true;
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Checkbox("Instant Block (Cancel attacks to block)", &Settings::bInstantBlock)) settings_changed = true;

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::Text("Animation Type");

        const char* animItems[] = { "BFCO All Attacks", "Vanilla Light Attacks" };
        const char* animTooltips[] = {
            "All attacks use BFCO animations",
            "Player light attacks use vanilla animations"
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


        ImGui::Text("Light Attack Mode");
        const char* lmbItems[] = { "Vanilla", "Modern", "Modern + AutoNA" };
        const char* lmbTooltips[] = {
            "Tap = Light, Hold = Power",
            "LMB = Light only",
            "Hold = Auto Light"
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
        if (ImGui::Checkbox("Disable jump attack", &Settings::bDisableJumpingAttack)) settings_changed = true;

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
    }
    // Registra o menu
    void Register() {
        if (SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("SKSE Menu Framework encontrado. Registrando o menu BFCO.");
            LoadSettings();
            SKSEMenuFramework::SetSection("BFCO");
            SKSEMenuFramework::AddSectionItem("Settings", Render);
        }
        else {
            SKSE::log::warn("SKSE Menu Framework nao encontrado. O menu BFCO nao sera registrado.");
        }
    }
}