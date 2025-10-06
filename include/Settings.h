#include <Windows.h>
#include <map>
#include <string>
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace Settings {
    // Checkboxes (baseado no seu MCM)

    inline bool bEnableComboAttack = true;
    inline bool bEnableDirectionalAttack = true;
    inline bool bEnableLmbPowerAttack = false;
    inline bool bEnableRmbPowerAttack = false;
    inline bool bDisableJumpingAttack = false;
    inline bool bPowerAttackLMB = false;

    // Keybinds (separados para teclado e controle)
    // Os valores padrão podem ser ajustados conforme necessário. Usei códigos de tecla comuns.
    inline uint32_t comboKey_k = 47;  // Teclado (V)
    inline uint32_t comboKey_m = 0;      
    inline uint32_t comboKey_g = 265;  // Controle (RB / R1)

    inline uint32_t PowerAttackKey_k = 18;  // Teclado (E)
    inline uint32_t PowerAttackKey_m = 0;      
    inline uint32_t PowerAttackKey_g = 512;  // Controle (LB / L1)
    inline uint32_t BlockKey_k = -1;       // Teclado (E)
    inline uint32_t BlockKey_m = -3;      
    inline uint32_t BlockKey_g = 0;  // Controle (LB / L1)
    inline bool hasCMF = false;
    inline bool lockSprintAttack = false;
}

// Namespace para organizar as funções do nosso menu
namespace BFCOMenu {
    // Registra o menu no SKSE Menu Framework
    void Register();

    // Funções para salvar e carregar as configurações de um arquivo JSON
    void LoadSettings();
    void SaveSettings();

    // Função que aplica as configurações carregadas/salvas às Variáveis Globais do jogo
    void UpdateGameGlobals();
    inline RE::TESGlobal* g_targetGlobal = nullptr;
}

const std::map<ImGuiKey, int> g_imgui_to_dx_map = {
    {ImGuiKey_1, 2},
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
    {ImGuiKey_Semicolon, 39},
    {ImGuiKey_Equal, 13},
    {ImGuiKey_Comma, 51},
    {ImGuiKey_Minus, 12},
    {ImGuiKey_Period, 52},
    {ImGuiKey_Slash, 53},
    {ImGuiKey_Backslash, 43},
    // --- MOUSE (Correto como antes, com offset para evitar colisão) ---
    {ImGuiKey_MouseLeft, 300},
    {ImGuiKey_MouseRight, 301},
    {ImGuiKey_MouseMiddle, 302},
    {ImGuiKey_MouseX1, 303},
    {ImGuiKey_MouseX2, 304},

    // --- CONTROLE (GAMEPAD) - CORRIGIDO SEGUINDO A LISTA HEXADECIMAL (XBOX) ---
    // Botões (Bitmasks)
    {ImGuiKey_GamepadDpadUp, 1},        // 0x0001
    {ImGuiKey_GamepadDpadDown, 2},      // 0x0002
    {ImGuiKey_GamepadDpadLeft, 4},      // 0x0004
    {ImGuiKey_GamepadDpadRight, 8},     // 0x0008
    {ImGuiKey_GamepadStart, 16},        // 0x0010
    {ImGuiKey_GamepadBack, 32},         // 0x0020
    {ImGuiKey_GamepadL3, 64},           // 0x0040 (Left Thumb)
    {ImGuiKey_GamepadR3, 128},          // 0x0080 (Right Thumb)
    {ImGuiKey_GamepadL1, 256},          // 0x0100 (Left Shoulder)
    {ImGuiKey_GamepadR1, 512},          // 0x0200 (Right Shoulder)
    {ImGuiKey_GamepadFaceDown, 4096},   // 0x1000 (A)
    {ImGuiKey_GamepadFaceRight, 8192},  // 0x2000 (B)
    {ImGuiKey_GamepadFaceLeft, 16384},  // 0x4000 (X)
    {ImGuiKey_GamepadFaceUp, 32768},    // 0x8000 (Y)

    // Gatilhos e Analógicos (tratados como botões na sua lista)
    {ImGuiKey_GamepadL2, 9},   // 0x0009 (Left Trigger)
    {ImGuiKey_GamepadR2, 10},  // 0x000A (Right Trigger)
    // A sua lista também tem kLeftStick = 0x0B e kRightStick = 0x0C,
    // que representam o input geral dos analógicos.
    // ImGui não tem uma tecla única para isso, apenas para o clique (L3/R3).
};

const std::map<int, const char*> g_gamepad_dx_to_name_map = {
    // --- GAMEPAD (ATUALIZADO) ---
    {1, "DPad Up"},
    {2, "DPad Down"},
    {4, "DPad Left"},
    {8, "DPad Right"},
    {16, "Start"},
    {32, "Back"},
    {64, "L3"},
    {128, "R3"},
    {256, "LB"},
    {512, "RB"},
    {4096, "A / X"},
    {8192, "B / O"},
    {16384, "X / Square"},
    {32768, "Y / Triangle"},
    {9, "LT/L2"},
    {10, "RT/R2"}};

// MAPA 2: Converte o Scan Code do DirectX para um Nome (o que você precisa exibir) - O seu mapa original.
const std::map<int, const char*> g_dx_to_name_map = {
    {0, "[Nenhuma]"},
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
    //{256, "Left Click"},
    //{257, "Right Click"},
    {258, "Middle Mouse Button"},
    {259, "Mouse 4"},
    {260, "Mouse 5"},
    {261, "Mouse 6"},
    {262, "Mouse 7"},
    {263, "Mouse 8"},
    {55, "Keypad *"}, 
    {181, "Keypad /"},
    {74, "Keypad -"},  // <-- ADICIONADO
    {78, "Keypad +"},  // <-- ADICIONADO
    {73, "Keypad 9"},  // <-- ADICIONADO
    {72, "Keypad 8"},  // <-- ADICIONADO
    {71, "Keypad 7"},  // <-- ADICIONADO
    {77, "Keypad 6"},  // <-- ADICIONADO
    {76, "Keypad 5"},  // <-- ADICIONADO
    {75, "Keypad 4"},  // <-- ADICIONADO
    {81, "Keypad 3"},  // <-- ADICIONADO
    {80, "Keypad 2"},  // <-- ADICIONADO
    {79, "Keypad 1"},  // <-- ADICIONADO
    {82, "Keypad 0"},  // <-- ADICIONADO
    {83, "Keypad ."},  // <-- ADICIONADO
    //{261, "Scroll Up"},
    //{262, "Scroll Down"}
    // Adicionei a key 0 para o caso "Nenhuma" para simplificar.
};