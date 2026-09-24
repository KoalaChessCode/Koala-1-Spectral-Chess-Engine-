#pragma once

#include <iostream>
#include "definitions.hpp" // in cpp files use "uci/definitions.h"

namespace uci {
void send(std::string res);
void send(std::string command, ::uci::arguments_t args);
}