#pragma once
#include <string>

// Estrutura que centraliza todas as configura��es do mod.
struct Settings {
    // --- Configura��es Gerais de Ataque Poderoso ---
    bool allowZeroStamina = true;
    bool onlyDuringAttack = false;
    bool disableBlockDuringAttack = true;
    bool queuePA = true;

    // --- Configura��es de Notifica��o e Timers ---
    bool notifyWindow = true;
    float notifyDuration = 0.05f;
    float queuePAExpire = 0.2f;

    // --- Configura��es do BFCO ---
    bool enableComboKey = true;
    int holdToPowerAttackMode = 0;
    bool lightAttackToPowerAttack = false;
    bool blockToPowerAttack = false;
    bool disableJumpingAttack = false;
    bool enableDirectionalPowerAttack = false;

    // --- Configura��es de Debug ---
    bool debugPAPress = false;
    bool debugPAActivate = false;

    // --- NOVAS: Configura��es de Teclas (Hotkeys) ---
    // Valores padr�o: V para ataque poderoso, Clique Direito para combo.
    int powerAttackKey_k = 47;  // Teclado (V)
    int powerAttackKey_g = 0;   // Controle (Nenhum)
    int comboKey_k = 301;       // Teclado (Mouse Direito)
    int comboKey_g = 512;       // Controle (RB / R1)
};

extern Settings g_settings;