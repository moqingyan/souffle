
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
#include <regex>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

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
        {"provenance", 't', "[ none | explain | explore | subtreeHeights ]", "explain", false,
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
            std::make_unique<RamTransformerSequence>(
                std::make_unique<ExpandFilterTransformer>(),
                std::make_unique<HoistConditionsTransformer>(),
                std::make_unique<MakeIndexTransformer>())),
        std::make_unique<IfConversionTransformer>(), std::make_unique<ChoiceConversionTransformer>(),
        std::make_unique<CollapseFiltersTransformer>(), std::make_unique<TupleIdTransformer>(),
        std::make_unique<RamLoopTransformer>(
            std::make_unique<RamTransformerSequence>(
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
    return 0;
}

int check_ram_err(const std::unique_ptr<RamTranslationUnit> &ramTranslationUnit){
    if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
        std::cerr << ramTranslationUnit->getErrorReport();
        std::cerr << "During Ram phase: " << std::to_string(ramTranslationUnit->getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        return 1;
    }
    return 0;
}



std::map<std::string, std::vector<std::string>> execute(std::string code, bool get_prov){

    std::list<std::string> result = {};
    setup_config();
    DebugReport debugReport;
    ErrorReport errReport(true); // no-warning

    // Generate AST and do AST transformation
    std::unique_ptr<AstTranslationUnit> astTranslationUnit = get_ast(code, debugReport, errReport);
    auto astTransformer = get_ast_transformer();
    astTransformer->apply(*astTranslationUnit);
    check_ast_err(astTranslationUnit);
    

    // Translate AST to RAM 
    // TODO: change translate unit to accept fact strings
    
    std::unique_ptr<RamTranslationUnit> ramTranslationUnit =
            AstTranslator().translateUnit(*astTranslationUnit);

    auto ramTransformer = get_ram_transformer();
    ramTransformer->apply(*ramTranslationUnit);

    check_ram_err(ramTranslationUnit);
    
    // Execute RAM
    std::unique_ptr<InterpreterEngine> interpreter(
                    std::make_unique<InterpreterEngine>(*ramTranslationUnit));
    interpreter->executeMain();

    std::map<std::string, std::vector<std::string>> execution_res = interpreter->get_execute_result(); 

    if (get_prov){
        // only run explain interface if interpreted
        InterpreterProgInterface interface(*interpreter);
        auto it = execution_res.find("target");
        if (it != execution_res.end()) {
            auto target_ls = it->second;

            for (auto target : target_ls) {
                explain(interface, false, Global::config().get("provenance") == "subtreeHeights", "target(\"" + target + "\")");
            }
        } else if (Global::config().get("provenance") == "explore") {
            explain(interface, true, false);
        }
    }

    return execution_res;
}

class Interpreter{
    public:
        Interpreter(const std::string &code) {
            std::list<std::string> result = {};
            setup_config();
            DebugReport debugReport;
            ErrorReport errReport(true); // no-warning

            // Generate AST and do AST transformation
            this->astUnit = ParserDriver::parseTranslationUnit(code, errReport, debugReport);
            auto astTransformer = get_ast_transformer();
            astTransformer->apply(*this->astUnit);
            check_ast_err(this->astUnit);
            
            // Translate AST to RAM 
            // TODO: change translate unit to accept fact strings
            
            this->tUnit = AstTranslator().translateUnit(*this->astUnit);

            auto ramTransformer = get_ram_transformer();
            ramTransformer->apply(*this->tUnit);

            check_ram_err(this->tUnit);
            
            // Execute RAM
            this->engine = std::make_unique<InterpreterEngine>(*this->tUnit);
            this->interface = std::make_shared<InterpreterProgInterface>(*this->engine);

            // Initialize ss
            ExplainConfig::getExplainConfig().json = true;
            ExplainConfig::getExplainConfig().outputStream = std::make_unique<std::ostringstream>();

            engine->executeMain();
            this->execution_res = engine->get_execute_result(); 
            // only run explain interface if interpreted
            // InterpreterProgInterface interface(*engine);
            this->interface = std::make_shared<InterpreterProgInterface>(*this->engine);
        }

        std::map<std::string, std::vector<std::string>> execute() {
            return this->execution_res;
        }

        std::string explainRule(std::string ruleName, std::string ruleTuple){
            explain(*this->interface, false, Global::config().get("provenance") == "subtreeHeights", ruleName + "(" + ruleTuple + ")");
            return this->get_explained_string();
        }

        std::map<std::string, std::string> explainRulename(std::string ruleName){
            std::map<std::string, std::string> explanation;

            for (auto res_map: this->execution_res){
                std::string curRuleName = res_map.first;
                if ( ruleName != curRuleName){
                    continue;
                }

                std::vector<std::string> ruleTuples = res_map.second;
                for (std::string ruleTuple: ruleTuples){
                    explain(*this->interface, false, Global::config().get("provenance") == "subtreeHeights", ruleName + "(" + ruleTuple + ")");
                    std::string curExplanation = this->get_explained_string();
                    explanation[ruleTuple] = curExplanation;
                }
            }
            return explanation;
        }

        
    private:
        std::unique_ptr<AstTranslationUnit> astUnit;
        std::unique_ptr<InterpreterEngine> engine;
        std::unique_ptr<RamTranslationUnit> tUnit;
        std::shared_ptr<InterpreterProgInterface> interface;
        std::map<std::string, std::vector<std::string>> execution_res;

        std::string get_explained_string() {
            auto &str_stream = static_cast<std::ostringstream&>(*ExplainConfig::getExplainConfig().outputStream);
            return str_stream.str();
        }
};


PYBIND11_MODULE(PySouffle, m) {
    m.doc() = "pybind11 example plugin"; // optional module docstring
    // py::class_<InterpreterEngine, std::shared_ptr<InterpreterEngine>>(m, "Interpreter");
    py::class_<Interpreter, std::shared_ptr<Interpreter>>(m, "Interpreter")
        .def(py::init<std::string>())
        //    .def("set_interpreter", &Interpreter::set_interpreter)
        .def("execute", &Interpreter::execute)
        .def("explainRule", &Interpreter::explainRule)
        .def("explainRulename", &Interpreter::explainRulename);

    // m.def("getInterpreter", &getInterpreter, "A function which generate interpreter");
    m.def("execute", &execute, "A function which takes in the code and return the execution result");
    // m.def("get_prov", &get_prov, "A function which takes in the code and return the execution result");
}