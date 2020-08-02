
#include "AstComponentChecker.h"
#include "AstPragmaChecker.h"
#include "AstSemanticChecker.h"
#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstTranslator.h"
#include "AstTypeAnalysis.h"
#include "ComponentInstantiationTransformer.h"
#include "DebugReport.h"
#include "DebugReporter.h"
#include "ErrorReport.h"
#include "Explain.h"
#include "Global.h"
#include "InterpreterEngine.h"
#include "InterpreterProgInterface.h"
#include "ParserDriver.h"
#include "PrecedenceGraph.h"
#include "RamIndexAnalysis.h"
#include "RamLevelAnalysis.h"
#include "RamProgram.h"
#include "RamTransformer.h"
#include "RamTransforms.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "Synthesiser.h"
#include "Util.h"
#include "config.h"
#include "profile/Tui.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <pybind11/pybind11.h>

int executeBinary(const std::string& binaryFilename) {
    assert(!binaryFilename.empty() && "binary filename cannot be blank");

    // check whether the executable exists
    if (!isExecutable(binaryFilename)) {
        throw std::invalid_argument("Generated executable <" + binaryFilename + "> could not be found");
    }

    // run the executable
    if (Global::config().has("library-dir")) {
        std::string ldPath;
        for (const std::string& library : splitString(Global::config().get("library-dir"), ' ')) {
            ldPath += library + ':';
        }
        ldPath.back() = ' ';
        setenv("LD_LIBRARY_PATH", ldPath.c_str(), 1);
    }

    int exitCode = system(binaryFilename.c_str());

    if (Global::config().get("dl-program").empty()) {
        remove(binaryFilename.c_str());
        remove((binaryFilename + ".cpp").c_str());
    }

    // exit with same code as executable
    if (exitCode != EXIT_SUCCESS) {
        return exitCode;
    }
    return 0;
}

/**
 * Compiles the given source file to a binary file.
 */
void compileToBinary(std::string compileCmd, const std::string& sourceFilename) {
    // add source code
    compileCmd += ' ';
    for (const std::string& path : splitString(Global::config().get("library-dir"), ' ')) {
        // The first entry may be blank
        if (path.empty()) {
            continue;
        }
        compileCmd += "-L" + path + ' ';
    }
    for (const std::string& library : splitString(Global::config().get("libraries"), ' ')) {
        // The first entry may be blank
        if (library.empty()) {
            continue;
        }
        compileCmd += "-l" + library + ' ';
    }

    compileCmd += sourceFilename;

    // run executable
    if (system(compileCmd.c_str()) != 0) {
        throw std::invalid_argument("failed to compile C++ source <" + sourceFilename + ">");
    }
}


