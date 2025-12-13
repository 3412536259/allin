#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include "json.hpp"

bool writeTempConfig(const std::string& jsonStr);
bool validateTempConfig(const nlohmann::json& j);
bool replaceMainConfig();
void cleanupTemp();