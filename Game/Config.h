#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
public:
    Config()
    {
        reload();
    }

    // Открывает файл настроек для чтения из него параметров
    void reload()
    {
        // Поток для чтения данных из settings.json
        std::ifstream fin(project_path + "settings.json");
        fin >> config; // Читаем из потока в config
        fin.close(); // Закрываем поток
    }

    // Возвращает значение настройки (например conf('WindowSize', 'Width'))
    auto operator()(const string& setting_dir, const string& setting_name) const
    {
        return config[setting_dir][setting_name];
    }

private:
    json config;
};