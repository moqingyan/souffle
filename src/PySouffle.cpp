
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

// auto get_ram_transformer(){
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

//     return ramTransform;
// }

// // parse the AST from the string code
// std::unique_ptr<AstTranslationUnit> get_ast(std::string code, DebugReport debugReport, ErrorReport errReport){
//     std::unique_ptr<AstTranslationUnit> astTranslationUnit =
//             ParserDriver::parseTranslationUnit(code, errReport, debugReport);
//     return astTranslationUnit;
// }

// void check_ast_err(const std::unique_ptr<AstTranslationUnit> &astTranslationUnit){
//     if (astTranslationUnit->getErrorReport().getNumErrors() != 0) {
//         std::cerr << astTranslationUnit->getErrorReport();
//         std::cerr << "During AST phase: " << std::to_string(astTranslationUnit->getErrorReport().getNumErrors()) +
//                              " errors generated, evaluation aborted"
//                   << std::endl;
//         exit(1);
//     }
// }

// void check_ram_err(const std::unique_ptr<RamTranslationUnit> &ramTranslationUnit){
//     if (ramTranslationUnit->getErrorReport().getNumIssues() != 0) {
//         std::cerr << ramTranslationUnit->getErrorReport();
//         std::cerr << "During Ram phase: " << std::to_string(ramTranslationUnit->getErrorReport().getNumErrors()) +
//                              " errors generated, evaluation aborted"
//                   << std::endl;
//         exit(1);
//     }
// }

std::string execute(std::string code, bool get_prov){

    // DebugReport debugReport;
    // ErrorReport errReport(true); // no-warning

    // // Generate AST and do AST transformation
    // std::unique_ptr<AstTranslationUnit> astTranslationUnit = get_ast(code, debugReport, errReport);
    // auto astTransformer = get_ast_transformer();
    // astTransformer->apply(*astTranslationUnit);
    // check_ast_err(astTranslationUnit);

    // // Translate AST to RAM 
    // // TODO: change translate unit to accept fact strings
    // std::unique_ptr<RamTranslationUnit> ramTranslationUnit =
    //         AstTranslator().translateUnit(*astTranslationUnit);
    // auto ramTransformer = get_ram_transformer();
    // ramTransformer->apply(*ramTranslationUnit);
    // check_ram_err(ramTranslationUnit);
    
    // // Execute RAM
    // std::unique_ptr<InterpreterEngine> interpreter(
    //                 std::make_unique<InterpreterEngine>(*ramTranslationUnit));
    // interpreter->executeMain();

    return "pass";

}

PYBIND11_MODULE(PySouffle, m) {
    m.doc() = "pybind11 example plugin"; // optional module docstring
    m.def("execute", &execute, "A function which takes in the code and return the execution result");
}