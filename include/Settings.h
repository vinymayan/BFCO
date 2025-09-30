#pragma once

// Estrutura que centraliza todas as configurações do mod.
// Isso substitui as variáveis globais 'const' que você tinha.
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
    bool lightAttackToPowerAttack = false;
    bool blockToPowerAttack = false;
    bool disableJumpingAttack = false;

    // --- Configurações de Debug ---
    bool debugPAPress = false;
    bool debugPAActivate = false;
};

extern Settings g_settings;