/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All Rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Global.cpp
 *
 * Defines a configuration environment
 *
 ***********************************************************************/

#include "Global.h"
#include <cassert>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <getopt.h>

namespace souffle {

void MainConfig::processArgs(int argc, char** argv, const std::string& header, const std::string& footer,
        const std::vector<MainOption> mainOptions) {
    constexpr auto ONE_SPACE = " ";
    constexpr auto TWO_SPACES = "  ";
    constexpr auto THREE_SPACES = "   ";

    // construct the help text using the main options
    {
        // create a stream to be 'printed' to
        std::stringstream ss;

        // print the header
        ss << header;

        // compute the maximum length of a line of the help text without including the description
        std::size_t maxLineLengthWithoutDescription = 0;
        {
            // initially compute the maximum line length without the long option, arguments, or description
            std::stringstream lineSchema;
            const auto shortOption = "?";
            const auto longOption = "";
            const auto arguments = "";
            const auto description = "";
            lineSchema << TWO_SPACES << "-" << shortOption << "," << ONE_SPACE << "--" << longOption << "=<"
                       << arguments << ">" << TWO_SPACES << description;
            maxLineLengthWithoutDescription = lineSchema.str().size();
        }
        {
            // then compute the maximum length of the long option plus the description
            std::size_t maxLongOptionPlusArgumentLength = 0;
            for (const MainOption& opt : mainOptions) {
                if (opt.longName.empty()) {
                    continue;
                }
                const auto longOptionPlusArgumentLength = opt.longName.size() + opt.argument.size();
                if (longOptionPlusArgumentLength > maxLongOptionPlusArgumentLength) {
                    maxLongOptionPlusArgumentLength = longOptionPlusArgumentLength;
                }
            }
            maxLineLengthWithoutDescription += maxLongOptionPlusArgumentLength;
        }

        // iterate over the options and pretty print them, using the computed maximum line length without the
        // description
        for (const MainOption& opt : mainOptions) {
            // the current line
            std::stringstream line;

            // if it is the main option, do nothing
            if (opt.longName.empty()) {
                continue;
            }

            // print the short form name and the argument parameter
            line << TWO_SPACES;
            if (isalpha(opt.shortName) != 0) {
                line << "-" << opt.shortName << ",";
            } else {
                line << THREE_SPACES;
            }

            // print the long form name
            line << ONE_SPACE << "--" << opt.longName;

            // print the argument parameter
            if (!opt.argument.empty()) {
                line << "=<" << opt.argument << ">";
            }

            // again, pad with empty space for prettiness
            for (std::size_t lineLength = line.str().size(); lineLength < maxLineLengthWithoutDescription;
                    ++lineLength) {
                line << ONE_SPACE;
            }

            // print the description
            line << opt.description << std::endl;

            ss << line.str();
        }

        // print the footer
        ss << footer;

        // finally, store the help text as a string
        _help = ss.str();
    }

    // use the main options to define the global configuration
    {
        // array of long names for classic getopt processing
        option longNames[mainOptions.size()];
        // string of short names for classic getopt processing
        std::string shortNames = "";
        // table to map the short name to its option
        std::map<const char, const MainOption*> optionTable;
        // counter to be incremented at each loop
        int i = 0;
        // iterate over the options provided
        for (const MainOption& opt : mainOptions) {
            assert(opt.shortName != '?' && "short name for option cannot be '?'");
            // put the option in the table, referenced by its short name
            optionTable[opt.shortName] = &opt;
            // set the default value for the option, if it exists
            if (!opt.byDefault.empty()) {
                set(opt.longName, opt.byDefault);
            }
            // skip the next bit if it is the option for the datalog file
            if (opt.longName.empty()) {
                continue;
            }
            // convert the main option to a plain old getopt option and put it in the array
            longNames[i] = {opt.longName.c_str(), opt.argument.empty() ? 0 : 1, nullptr, opt.shortName};
            // append the short name of the option to the string of short names
            shortNames += opt.shortName;
            // indicating with a ':' if it takes an argument
            if (!opt.argument.empty()) {
                shortNames += ":";
            }
            // increment counter
            ++i;
        }
        // the terminal option, needs to be null
        longNames[i] = {nullptr, 0, nullptr, 0};

        // use getopt to process the arguments given to the command line, with the parameters being the
        // short and long names from above
        int c;
        while ((c = getopt_long(argc, argv, shortNames.c_str(), longNames, nullptr)) != EOF) {
            // case for the unknown option
            if (c == '?') {
                std::cerr << Global::config().help();
                throw std::runtime_error("Error: Unknown command line option.");
            }
            // obtain an iterator to the option in the table referenced by the current short name
            auto iter = optionTable.find(c);
            // case for the unknown option, again
            assert(iter != optionTable.end() && "unexpected case in getopt");
            // define the value for the option in the global configuration as its argument or an empty string
            //  if no argument exists
            std::string arg = optarg != nullptr ? std::string(optarg) : std::string();
            // if the option allows multiple arguments
            if (iter->second->takesMany) {
                // set the value of the option in the global config to the concatenation of its previous
                // value, a space and the current argument
                set(iter->second->longName, get(iter->second->longName) + ' ' + arg);
                // otherwise, set the value of the option in the global config
            } else {
                // but only if it isn't set already
                if (has(iter->second->longName) &&
                        (iter->second->byDefault.empty() ||
                                !has(iter->second->longName, iter->second->byDefault))) {
                    throw std::runtime_error(
                            "Error: Only one argument allowed for option '" + iter->second->longName + "'");
                }
                set(iter->second->longName, arg);
            }
        }
    }

    std::cout << "obtain datalog" << std::endl;
    // obtain the name of the datalog file, and store it in the option with the empty key
    if (argc > 1 && !Global::config().has("help") && !Global::config().has("version")) {
        std::string filename = "";
        // ensure that the optind is less than the total number of arguments
        if (argc > 1 && optind >= argc) {
            std::cerr << Global::config().help();
            throw std::runtime_error("Error: Unknown command line option.");
        }
        // if only one datalog program is allowed
        if (mainOptions[0].longName.empty() && mainOptions[0].takesMany) {
            // set the option in the global config for the main datalog file to that specified by the command
            // line arguments
            set("", std::string(argv[optind]));
            // otherwise, if multiple input filenames are allowed
        } else {
            std::string filenames = "";
            // for each of the command line arguments not associated with an option
            for (; optind < argc; optind++) {
                // append this filename to the concatenated string of filenames
                if (filenames.empty()) {
                    filenames = argv[optind];
                } else {
                    filenames = filenames + " " + std::string(argv[optind]);
                }
            }
            // set the option in the global config for the main datalog file to all those specified by the
            // command line arguments
            set("", filenames);
        }
    }
}
}  // namespace souffle
