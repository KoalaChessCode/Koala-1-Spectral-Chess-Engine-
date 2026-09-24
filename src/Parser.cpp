#include "Parser.hpp"

#include <cctype>

#include "events.hpp"

namespace uci {

const Parser::command_table_t& Parser::commandTable() {
    static const command_table_t table{
        {
            "uci",
            CommandSpec{
                event::UCI,
                {}
            }
        },
        {
            "debug",
            CommandSpec{
                event::DEBUG,
                {"on", "off"}
            }
        },
        {
            "isready",
            CommandSpec{
                event::ISREADY,
                {}
            }
        },
        {
            "setoption",
            CommandSpec{
                event::SETOPTION,
                {"name", "value"}
            }
        },
        {
            "register",
            CommandSpec{
                event::REGISTER,
                {"later", "name", "code"}
            }
        },
        {
            "ucinewgame",
            CommandSpec{
                event::UCINEWGAME,
                {}
            }
        },
        {
            "position",
            CommandSpec{
                event::POSITION,
                {"fen", "moves", "startpos"}
            }
        },
        {
            "go",
            CommandSpec{
                event::GO,
                {
                    "searchmoves",
                    "ponder",
                    "wtime",
                    "btime",
                    "winc",
                    "binc",
                    "movestogo",
                    "depth",
                    "nodes",
                    "mate",
                    "movetime",
                    "infinite"
                }
            }
        },
        {
            "perft",
            CommandSpec{
                event::PERFT,
                {"depth", "fen"}
            }
        },
        {
            "stop",
            CommandSpec{
                event::STOP,
                {}
            }
        },
        {
            "ponderhit",
            CommandSpec{
                event::PONDERHIT,
                {}
            }
        },
        {
            "quit",
            CommandSpec{
                event::QUIT,
                {}
            }
        },
        {
            "black",
            CommandSpec{
                event::BLACK,
                {}
            }
        },
        {
            "white",
            CommandSpec{
                event::WHITE,
                {}
            }
        }
    };

    return table;
}

std::string_view Parser::nextWord(
    std::string_view input,
    std::size_t& position
) {
    while (
        position < input.size() &&
        std::isspace(
            static_cast<unsigned char>(input[position])
        )
    ) {
        ++position;
    }

    const std::size_t begin = position;

    while (
        position < input.size() &&
        !std::isspace(
            static_cast<unsigned char>(input[position])
        )
    ) {
        ++position;
    }

    return input.substr(begin, position - begin);
}

uint8_t Parser::parseInputForCommand(
    std::string_view input
) const {
    std::size_t position = 0;
    const std::string_view command = nextWord(input, position);

    const auto& table = commandTable();
    const auto iterator = table.find(command);

    if (iterator == table.end()) {
        return event::NO_MATCHING_COMMAND;
    }

    return iterator->second.event;
}

arguments_t Parser::parseInputForArguments(
    std::string_view input
) const {
    arguments_t arguments;

    std::size_t position = 0;
    const std::string_view command = nextWord(input, position);

    const auto& table = commandTable();
    const auto commandIterator = table.find(command);

    if (commandIterator == table.end()) {
        return arguments;
    }

    const auto& keywords = commandIterator->second.keywords;

    std::string currentKey;
    std::string currentValue;

    while (true) {
        const std::string_view word = nextWord(input, position);

        if (word.empty()) {
            break;
        }

        const bool isKey =
            keywords.find(word) != keywords.end();

        if (isKey) {
            if (!currentKey.empty()) {
                arguments[currentKey] = currentValue;
            }

            currentKey.assign(word);
            currentValue.clear();
            continue;
        }

        /*
         * Jeżeli komenda nie ma żadnych słów kluczowych,
         * ignorujemy jej pozostałe argumenty.
         */
        if (currentKey.empty()) {
            continue;
        }

        if (!currentValue.empty()) {
            currentValue.push_back(' ');
        }

        currentValue.append(word);
    }

    if (!currentKey.empty()) {
        arguments[currentKey] = currentValue;
    }

    return arguments;
}

std::pair<uint8_t, arguments_t> Parser::parseInput(
    std::string_view input
) const {
    const uint8_t command = parseInputForCommand(input);

    if (command == event::NO_MATCHING_COMMAND) {
        return {
            event::NO_MATCHING_COMMAND,
            {}
        };
    }

    return {
        command,
        parseInputForArguments(input)
    };
}

std::string Parser::getFirstWord(
    const std::string& sentence
) const {
    return getFirstWord(0, sentence);
}

std::string Parser::getFirstWord(
    uint8_t index,
    const std::string& sentence
) const {
    if (index >= sentence.size()) {
        return {};
    }

    std::size_t position = index;

    /*
     * Zachowujemy sensowną obsługę spacji także wtedy,
     * gdy index wskazuje na białe znaki.
     */
    while (
        position < sentence.size() &&
        std::isspace(
            static_cast<unsigned char>(sentence[position])
        )
    ) {
        ++position;
    }

    const std::size_t begin = position;

    while (
        position < sentence.size() &&
        !std::isspace(
            static_cast<unsigned char>(sentence[position])
        )
    ) {
        ++position;
    }

    return sentence.substr(begin, position - begin);
}

}