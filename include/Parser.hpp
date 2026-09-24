#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "definitions.hpp"

namespace uci {

class Parser {
private:
    struct CommandSpec {
        uint8_t event;
        std::unordered_set<std::string_view> keywords;
    };

    using command_table_t =
        std::unordered_map<std::string_view, CommandSpec>;

    static const command_table_t& commandTable();

    static std::string_view nextWord(
        std::string_view input,
        std::size_t& position
    );

public:
    Parser() = default;
    ~Parser() = default;

    uint8_t parseInputForCommand(
        std::string_view input
    ) const;

    arguments_t parseInputForArguments(
        std::string_view input
    ) const;

    std::pair<uint8_t, arguments_t> parseInput(
        std::string_view input
    ) const;

    std::string getFirstWord(
        const std::string& sentence
    ) const;

    std::string getFirstWord(
        uint8_t index,
        const std::string& sentence
    ) const;
};

}