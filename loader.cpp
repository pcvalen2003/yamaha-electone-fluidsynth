#include "electone.h"
#include <yaml-cpp/yaml.h>

// Definimos las variables globales de DB aquí
std::map<int, SoundPatch> generalSoundsDB;
std::map<int, SoundPatch> leadSoundsDB;

void loadInstrumentDB(const std::string& filename) {
    try {
        std::cout << "Cargando Sonidos: " << filename << std::endl;
        YAML::Node config = YAML::LoadFile(filename);

        // 1. Cargar "sounds" (General)
        if (config["sounds"]) {
            for (const auto& item : config["sounds"]) {
                int id = item.first.as<int>();
                auto values = item.second.as<std::vector<int>>(); // [Bank, PC]

                SoundPatch p;
                if (values.size() >= 2) {
                    p.bank = values[0];
                    p.program = values[1];
                }
                generalSoundsDB[id] = p;
            }
        }

        // 2. Cargar "lead_sounds" (Con Portamento)
        if (config["lead_sounds"]) {
            for (const auto& item : config["lead_sounds"]) {
                int id = item.first.as<int>();
                auto values = item.second.as<std::vector<int>>(); // [Bank, PC, PortaSw, PortaTime]

                SoundPatch p;
                if (values.size() >= 2) {
                    p.bank = values[0];
                    p.program = values[1];
                }
                if (values.size() >= 4) {
                    p.portamento = (values[2] == 1);
                    p.portamentoTime = values[3];
                }
                leadSoundsDB[id] = p;
            }
        }
        std::cout << "  -> Sonidos cargados OK." << std::endl;

    } catch (const YAML::Exception& e) {
        std::cerr << "Error YAML Sounds: " << e.what() << std::endl;
    }
}

// ... (Acá siguen las funciones loadDrumStyles y loadAcompStyles igual que antes) ...
std::map<int, DrumStyle> loadDrumStyles(const std::string& filename) {
    std::map<int, DrumStyle> db;
    try {
        std::cout << "Cargando Ritmos: " << filename << std::endl;
        YAML::Node config = YAML::LoadFile(filename);

        for (const auto& styleNode : config) {
            int styleId = styleNode.first.as<int>();
            DrumStyle ds;
            if (styleNode.second["steps"]) ds.steps = styleNode.second["steps"].as<int>();

            for (const auto& content : styleNode.second) {
                std::string key = content.first.as<std::string>();
                if (key == "steps") continue;

                if (key == "fills") {
                    for (const auto& fillNode : content.second) {
                        std::string fKey = fillNode.first.as<std::string>();
                        if (fKey == "resolution") {
                            for (const auto& r : fillNode.second) ds.resolution[r.first.as<int>()] = r.second.as<int>();
                        } else {
                            int fId = fillNode.first.as<int>();
                            for (const auto& tr : fillNode.second) {
                                if (tr.first.as<std::string>() == "resolution") continue;
                                ds.fills[fId].tracks[tr.first.as<int>()] = tr.second.as<std::vector<int>>();
                            }
                        }
                    }
                } else {
                    try {
                        int varId = std::stoi(key);
                        for (const auto& tr : content.second) {
                            ds.variations[varId].tracks[tr.first.as<int>()] = tr.second.as<std::vector<int>>();
                        }
                    } catch (...) {}
                }
            }
            db[styleId] = ds;
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "Error YAML Ritmos: " << e.what() << std::endl;
    }
    return db;
}

std::map<int, AcompStyle> loadAcompStyles(const std::string& filename) {
    std::map<int, AcompStyle> db;
    try {
        std::cout << "Cargando Acomp: " << filename << std::endl;
        YAML::Node config = YAML::LoadFile(filename);

        for (const auto& styleNode : config) {
            int styleId = styleNode.first.as<int>();
            AcompStyle as;

            for (const auto& patNode : styleNode.second) {
                int patId = patNode.first.as<int>();
                AcompPattern ap;
                YAML::Node data = patNode.second;

                if (data["program"]) ap.program = data["program"].as<int>();
                if (data["mode"]) ap.mode = data["mode"].as<std::string>();
                if (data["velocity"]) ap.velocity = data["velocity"].as<int>();
                if (data["steps"]) ap.steps = data["steps"].as<int>();
                if (data["pattern"]) ap.pattern = data["pattern"].as<std::vector<int>>();

                as.patterns[patId] = ap;
            }
            db[styleId] = as;
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "Error YAML Acomp: " << e.what() << std::endl;
    }
    return db;
}