auto setup_config(){
        std::vector<MainOption> options{{"", 0, "", "", false, ""},
                {"fact-dir", 'F', "DIR", ".", false, "Specify directory for fact files."},
                {"include-dir", 'I', "DIR", ".", true, "Specify directory for include files."},
                {"output-dir", 'D', "DIR", ".", false,
                        "Specify directory for output files. If <DIR> is `-` then stdout is used."},
                {"jobs", 'j', "N", "1", false,
                        "Run interpreter/compiler in parallel using N threads, N=auto for system "
                        "default."},
                {"compile", 'c', "", "", false,
                        "Generate C++ source code, compile to a binary executable, then run this "
                        "executable."},
                {"generate", 'g', "FILE", "", false,
                        "Generate C++ source code for the given Datalog program and write it to "
                        "<FILE>. If <FILE> is `-` then stdout is used."},
                {"swig", 's', "LANG", "", false,
                        "Generate SWIG interface for given language. The values <LANG> accepts is java and "
                        "python. "},
                {"library-dir", 'L', "DIR", "", true, "Specify directory for library files."},
                {"libraries", 'l', "FILE", "", true, "Specify libraries."},
                {"no-warn", 'w', "", "", false, "Disable warnings."},
                {"magic-transform", 'm', "RELATIONS", "", false,
                        "Enable magic set transformation changes on the given relations, use '*' "
                        "for all."},
                {"macro", 'M', "MACROS", "", false, "Set macro definitions for the pre-processor"},
                {"disable-transformers", 'z', "TRANSFORMERS", "", false,
                        "Disable the given AST transformers."},
                {"dl-program", 'o', "FILE", "", false,
                        "Generate C++ source code, written to <FILE>, and compile this to a "
                        "binary executable (without executing it)."},
                {"live-profile", '\2', "", "", false, "Enable live profiling."},
                {"profile", 'p', "FILE", "", false, "Enable profiling, and write profile data to <FILE>."},
                {"profile-use", 'u', "FILE", "", false,
                        "Use profile log-file <FILE> for profile-guided optimization."},
                {"debug-report", 'r', "/Users/moqingyan/Desktop/Research/example/debug_report", "", true, "Write HTML debug report to <FILE>."},
                {"pragma", 'P', "OPTIONS", "", false, "Set pragma options."},
                {"provenance", 't', "[ none | explain | explore | subtreeHeights ]", "", false,
                        "Enable provenance instrumentation and interaction."},
                {"verbose", 'v', "", "", false, "Verbose output."},
                {"version", '\3', "", "", false, "Version."},
                {"show", '\4',
                        "[ parse-errors | precedence-graph | scc-graph | transformed-datalog | "
                        "transformed-ram | type-analysis ]",
                        "", false, "Print selected program information."},
                {"parse-errors", '\5', "", "", false, "Show parsing errors, if any, then exit."},
                {"help", 'h', "", "", false, "Display this help message."}};
        
        char * argv[1];
        argv[0] = "/Users/moqingyan/Desktop/Research/souffle/src/souffle";
        Global::config().processArgs(1, argv, "", "", options);
}
auto get_ast_transformer(){
    // Magic-Set pipeline
    auto magicPipeline = std::make_unique<ConditionalTransformer>(Global::config().has("magic-transform"),
            std::make_unique<PipelineTransformer>(std::make_unique<NormaliseConstraintsTransformer>(),
                    std::make_unique<MagicSetTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
                    std::make_unique<RemoveRelationCopiesTransformer>(),
                    std::make_unique<RemoveEmptyRelationsTransformer>(),
                    std::make_unique<RemoveRedundantRelationsTransformer>()));

    // Equivalence pipeline
    auto equivalencePipeline =
            std::make_unique<PipelineTransformer>(std::make_unique<MinimiseProgramTransformer>(),
                    std::make_unique<RemoveRelationCopiesTransformer>(),
                    std::make_unique<RemoveEmptyRelationsTransformer>(),
                    std::make_unique<RemoveRedundantRelationsTransformer>());

    // Partitioning pipeline
    auto partitionPipeline =
            std::make_unique<PipelineTransformer>(std::make_unique<NameUnnamedVariablesTransformer>(),
                    std::make_unique<PartitionBodyLiteralsTransformer>(),
                    std::make_unique<ReplaceSingletonVariablesTransformer>());

    // Provenance pipeline
    auto provenancePipeline = std::make_unique<PipelineTransformer>(std::make_unique<ConditionalTransformer>(
            Global::config().has("provenance"), std::make_unique<ProvenanceTransformer>()));

    // Main pipeline
    auto pipeline = std::make_unique<PipelineTransformer>(std::make_unique<AstComponentChecker>(),
            std::make_unique<ComponentInstantiationTransformer>(),
            std::make_unique<UniqueAggregationVariablesTransformer>(),
            std::make_unique<AstUserDefinedFunctorsTransformer>(),
            std::make_unique<PolymorphicObjectsTransformer>(), std::make_unique<AstSemanticChecker>(),
            std::make_unique<RemoveTypecastsTransformer>(),
            std::make_unique<RemoveBooleanConstraintsTransformer>(),
            std::make_unique<ResolveAliasesTransformer>(), std::make_unique<MinimiseProgramTransformer>(),
            std::make_unique<InlineRelationsTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
            std::make_unique<RemoveRedundantRelationsTransformer>(),
            std::make_unique<RemoveRelationCopiesTransformer>(),
            std::make_unique<RemoveEmptyRelationsTransformer>(),
            std::make_unique<ReplaceSingletonVariablesTransformer>(),
            std::make_unique<FixpointTransformer>(
                    std::make_unique<PipelineTransformer>(std::make_unique<ReduceExistentialsTransformer>(),
                            std::make_unique<RemoveRedundantRelationsTransformer>())),
            std::make_unique<RemoveRelationCopiesTransformer>(), std::move(partitionPipeline),
            std::make_unique<FixpointTransformer>(std::make_unique<MinimiseProgramTransformer>()),
            std::make_unique<RemoveRelationCopiesTransformer>(),
            std::make_unique<ReorderLiteralsTransformer>(),
            std::make_unique<PipelineTransformer>(std::make_unique<ResolveAliasesTransformer>(),
                    std::make_unique<MaterializeAggregationQueriesTransformer>()),
            std::make_unique<RemoveRedundantSumsTransformer>(),
            std::make_unique<RemoveEmptyRelationsTransformer>(),
            std::make_unique<ReorderLiteralsTransformer>(), std::move(magicPipeline),
            std::make_unique<AstExecutionPlanChecker>(), std::move(provenancePipeline));

    return pipeline;
}

auto get_ram_transformer(){
    std::unique_ptr<RamTransformer> ramTransform = std::make_unique<RamTransformerSequence>(
            std::make_unique<RamLoopTransformer>(
                    std::make_unique<RamTransformerSequence>(std::make_unique<ExpandFilterTransformer>(),
                            std::make_unique<HoistConditionsTransformer>(),
                            std::make_unique<MakeIndexTransformer>())),
            std::make_unique<IfConversionTransformer>(), std::make_unique<ChoiceConversionTransformer>(),
            std::make_unique<CollapseFiltersTransformer>(), std::make_unique<TupleIdTransformer>(),
            std::make_unique<RamLoopTransformer>(std::make_unique<RamTransformerSequence>(
                    std::make_unique<HoistAggregateTransformer>(), std::make_unique<TupleIdTransformer>())),
            std::make_unique<ExpandFilterTransformer>(), std::make_unique<HoistConditionsTransformer>(),
            std::make_unique<CollapseFiltersTransformer>(),
            std::make_unique<EliminateDuplicatesTransformer>(),
            std::make_unique<ReorderConditionsTransformer>(),
            std::make_unique<RamLoopTransformer>(std::make_unique<ReorderFilterBreak>()),
            std::make_unique<RamConditionalTransformer>(
                    // job count of 0 means all cores are used.
                    []() -> bool { return std::stoi(Global::config().get("jobs")) != 1; },
                    std::make_unique<ParallelTransformer>()),
            std::make_unique<ReportIndexTransformer>());

    return ramTransform;
}

