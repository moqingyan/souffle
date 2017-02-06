#include "GlobalConfig.h"

namespace souffle {


GlobalConfig::GlobalConfig()
    : simple::Table<std::string, std::string>() {}

void GlobalConfig::processArgs() {

    option longNames[mainOptions.size()];
    std::string shortNames = "";
    std::map<const char, const MainOption*> optionTable;
    int i = 0;
    for (const MainOption& opt : mainOptions) {
        optionTable[opt.shortName] = &opt;
        if (!opt.byDefault.empty())
            set(opt.longName, opt.byDefault);
        if (opt.longName == "")
            continue;
        longNames[i] = (option) { opt.longName.c_str(), (!opt.argument.empty()), nullptr, opt.shortName };
        shortNames += opt.shortName;
        if (!opt.argument.empty())
            shortNames += ":";
        ++i;
    }
    longNames[i] = {nullptr, false, nullptr, 0}; // the terminal option, needs to be null

    int c;     /* command-line arguments processing */
    while ((c = getopt_long(argc, argv, shortNames.c_str(), longNames, nullptr)) != EOF) {
        if (c == '?') error();
        auto iter = optionTable.find(c);
        if (iter == optionTable.end()) {
            ASSERT("Default label in getopt switch.");
            abort();
        }
        std::string arg = (optarg) ? std::string(optarg) : std::string();
        if (!iter->second->delimiter.empty()) {
            set(iter->second->longName, get(iter->second->longName) + iter->second->delimiter + arg);
        } else {
            set(iter->second->longName, arg);
        }
    }
}

void GlobalConfig::initialize(int argc, char** argv, const std::string header, const std::string footer, const std::vector<MainOption> mainOptions) {

    this->argc = argc;
    this->argv = argv;
    this->header = header;
    this->footer = footer;
    this->mainOptions = mainOptions;

    processArgs();


}

void GlobalConfig::printHelp(std::ostream& os) {

    // print the header
    os << header;

    // iterate over the options and obtain the maximum name and argument lengths
    int maxLongNameLength = 0, maxArgumentIdLength = 0;
    for (const MainOption& opt : mainOptions) {
        // if it is the main option, do nothing
        if (opt.longName == "") continue;
        // otherwise, proceed with the calculation
        maxLongNameLength = ((int) opt.longName.size() > maxLongNameLength) ? opt.longName.size() : maxLongNameLength;
        maxArgumentIdLength = ((int) opt.argument.size() > maxArgumentIdLength) ? opt.argument.size() : maxArgumentIdLength;
    }

    // iterator over the options and pretty print them, using padding as determined
    // by the maximum name and argument lengths
    int length;
    for (const MainOption& opt : mainOptions) {

        // if it is the main option, do nothing
        if (opt.longName == "") continue;

        // print the short form name and the argument parameter
        length = 0;
        os << "\t";
        if (isalpha(opt.shortName)) {
            os << "-" << opt.shortName;
            if (!opt.argument.empty()) {
                os << "<" << opt.argument << ">";
                length = opt.argument.size() + 2;
            }
        } else {
            os << "  ";
        }

        // pad with empty space for prettiness
        for (; length < maxArgumentIdLength + 2; ++length) os << " ";

        // print the long form name and the argument parameter
        length = 0;
        os << "\t" << "--" << opt.longName;
        if (!opt.argument.empty()) {
            os << "=<" << opt.argument << ">";
            length = opt.argument.size() + 3;
        }

        // again, pad with empty space for prettiness
        for (length += opt.longName.size(); length < maxArgumentIdLength + maxLongNameLength + 3; ++length) os << " ";

        // print the description
        os << "\t" << opt.description << std::endl;

    }

    // print the footer
    os << footer;
}

void GlobalConfig::error() {
    for(int i = 0; i < argc; i++) {
        std::cerr << argv[i] << " ";
    }
    std::cerr << "\nError parsing command-line arguments.\n";
    printHelp(std::cerr);
    exit(1);
}

};
