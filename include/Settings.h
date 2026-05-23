#include <Windows.h>
#include <map>
#include <string>
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "InputManagerAPI.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

inline const char* actionStateNames[] = { "Ignore", "Tap", "Hold", "Gesture", "Press" };

constexpr uint32_t MOUSE_OFFSET = 256;
constexpr uint32_t GAMEPAD_OFFSET = 266;

inline const char* pcKeyNames[] = {
    "None",
    // Mouse
    "Mouse 1 (Left)", "Mouse 2 (Right)", "Mouse 3 (Middle)", "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7", "Mouse 8",
    "Mouse Wheel Up", "Mouse Wheel Down",
    // Alfabeto
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    // Números
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    // Pontuação e Símbolos
    "Minus ( - )", "Equals ( = )", "Bracket Left ( [ )", "Bracket Right ( ] )", "Semicolon ( ; )", "Apostrophe ( ' )", "Tilde ( ~ )", "Backslash ( \\ )", "Comma ( , )", "Period ( . )", "Slash ( / )",
    // F-Keys
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    // Especiais e Modificadores
    "Esc", "Tab", "Caps Lock", "Shift (Left)", "Shift (Right)", "Ctrl (Left)", "Ctrl (Right)", "Alt (Left)", "Alt (Right)",
    "Space", "Enter", "Backspace", "Print Screen", "Scroll Lock", "Pause", "Num Lock",
    // Navegação
    "Up Arrow", "Down Arrow", "Left Arrow", "Right Arrow", "Insert", "Delete", "Home", "End", "Page Up", "Page Down",
    // Numpad
    "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8", "Num 9",
    "Num +", "Num -", "Num *", "Num /", "Num Enter", "Num Dot"
};

