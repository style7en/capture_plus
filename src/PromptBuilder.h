#pragma once
#include "Pch.h"

namespace prompts {

std::string aiAnalysis();
std::string aiOcr();
std::string translate(const std::string& targetLanguage, const std::string& text);

}
