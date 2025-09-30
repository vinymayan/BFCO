#pragma once

// Estrutura que centraliza todas as configura��es do mod.
// Isso substitui as vari�veis globais 'const' que voc� tinha.
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
    bool lightAttackToPowerAttack = false;
    bool blockToPowerAttack = false;
    bool disableJumpingAttack = false;

    // --- Configura��es de Debug ---
    bool debugPAPress = false;
    bool debugPAActivate = false;
};

extern Settings g_settings;