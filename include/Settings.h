#pragma once
#include <string>

// Estrutura que centraliza todas as configurações do mod.
struct Settings {
    // --- Configurações Gerais de Ataque Poderoso ---
    bool allowZeroStamina = true;
    bool onlyDuringAttack = false;
    bool disableBlockDuringAttack = true;
    bool queuePA = true;

    // --- Configurações de Notificação e Timers ---
    bool notifyWindow = true;
    float notifyDuration = 0.05f;
    float queuePAExpire = 0.2f;

    // --- Configurações do BFCO ---
    bool enableComboKey = true;
    int holdToPowerAttackMode = 0;
    bool lightAttackToPowerAttack = false;
    bool blockToPowerAttack = false;
    bool disableJumpingAttack = false;
    bool enableDirectionalPowerAttack = false;

    // --- Configurações de Debug ---
    bool debugPAPress = false;
    bool debugPAActivate = false;

    // --- NOVAS: Configurações de Teclas (Hotkeys) ---
    // Valores padrão: V para ataque poderoso, Clique Direito para combo.
    int powerAttackKey_k = 47;  // Teclado (V)
    int powerAttackKey_g = 0;   // Controle (Nenhum)
    int comboKey_k = 301;       // Teclado (Mouse Direito)
    int comboKey_g = 512;       // Controle (RB / R1)
};

extern Settings g_settings;