inline const int pcKeyIDs[] = {
    0,
    // Mouse (Base + 256)
    RE::BSWin32MouseDevice::Keys::kLeftButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kRightButton + MOUSE_OFFSET,
    RE::BSWin32MouseDevice::Keys::kMiddleButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton3 + MOUSE_OFFSET,
    RE::BSWin32MouseDevice::Keys::kButton4 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton5 + MOUSE_OFFSET,
    RE::BSWin32MouseDevice::Keys::kButton6 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton7 + MOUSE_OFFSET,
    RE::BSWin32MouseDevice::Keys::kWheelUp + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kWheelDown + MOUSE_OFFSET,
    // Alfabeto
    RE::BSKeyboardDevice::Keys::kA, RE::BSKeyboardDevice::Keys::kB, RE::BSKeyboardDevice::Keys::kC, RE::BSKeyboardDevice::Keys::kD,
    RE::BSKeyboardDevice::Keys::kE, RE::BSKeyboardDevice::Keys::kF, RE::BSKeyboardDevice::Keys::kG, RE::BSKeyboardDevice::Keys::kH,
    RE::BSKeyboardDevice::Keys::kI, RE::BSKeyboardDevice::Keys::kJ, RE::BSKeyboardDevice::Keys::kK, RE::BSKeyboardDevice::Keys::kL,
    RE::BSKeyboardDevice::Keys::kM, RE::BSKeyboardDevice::Keys::kN, RE::BSKeyboardDevice::Keys::kO, RE::BSKeyboardDevice::Keys::kP,
    RE::BSKeyboardDevice::Keys::kQ, RE::BSKeyboardDevice::Keys::kR, RE::BSKeyboardDevice::Keys::kS, RE::BSKeyboardDevice::Keys::kT,
    RE::BSKeyboardDevice::Keys::kU, RE::BSKeyboardDevice::Keys::kV, RE::BSKeyboardDevice::Keys::kW, RE::BSKeyboardDevice::Keys::kX,
    RE::BSKeyboardDevice::Keys::kY, RE::BSKeyboardDevice::Keys::kZ,
    // Números
    RE::BSKeyboardDevice::Keys::kNum1, RE::BSKeyboardDevice::Keys::kNum2, RE::BSKeyboardDevice::Keys::kNum3, RE::BSKeyboardDevice::Keys::kNum4,
    RE::BSKeyboardDevice::Keys::kNum5, RE::BSKeyboardDevice::Keys::kNum6, RE::BSKeyboardDevice::Keys::kNum7, RE::BSKeyboardDevice::Keys::kNum8,
    RE::BSKeyboardDevice::Keys::kNum9, RE::BSKeyboardDevice::Keys::kNum0,
    // Pontuação e Símbolos
    RE::BSKeyboardDevice::Keys::kMinus, RE::BSKeyboardDevice::Keys::kEquals, RE::BSKeyboardDevice::Keys::kBracketLeft,
    RE::BSKeyboardDevice::Keys::kBracketRight, RE::BSKeyboardDevice::Keys::kSemicolon, RE::BSKeyboardDevice::Keys::kApostrophe,
    RE::BSKeyboardDevice::Keys::kTilde, RE::BSKeyboardDevice::Keys::kBackslash, RE::BSKeyboardDevice::Keys::kComma,
    RE::BSKeyboardDevice::Keys::kPeriod, RE::BSKeyboardDevice::Keys::kSlash,
    // F-Keys
    RE::BSKeyboardDevice::Keys::kF1, RE::BSKeyboardDevice::Keys::kF2, RE::BSKeyboardDevice::Keys::kF3, RE::BSKeyboardDevice::Keys::kF4,
    RE::BSKeyboardDevice::Keys::kF5, RE::BSKeyboardDevice::Keys::kF6, RE::BSKeyboardDevice::Keys::kF7, RE::BSKeyboardDevice::Keys::kF8,
    RE::BSKeyboardDevice::Keys::kF9, RE::BSKeyboardDevice::Keys::kF10, RE::BSKeyboardDevice::Keys::kF11, RE::BSKeyboardDevice::Keys::kF12,
    // Especiais e Modificadores
    RE::BSKeyboardDevice::Keys::kEscape, RE::BSKeyboardDevice::Keys::kTab, RE::BSKeyboardDevice::Keys::kCapsLock,
    RE::BSKeyboardDevice::Keys::kLeftShift, RE::BSKeyboardDevice::Keys::kRightShift,
    RE::BSKeyboardDevice::Keys::kLeftControl, RE::BSKeyboardDevice::Keys::kRightControl,
    RE::BSKeyboardDevice::Keys::kLeftAlt, RE::BSKeyboardDevice::Keys::kRightAlt,
    RE::BSKeyboardDevice::Keys::kSpacebar, RE::BSKeyboardDevice::Keys::kEnter, RE::BSKeyboardDevice::Keys::kBackspace,
    RE::BSKeyboardDevice::Keys::kPrintScreen, RE::BSKeyboardDevice::Keys::kScrollLock, RE::BSKeyboardDevice::Keys::kPause,
    RE::BSKeyboardDevice::Keys::kNumLock,
    // Navegação
    RE::BSKeyboardDevice::Keys::kUp, RE::BSKeyboardDevice::Keys::kDown, RE::BSKeyboardDevice::Keys::kLeft, RE::BSKeyboardDevice::Keys::kRight,
    RE::BSKeyboardDevice::Keys::kInsert, RE::BSKeyboardDevice::Keys::kDelete, RE::BSKeyboardDevice::Keys::kHome, RE::BSKeyboardDevice::Keys::kEnd,
    RE::BSKeyboardDevice::Keys::kPageUp, RE::BSKeyboardDevice::Keys::kPageDown,
    // Numpad
    RE::BSKeyboardDevice::Keys::kKP_0, RE::BSKeyboardDevice::Keys::kKP_1, RE::BSKeyboardDevice::Keys::kKP_2, RE::BSKeyboardDevice::Keys::kKP_3,
    RE::BSKeyboardDevice::Keys::kKP_4, RE::BSKeyboardDevice::Keys::kKP_5, RE::BSKeyboardDevice::Keys::kKP_6, RE::BSKeyboardDevice::Keys::kKP_7,
    RE::BSKeyboardDevice::Keys::kKP_8, RE::BSKeyboardDevice::Keys::kKP_9,
    RE::BSKeyboardDevice::Keys::kKP_Plus, RE::BSKeyboardDevice::Keys::kKP_Subtract, RE::BSKeyboardDevice::Keys::kKP_Multiply,
    RE::BSKeyboardDevice::Keys::kKP_Divide, RE::BSKeyboardDevice::Keys::kKP_Enter, RE::BSKeyboardDevice::Keys::kKP_Decimal
};