// parse the AST from the string code
std::unique_ptr<AstTranslationUnit> get_ast(std::string code, DebugReport debugReport, ErrorReport errReport){
    std::unique_ptr<AstTranslationUnit> astTranslationUnit =
            ParserDriver::parseTranslationUnit(code, errReport, debugReport);
    return astTranslationUnit;
}

int check_ast_err(const std::unique_ptr<AstTranslationUnit> &astTranslationUnit){
    if (astTranslationUnit->getErrorReport().getNumErrors() != 0) {
        std::cerr << astTranslationUnit->getErrorReport();
        std::cerr << "During AST phase: " << std::to_string(astTranslationUnit->getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        return 1;
    }
    else {
            std::cout << "No Error in AST phase" << std::endl;
    }
    return 0;
}

int check_ram_err(const std::unique_ptr<RamTranslationUnit> &ramTranslationUnit){
    std::cout << "Start RAM Phase" << std::endl;
    if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
        std::cerr << ramTranslationUnit->getErrorReport();
        std::cerr << "During Ram phase: " << std::to_string(ramTranslationUnit->getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        return 1;
    }
    else {
            std::cout << "No Error in RAM phase" << std::endl;
    }
    return 0;
}

int execute(std::string code, bool get_prov){

    DebugReport debugReport;
    ErrorReport errReport(true); // no-warning

    // Generate AST and do AST transformation
    std::unique_ptr<AstTranslationUnit> astTranslationUnit = get_ast(code, debugReport, errReport);
    auto astTransformer = get_ast_transformer();
    astTransformer->apply(*astTranslationUnit);
    check_ast_err(astTranslationUnit);
    

    // Translate AST to RAM 
    // TODO: change translate unit to accept fact strings
    
    std::cout << "RAM: start phase: astTranslationUnit " << astTranslationUnit.get() << std::endl;
    std::unique_ptr<RamTranslationUnit> ramTranslationUnit =
            AstTranslator().translateUnit(*astTranslationUnit);

    std::cout << "RAM: finish translate unit" << std::endl;
    auto ramTransformer = get_ram_transformer();
    ramTransformer->apply(*ramTranslationUnit);
    std::cout << "RAM: finish transform unit" << std::endl;

    check_ram_err(ramTranslationUnit);
    
    // Execute RAM
    std::unique_ptr<InterpreterEngine> interpreter(
                    std::make_unique<InterpreterEngine>(*ramTranslationUnit));
    interpreter->executeMain();

    // char * argv[2];
    // argv[0] = "/Users/moqingyan/Desktop/Research/souffle/src/souffle";
    // argv[1] = "qa.dl";
    // int argc=2;

 /* Time taking for overall runtime */
//     auto souffle_start = std::chrono::high_resolution_clock::now();

//     /* have all to do with command line arguments in its own scope, as these are accessible through the global
//      * configuration only */
//     try {
//         std::stringstream header;
//         header << "============================================================================" << std::endl;
//         header << "souffle -- A datalog engine." << std::endl;
//         header << "Usage: souffle [OPTION] FILE." << std::endl;
//         header << "----------------------------------------------------------------------------" << std::endl;
//         header << "Options:" << std::endl;

//         std::stringstream footer;
//         footer << "----------------------------------------------------------------------------" << std::endl;
//         footer << "Version: " << PACKAGE_VERSION << "" << std::endl;
//         footer << "----------------------------------------------------------------------------" << std::endl;
//         footer << "Copyright (c) 2016-20 The Souffle Developers." << std::endl;
//         footer << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
//         footer << "All rights reserved." << std::endl;
//         footer << "============================================================================" << std::endl;

//         // command line options, the environment will be filled with the arguments passed to them, or
//         // the empty string if they take none
//         // main option, the datalog program itself, has an empty key
//         std::vector<MainOption> options{{"", 0, "", "", false, ""},
//                 {"fact-dir", 'F', "DIR", ".", false, "Specify directory for fact files."},
//                 {"include-dir", 'I', "DIR", ".", true, "Specify directory for include files."},
//                 {"output-dir", 'D', "DIR", ".", false,
//                         "Specify directory for output files. If <DIR> is `-` then stdout is used."},
//                 {"jobs", 'j', "N", "1", false,
//                         "Run interpreter/compiler in parallel using N threads, N=auto for system "
//                         "default."},
//                 {"compile", 'c', "", "", false,
//                         "Generate C++ source code, compile to a binary executable, then run this "
//                         "executable."},
//                 {"generate", 'g', "FILE", "", false,
//                         "Generate C++ source code for the given Datalog program and write it to "
//                         "<FILE>. If <FILE> is `-` then stdout is used."},
//                 {"swig", 's', "LANG", "", false,
//                         "Generate SWIG interface for given language. The values <LANG> accepts is java and "
//                         "python. "},
//                 {"library-dir", 'L', "DIR", "", true, "Specify directory for library files."},
//                 {"libraries", 'l', "FILE", "", true, "Specify libraries."},
//                 {"no-warn", 'w', "", "", false, "Disable warnings."},
//                 {"magic-transform", 'm', "RELATIONS", "", false,
//                         "Enable magic set transformation changes on the given relations, use '*' "
//                         "for all."},
//                 {"macro", 'M', "MACROS", "", false, "Set macro definitions for the pre-processor"},
//                 {"disable-transformers", 'z', "TRANSFORMERS", "", false,
//                         "Disable the given AST transformers."},
//                 {"dl-program", 'o', "FILE", "", false,
//                         "Generate C++ source code, written to <FILE>, and compile this to a "
//                         "binary executable (without executing it)."},
//                 {"live-profile", '\2', "", "", false, "Enable live profiling."},
//                 {"profile", 'p', "FILE", "", false, "Enable profiling, and write profile data to <FILE>."},
//                 {"profile-use", 'u', "FILE", "", false,
//                         "Use profile log-file <FILE> for profile-guided optimization."},
//                 {"debug-report", 'r', "FILE", "", false, "Write HTML debug report to <FILE>."},
//                 {"pragma", 'P', "OPTIONS", "", false, "Set pragma options."},
//                 {"provenance", 't', "[ none | explain | explore | subtreeHeights ]", "", false,
//                         "Enable provenance instrumentation and interaction."},
//                 {"verbose", 'v', "", "", false, "Verbose output."},
//                 {"version", '\3', "", "", false, "Version."},
//                 {"show", '\4',
//                         "[ parse-errors | precedence-graph | scc-graph | transformed-datalog | "
//                         "transformed-ram | type-analysis ]",
//                         "", false, "Print selected program information."},
//                 {"parse-errors", '\5', "", "", false, "Show parsing errors, if any, then exit."},
//                 {"help", 'h', "", "", false, "Display this help message."}};
//         Global::config().processArgs(margc, margv, header.str(), footer.str(), options);
//         Global::config().print(std::cout);

//         // std::cout << "Ignore config" << std::endl;
//         // ------ command line arguments -------------

//         // Take in pragma options from the command line
//         if (Global::config().has("pragma")) {
//             std::vector<std::string> configOptions = splitString(Global::config().get("pragma"), ';');
//             for (const std::string& option : configOptions) {
//                 size_t splitPoint = option.find(':');

//                 std::string optionName = option.substr(0, splitPoint);
//                 std::string optionValue = (splitPoint == std::string::npos)
//                                                   ? ""
//                                                   : option.substr(splitPoint + 1, option.length());

//                 if (!Global::config().has(optionName)) {
//                     Global::config().set(optionName, optionValue);
//                 }
//             }
//         }

//         /* for the version option, if given print the version text then exit */
//         std::cout << "check version" << std::endl;
//         if (Global::config().has("version")) {
//             std::cout << "is version" << std::endl;
//             std::cout << "Souffle: " << PACKAGE_VERSION;
//             std::cout << "(" << RAM_DOMAIN_SIZE << "bit Domains)";
//             std::cout << std::endl;
//             std::cout << "Copyright (c) 2016-19 The Souffle Developers." << std::endl;
//             std::cout << "Copyright (c) 2013-16 Oracle and/or its affiliates." << std::endl;
//             return 0;
//         }
//         Global::config().set("version", PACKAGE_VERSION);

//         /* for the help option, if given simply print the help text then exit */
//         if (!Global::config().has("") || Global::config().has("help")) {
//             std::cout << "is helper" << std::endl;
//             std::cout << Global::config().help();
//             return 0;
//         }
        
//         std::cout << "not helper" << std::endl;
//         /* check that datalog program exists */
//         if (!existFile(Global::config().get(""))) {
//             throw std::runtime_error("cannot open file " + std::string(Global::config().get("")));
//         }

//         /* for the jobs option, to determine the number of threads used */
// #ifdef _OPENMP
//         std::cout << "open mp" << std::endl;
//         if (isNumber(Global::config().get("jobs").c_str())) {
//             if (std::stoi(Global::config().get("jobs")) < 1) {
//                 throw std::runtime_error("-j/--jobs may only be set to 'auto' or an integer greater than 0.");
//             }
//         } else {
//             if (!Global::config().has("jobs", "auto")) {
//                 throw std::runtime_error("-j/--jobs may only be set to 'auto' or an integer greater than 0.");
//             }
//             Global::config().set("jobs", "0");
//         }
// #else
//         // Check that -j option has not been changed from the default
//         std::cout << "check job" << std::endl;
//         if (Global::config().get("jobs") != "1") {
//             std::cerr << "\nThis installation of Souffle does not support concurrent jobs.\n";
//         }
// #endif

//         std::cout << "check output exist" <<std::endl;
//         /* if an output directory is given, check it exists */
//         if (Global::config().has("output-dir") && !Global::config().has("output-dir", "-") &&
//                 !existDir(Global::config().get("output-dir")) &&
//                 !(Global::config().has("generate") ||
//                         (Global::config().has("dl-program") && !Global::config().has("compile")))) {
//             throw std::runtime_error(
//                     "output directory " + Global::config().get("output-dir") + " does not exists");
//         }

//         /* collect all input directories for the c pre-processor */
//         if (Global::config().has("include-dir")) {
//             std::string currentInclude = "";
//             std::string allIncludes = "";
//             for (const char& ch : Global::config().get("include-dir")) {
//                 if (ch == ' ') {
//                     if (!existDir(currentInclude)) {
//                         throw std::runtime_error("include directory " + currentInclude + " does not exists");
//                     } else {
//                         allIncludes += " -I";
//                         allIncludes += currentInclude;
//                         currentInclude = "";
//                     }
//                 } else {
//                     currentInclude += ch;
//                 }
//             }
//             allIncludes += " -I" + currentInclude;
//             Global::config().set("include-dir", allIncludes);
//         }

//         /* collect all macro definitions for the pre-processor */
//         if (Global::config().has("macro")) {
//             std::string currentMacro = "";
//             std::string allMacros = "";
//             for (const char& ch : Global::config().get("macro")) {
//                 if (ch == ' ') {
//                     allMacros += " -D";
//                     allMacros += currentMacro;
//                     currentMacro = "";
//                 } else {
//                     currentMacro += ch;
//                 }
//             }
//             allMacros += " -D" + currentMacro;
//             Global::config().set("macro", allMacros);
//         }

//         /* turn on compilation of executables */
//         if (Global::config().has("dl-program")) {
//             Global::config().set("compile");
//         }

//         if (Global::config().has("live-profile") && !Global::config().has("profile")) {
//             Global::config().set("profile");
//         }
//     } catch (std::exception& e) {
//         std::cerr << e.what() << std::endl;
//         return 1;
//     }

//     /**
//      * Ensure that code generation is enabled if using SWIG interface option.
//      */
//     if (Global::config().has("swig") && !Global::config().has("generate")) {
//         Global::config().set("generate", simpleName(Global::config().get("")));
//     }

//     // ------ start souffle -------------

//     std::string souffleExecutable = which(margv[0]);
//     std::cout << "souffle executable: " << souffleExecutable << std::endl;

//     if (souffleExecutable.empty()) {
//         throw std::runtime_error("failed to determine souffle executable path");
//     }

//     /* Create the pipe to establish a communication between cpp and souffle */
//     std::string cmd = ::which("mcpp");

//     if (!isExecutable(cmd)) {
//         throw std::runtime_error("failed to locate mcpp pre-processor");
//     }

//     cmd += " -e utf8 -W0 " + Global::config().get("include-dir");
//     if (Global::config().has("macro")) {
//         cmd += " " + Global::config().get("macro");
//     }
//     // Add RamDomain size as a macro
//     cmd += " -DRAM_DOMAIN_SIZE=" + std::to_string(RAM_DOMAIN_SIZE);
//     cmd += " " + Global::config().get("");
//     FILE* in = popen(cmd.c_str(), "r");
//     std::cout << "cmd: " << cmd << std::endl;

//     /* Time taking for parsing */
//     auto parser_start = std::chrono::high_resolution_clock::now();

//     // ------- parse program -------------

//     // parse file
//     //ErrorReport errReport(Global::config().has("no-warn"));
//     ErrorReport errReport(true); // no-warning
//     DebugReport debugReport;
//     std::unique_ptr<AstTranslationUnit> astTranslationUnit =
//             ParserDriver::parseTranslationUnit("<stdin>", in, errReport, debugReport);

// //     std::unique_ptr<AstTranslationUnit> astTranslationUnit = get_ast(code, debugReport, errReport);
//     // close input pipe
//     int preprocessor_status = pclose(in);
//     if (preprocessor_status == -1) {
//         perror(nullptr);
//         throw std::runtime_error("failed to close pre-processor pipe");
//     }

//     /* Report run-time of the parser if verbose flag is set */
//     if (Global::config().has("verbose")) {
//         auto parser_end = std::chrono::high_resolution_clock::now();
//         std::cout << "Parse Time: " << std::chrono::duration<double>(parser_end - parser_start).count()
//                   << "sec\n";
//     }

//     if (Global::config().get("show") == "parse-errors") {
//         std::cout << astTranslationUnit->getErrorReport();
//         return astTranslationUnit->getErrorReport().getNumErrors();
//     }

//     // ------- check for parse errors -------------
//     if (astTranslationUnit->getErrorReport().getNumErrors() != 0) {
//         std::cerr << astTranslationUnit->getErrorReport();
//         std::cerr << std::to_string(astTranslationUnit->getErrorReport().getNumErrors()) +
//                              " errors generated, evaluation aborted"
//                   << std::endl;
//         return 1;
//     }
    
//     std::cout << "Parse Finished" << std::endl;
//     // ------- rewriting / optimizations -------------

//     /* set up additional global options based on pragma declaratives */
//     (std::make_unique<AstPragmaChecker>())->apply(*astTranslationUnit);

//     /* construct the transformation pipeline */

//     // Magic-Set pipeline
//     auto magicPipeline = std::make_unique<ConditionalTransformer>(Global::config().has("magic-transform"),
//             std::make_unique<PipelineTransformer>(std::make_unique<NormaliseConstraintsTransformer>(),
//                     std::make_unique<MagicSetTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
//                     std::make_unique<RemoveRelationCopiesTransformer>(),
//                     std::make_unique<RemoveEmptyRelationsTransformer>(),
//                     std::make_unique<RemoveRedundantRelationsTransformer>()));

//     // Equivalence pipeline
//     auto equivalencePipeline =
//             std::make_unique<PipelineTransformer>(std::make_unique<MinimiseProgramTransformer>(),
//                     std::make_unique<RemoveRelationCopiesTransformer>(),
//                     std::make_unique<RemoveEmptyRelationsTransformer>(),
//                     std::make_unique<RemoveRedundantRelationsTransformer>());

//     // Partitioning pipeline
//     auto partitionPipeline =
//             std::make_unique<PipelineTransformer>(std::make_unique<NameUnnamedVariablesTransformer>(),
//                     std::make_unique<PartitionBodyLiteralsTransformer>(),
//                     std::make_unique<ReplaceSingletonVariablesTransformer>());

//     // Provenance pipeline
//     auto provenancePipeline = std::make_unique<PipelineTransformer>(std::make_unique<ConditionalTransformer>(
//             Global::config().has("provenance"), std::make_unique<ProvenanceTransformer>()));

//     // Main pipeline
//     auto pipeline = std::make_unique<PipelineTransformer>(std::make_unique<AstComponentChecker>(),
//             std::make_unique<ComponentInstantiationTransformer>(),
//             std::make_unique<UniqueAggregationVariablesTransformer>(),
//             std::make_unique<AstUserDefinedFunctorsTransformer>(),
//             std::make_unique<PolymorphicObjectsTransformer>(), std::make_unique<AstSemanticChecker>(),
//             std::make_unique<RemoveTypecastsTransformer>(),
//             std::make_unique<RemoveBooleanConstraintsTransformer>(),
//             std::make_unique<ResolveAliasesTransformer>(), std::make_unique<MinimiseProgramTransformer>(),
//             std::make_unique<InlineRelationsTransformer>(), std::make_unique<ResolveAliasesTransformer>(),
//             std::make_unique<RemoveRedundantRelationsTransformer>(),
//             std::make_unique<RemoveRelationCopiesTransformer>(),
//             std::make_unique<RemoveEmptyRelationsTransformer>(),
//             std::make_unique<ReplaceSingletonVariablesTransformer>(),
//             std::make_unique<FixpointTransformer>(
//                     std::make_unique<PipelineTransformer>(std::make_unique<ReduceExistentialsTransformer>(),
//                             std::make_unique<RemoveRedundantRelationsTransformer>())),
//             std::make_unique<RemoveRelationCopiesTransformer>(), std::move(partitionPipeline),
//             std::make_unique<FixpointTransformer>(std::make_unique<MinimiseProgramTransformer>()),
//             std::make_unique<RemoveRelationCopiesTransformer>(),
//             std::make_unique<ReorderLiteralsTransformer>(),
//             std::make_unique<PipelineTransformer>(std::make_unique<ResolveAliasesTransformer>(),
//                     std::make_unique<MaterializeAggregationQueriesTransformer>()),
//             std::make_unique<RemoveRedundantSumsTransformer>(),
//             std::make_unique<RemoveEmptyRelationsTransformer>(),
//             std::make_unique<ReorderLiteralsTransformer>(), std::move(magicPipeline),
//             std::make_unique<AstExecutionPlanChecker>(), std::move(provenancePipeline));

//     // Disable unwanted transformations
//     if (Global::config().has("disable-transformers")) {
//         std::vector<std::string> givenTransformers =
//                 splitString(Global::config().get("disable-transformers"), ',');
//         pipeline->disableTransformers(
//                 std::set<std::string>(givenTransformers.begin(), givenTransformers.end()));
//     }

//     // Set up the debug report if necessary
//     if (Global::config().has("debug-report")) {
//         auto parser_end = std::chrono::high_resolution_clock::now();
//         std::stringstream ss;

//         // Add current time
//         std::time_t time = std::time(nullptr);
//         ss << "Executed at ";
//         ss << std::put_time(std::localtime(&time), "%F %T") << "\n";

//         // Add config
//         ss << "(\n";
//         ss << join(Global::config().data(), ",\n", [](std::ostream& out, const auto& arg) {
//             out << "  \"" << arg.first << "\" -> \"" << arg.second << '"';
//         });
//         ss << "\n)";

//         debugReport.addSection("Configuration", "Configuration", ss.str());

//         // Add parsing runtime
//         std::string runtimeStr =
//                 "(" + std::to_string(std::chrono::duration<double>(parser_end - parser_start).count()) + "s)";
//         debugReport.addSection("Parsing", "Parsing " + runtimeStr, "");

//         pipeline->setDebugReport();
//     }

//     // Toggle pipeline verbosity
//     pipeline->setVerbosity(Global::config().has("verbose"));

//     // Apply all the transformations
//     pipeline->apply(*astTranslationUnit);

//     if (Global::config().has("show")) {
//         // Output the transformed datalog and return
//         if (Global::config().get("show") == "transformed-datalog") {
//             std::cout << *astTranslationUnit->getProgram() << std::endl;
//             return 0;
//         }

//         // Output the precedence graph in graphviz dot format and return
//         if (Global::config().get("show") == "precedence-graph") {
//             astTranslationUnit->getAnalysis<PrecedenceGraph>()->print(std::cout);
//             std::cout << std::endl;
//             return 0;
//         }

//         // Output the scc graph in graphviz dot format and return
//         if (Global::config().get("show") == "scc-graph") {
//             astTranslationUnit->getAnalysis<SCCGraph>()->print(std::cout);
//             std::cout << std::endl;
//             return 0;
//         }

//         // Output the type analysis
//         if (Global::config().get("show") == "type-analysis") {
//             astTranslationUnit->getAnalysis<TypeAnalysis>()->print(std::cout);
//             std::cout << std::endl;
//             return 0;
//         }
//     }

//     // ------- execution -------------
//     /* translate AST to RAM */
//     debugReport.startSection();
//     std::unique_ptr<RamTranslationUnit> ramTranslationUnit =
//             AstTranslator().translateUnit(*astTranslationUnit);
//     debugReport.endSection("ast-to-ram", "Translate AST to RAM");

//     std::unique_ptr<RamTransformer> ramTransform = std::make_unique<RamTransformerSequence>(
//             std::make_unique<RamLoopTransformer>(
//                     std::make_unique<RamTransformerSequence>(std::make_unique<ExpandFilterTransformer>(),
//                             std::make_unique<HoistConditionsTransformer>(),
//                             std::make_unique<MakeIndexTransformer>())),
//             std::make_unique<IfConversionTransformer>(), std::make_unique<ChoiceConversionTransformer>(),
//             std::make_unique<CollapseFiltersTransformer>(), std::make_unique<TupleIdTransformer>(),
//             std::make_unique<RamLoopTransformer>(std::make_unique<RamTransformerSequence>(
//                     std::make_unique<HoistAggregateTransformer>(), std::make_unique<TupleIdTransformer>())),
//             std::make_unique<ExpandFilterTransformer>(), std::make_unique<HoistConditionsTransformer>(),
//             std::make_unique<CollapseFiltersTransformer>(),
//             std::make_unique<EliminateDuplicatesTransformer>(),
//             std::make_unique<ReorderConditionsTransformer>(),
//             std::make_unique<RamLoopTransformer>(std::make_unique<ReorderFilterBreak>()),
//             std::make_unique<RamConditionalTransformer>(
//                     // job count of 0 means all cores are used.
//                     []() -> bool { return std::stoi(Global::config().get("jobs")) != 1; },
//                     std::make_unique<ParallelTransformer>()),
//             std::make_unique<ReportIndexTransformer>());

//     ramTransform->apply(*ramTranslationUnit);
//     if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
//         std::cerr << ramTranslationUnit->getErrorReport();
//     }

//     // Output the transformed RAM program and return
//     if (Global::config().get("show") == "transformed-ram") {
//         std::cout << ramTranslationUnit->getProgram();
//         return 0;
//     }

//     try {
//         if (!Global::config().has("compile") && !Global::config().has("dl-program") &&
//                 !Global::config().has("generate") && !Global::config().has("swig")) {
//             // ------- interpreter -------------


//             std::thread profiler;
//             // Start up profiler if needed
//             if (Global::config().has("live-profile") && !Global::config().has("compile")) {
//                 profiler = std::thread([]() { profile::Tui().runProf(); });
//             }

//             // configure and execute interpreter
//             std::unique_ptr<InterpreterEngine> interpreter(
//                     std::make_unique<InterpreterEngine>(*ramTranslationUnit));
//             interpreter->executeMain();
//             // If the profiler was started, join back here once it exits.
//             if (profiler.joinable()) {
//                 profiler.join();
//             }
//             if (Global::config().has("provenance")) {
//                 // Test for bugged combination of provenance, interpreted souffle, and concurrency
//                 if (Global::config().get("jobs") != "1") {
//                     throw std::runtime_error("Provenance is not supported with parallel interpreted mode");
//                 }

//                 // only run explain interface if interpreted
//                 InterpreterProgInterface interface(*interpreter);
//                 std::string explain_str = "explain";
//                 std::string prov = Global::config().get("provenance");
//                 std::size_t found = prov.find(explain_str);

//                 auto split = [](std::string input, const std::string &delim) {
//                     std::vector<std::string> target_ls;
//                     std::size_t pos = 0;
//                     while ((pos = input.find(delim)) != std::string::npos) {
//                         std::string target = input.substr(0, pos);
//                         if (!target.empty()) target_ls.push_back(target);
//                         std::cout << target << std::endl;
//                         input.erase(0, pos + delim.length());
//                     }
//                     if (!input.empty()) target_ls.push_back(input);
//                     return target_ls;
//                 };

//                 if ( (found != std::string::npos) || Global::config().get("provenance") == "subtreeHeights") {
//                     auto target_ls = split(prov.substr(found + explain_str.size()), ",");
//                     std::cout << "prov: " << prov << ", target: " << target_ls << std::endl;

//                     for (auto target : target_ls) {
//                         std::cout << "Explaining target " << target << std::endl;
//                         explain(interface, false, Global::config().get("provenance") == "subtreeHeights", "target(" + target + ")");
//                     }
//                 } else if (Global::config().get("provenance") == "explore") {
//                     explain(interface, true, false);
//                 }
//             }
//         } else {
//             // ------- compiler -------------
//             std::unique_ptr<Synthesiser> synthesiser = std::make_unique<Synthesiser>(*ramTranslationUnit);

//             // Find the base filename for code generation and execution
//             std::string baseFilename;
//             if (Global::config().has("dl-program")) {
//                 baseFilename = Global::config().get("dl-program");
//             } else if (Global::config().has("generate")) {
//                 baseFilename = Global::config().get("generate");

//                 // trim .cpp extension if it exists
//                 if (baseFilename.size() >= 4 && baseFilename.substr(baseFilename.size() - 4) == ".cpp") {
//                     baseFilename = baseFilename.substr(0, baseFilename.size() - 4);
//                 }
//             } else {
//                 baseFilename = tempFile();
//             }
//             if (baseName(baseFilename) == "/" || baseName(baseFilename) == ".") {
//                 baseFilename = tempFile();
//             }

//             std::string baseIdentifier = identifier(simpleName(baseFilename));
//             std::string sourceFilename = baseFilename + ".cpp";

//             bool withSharedLibrary;
//             const bool emitToStdOut = Global::config().has("generate", "-");
//             if (emitToStdOut)
//                 synthesiser->generateCode(std::cout, baseIdentifier, withSharedLibrary);
//             else {
//                 std::ofstream os{sourceFilename};
//                 synthesiser->generateCode(os, baseIdentifier, withSharedLibrary);
//             }

//             if (withSharedLibrary) {
//                 if (!Global::config().has("libraries")) {
//                     Global::config().set("libraries", "functors");
//                 }
//                 if (!Global::config().has("library-dir")) {
//                     Global::config().set("library-dir", ".");
//                 }
//             }

//             auto findCompileCmd = [&] {
//                 auto cmd = ::findTool("souffle-compile", souffleExecutable, ".");
//                 /* Fail if a souffle-compile executable is not found */
//                 if (!isExecutable(cmd)) {
//                     throw std::runtime_error("failed to locate souffle-compile");
//                 }
//                 return cmd;
//             };

//             if (Global::config().has("swig")) {
//                 auto compileCmd = findCompileCmd() + " -s " + Global::config().get("swig") + " ";
//                 compileToBinary(compileCmd, sourceFilename);
//             } else if (Global::config().has("compile")) {
//                 auto start = std::chrono::high_resolution_clock::now();
//                 compileToBinary(findCompileCmd(), sourceFilename);
//                 /* Report overall run-time in verbose mode */
//                 if (Global::config().has("verbose")) {
//                     auto end = std::chrono::high_resolution_clock::now();
//                     std::cout << "Compilation Time: " << std::chrono::duration<double>(end - start).count()
//                               << "sec\n";
//                 }
//                 // run compiled C++ program if requested.
//                 if (!Global::config().has("dl-program") && !Global::config().has("swig")) {
//                     executeBinary(baseFilename);
//                 }
//             }
//         }
//     } catch (std::exception& e) {
//         std::cerr << e.what() << std::endl;
//         return 1;
//     }

//     /* Report overall run-time in verbose mode */
//     if (Global::config().has("verbose")) {
//         auto souffle_end = std::chrono::high_resolution_clock::now();
//         std::cout << "Total Time: " << std::chrono::duration<double>(souffle_end - souffle_start).count()
//                   << "sec\n";
//     }

//     return 0;

}

// int main () {
//         std::string datalog_example = ".type ObjectID \n .type Word \n \n .decl name(x: Word, y:ObjectID) \n name(\"apple\", \"1\"). \n name(\"banana\", \"2\"). \n .decl target(o:ObjectID)\n .output target\n target(O) :- name(\"banana\", O).";
//         execute(datalog_example, false);
// }

PYBIND11_MODULE(PySouffle, m) {
    m.doc() = "pybind11 example plugin"; // optional module docstring
    m.def("execute", &execute, "A function which takes in the code and return the execution result");
}