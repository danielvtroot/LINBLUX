#pragma once
#include <nlohmann/json.hpp>
#include "game/Rig.h"

namespace loblox::game {
nlohmann::json EncodeAssets(const GameAssets& assets);
// Parses into a candidate; failure never changes destination.
bool DecodeAssets(const nlohmann::json& json, GameAssets& assets, std::string& error);
}
