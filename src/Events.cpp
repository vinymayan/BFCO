#include "Events.h"

bool DoesPluginFileExist(const std::string& pluginName) {
    // Constr�i o caminho esperado para a DLL
    std::string pathString = "Data/SKSE/Plugins/" + pluginName + ".dll";

    // Cria um objeto de caminho
    std::filesystem::path pluginPath(pathString);

    // Verifica se o arquivo existe no caminho especificado
    if (std::filesystem::exists(pluginPath)) {
        SKSE::log::info("VERIFICA��O DE ARQUIVO: '{}' encontrado no disco.", pathString);
        return true;
    } else {
        SKSE::log::info("VERIFICA��O DE ARQUIVO: '{}' N�O encontrado no disco.", pathString);
        return false;
    }
}

bool IsCycleMovesetActive() {

    if (DoesPluginFileExist("CycleMovesets")) {
        Settings::hasCMF = true;
        return true;
    } else {
        Settings::hasCMF = false;
        return false;
    }
}