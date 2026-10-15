#pragma once
#include <string>

int doCommit(const std::string& rawMsg);
int doPush(bool force, const std::string& forcedKey, bool learn);
