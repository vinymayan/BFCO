#pragma once 
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "ClibUtil/singleton.hpp"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "Settings.h"
#include <map>

// Mapas de conversão de teclas (declarados aqui, definidos em Manager.cpp)
extern const std::map<ImGuiKey, int> g_imgui_to_dx_map;
extern const std::map<int, const char*> g_dx_to_name_map;
extern const std::map<int, const char*> g_gamepad_dx_to_name_map;

inline RE::TESGlobal* DirPowerAttack = nullptr;
inline RE::TESGlobal* Global_LmbPowerAttack = nullptr;
inline RE::TESGlobal* Global_RmbPowerAttack = nullptr;
inline RE::TESGlobal* Global_KeyAttackComb = nullptr;
inline RE::TESGlobal* Global_DisJumpAttack = nullptr;

namespace SettingsMenu {
    // Função para salvar as configurações atuais no arquivo JSON.
    void Save();
    // Função para carregar as configurações do arquivo JSON.
    void Load();
    // Função para registrar o menu no SKSE Menu Framework.
    void Register();
    void __stdcall Render();
    void SyncGlobals();
    // --- Funções de Keybinding ---
    void Keybind(const char* label, int* dx_key_ptr);
    void GamepadKeybind(const char* label, int* dx_key_ptr);
}