inline const char* gamepadKeyNames[] = {
    "None",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
    "Start / Options", "Back / Share / Select", "LS / L3 (Left Stick)", "RS / R3 (Right Stick)",
    "LB / L1 (Left Bumper)", "RB / R1 (Right Bumper)",
    "LT / L2 (Left Trigger)", "RT / R2 (Right Trigger)",
    "A / Cross", "B / Circle", "X / Square", "Y / Triangle"
};

inline const int gamepadKeyIDs[] = {
    0,
    RE::BSWin32GamepadDevice::Keys::kUp + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kDown + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kLeft + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kRight + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kStart + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kBack + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kLeftThumb + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kRightThumb + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kLeftShoulder + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kRightShoulder + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kLeftTrigger + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kRightTrigger + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kA + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kB + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kX + GAMEPAD_OFFSET,
    RE::BSWin32GamepadDevice::Keys::kY + GAMEPAD_OFFSET
};

namespace Settings {
    inline std::vector<int> ComboActionIDs;
    inline std::vector<int> ComboMotionIDs;
    inline std::vector<int> PowerAttackActionIDs;
    inline std::vector<int> PowerAttackMotionIDs;
    inline std::vector<int> SpecialAttackActionIDs;
    inline std::vector<int> SpecialAttackMotionIDs;
    inline std::vector<int> PowerSpecialAttackActionIDs;
    inline std::vector<int> PowerSpecialAttackMotionIDs;
    // Checkboxes (baseado no seu MCM)
    inline bool bEnableComboAttack = true;
    inline bool bEnableDirectionalAttack = false;
    inline bool bEnableSpecialPowerAttack = true;
    inline bool bEnableSpecialAttack = true;
    inline bool bEnablePowerAttack = true;
    inline bool bDisableJumpingAttack = false;
    inline bool bDisableSpecialAttack = false;
    inline bool bDisablePowerSpecialAttack = false;
    inline int bPowerAttackLMB = 0;
    static inline bool bInstantBlock = true;
    static inline int AnimationType = 0;

    // Sistemas de bloqueio por Perk e Input Direcional Customizado
    inline bool bLockComboPerk = false;
    inline RE::FormID comboPerkID = 0;
    inline bool bLockPowerPerk = false;
    inline RE::FormID powerPerkID = 0;
    inline bool bLockSpecialPerk = false;
    inline RE::FormID specialPerkID = 0;
    inline bool bLockPowerSpecialPerk = false;
    inline RE::FormID powerSpecialPerkID = 0;
    inline bool bDisableDirPowerCustomInput = false;

}

// Namespace para organizar as funções do nosso menu
namespace BFCOMenu {
    void UnregisterInputCategory(const std::string& actionId);
    void TweenPauseRegister();
    void RegisterAllInputs();
    // Registra o menu no SKSE Menu Framework
    void Register();

    // Funções para salvar e carregar as configurações de um arquivo JSON
    void LoadSettings();
    void SaveSettings();

    // Função que aplica as configurações carregadas/salvas às Variáveis Globais do jogo
    void UpdateGameGlobals();
    inline RE::TESGlobal* g_targetGlobal = nullptr;
}

