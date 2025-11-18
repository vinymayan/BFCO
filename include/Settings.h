#include <Windows.h>
#include <map>
#include <string>
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace Settings {
    // Checkboxes (baseado no seu MCM)
    inline bool _isCurrentlyBlocking = false;
    inline bool DodgeCancel = false;
    inline bool CanPA = false;
    inline bool bEnableComboAttack = true;
    inline bool bEnableDirectionalAttack = true;
    inline bool bEnablePowerAttack = true;
    //inline bool bEnableLmbPowerAttack = false;
    //inline bool bEnableRmbPowerAttack = false;
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
    inline uint32_t BlockKey_g = -2;  // Controle (LB / L1)
    inline uint32_t AttackKey_k = -1;       // Teclado (E)
    inline uint32_t AttackKey_m = -3;      
    inline uint32_t AttackKey_g = -2;  // Controle (LB / L1)
    inline bool hasCMF = false;
    inline bool lockSprintAttack = false;
    inline bool disableMStaBash = false;
    inline bool disableDualblock = false;
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



const std::map<int, const char*> g_gamepad_dx_to_name_map = {
    {0, "[Nenhuma]"},
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
    {257, "Right Click"},
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