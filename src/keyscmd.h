#pragma once
#include <string>
#include <vector>

// The `keys` subcommand: manage which identity git_wrapper uses for which
// remote. `args` is everything after the word "keys" (args[0] is the verb;
// empty means "list").
int keysCommand(const std::vector<std::string>& args);
