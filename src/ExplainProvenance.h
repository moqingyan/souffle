/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExplainProvenance.h
 *
 * Abstract class for implementing an instance of the explain interface for provenance
 *
 ***********************************************************************/

#pragma once

#include "ExplainTree.h"
#include "RamTypes.h"
#include "SouffleInterface.h"
#include "Util.h"
#include "WriteStreamCSV.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/** Equivalence class for variables in query command */
class Equivalence {
public:
    /** Destructor */
    ~Equivalence() = default;

    /**
     * Constructor for Equvialence class
     * @param t, type of the variable
     * @param s, symbol of the variable
     * @param idx, first occurence of the variable
     * */
    Equivalence(char t, std::string s, std::pair<size_t, size_t> idx) : type(t), symbol(std::move(s)) {
        indices.push_back(idx);
    }

    /** Copy constructor */
    Equivalence(const Equivalence& o) = default;

    /** Copy assignment operator */
    Equivalence& operator=(const Equivalence& o) = default;

    /** Add index at the end of indices vector */
    void push_back(std::pair<size_t, size_t> idx) {
        indices.push_back(idx);
    }

    /** Verify if elements at the indices are equivalent in the given product */
    bool verify(const std::vector<tuple>& product) const {
        for (size_t i = 1; i < indices.size(); ++i) {
            if (product[indices[i].first][indices[i].second] !=
                    product[indices[i - 1].first][indices[i - 1].second]) {
                return false;
            }
        }
        return true;
    }

    /** Extract index of the first occurrence of the varible */
    std::pair<size_t, size_t> getFirstIdx() {
        return indices[0];
    }

    /** Get indices of equivalent variables */
    std::vector<std::pair<size_t, size_t>> getIndices() {
        return indices;
    }

    /** Get type of the variable of the equivalence class,
     * 'i' for RamSigned, 's' for symbol
     * 'u' for RamUnsigned, 'f' for RamFloat
     */
    char getType() {
        return type;
    }

    /** Get the symbol of variable */
    std::string getSymbol() {
        return symbol;
    }

private:
    char type;
    std::string symbol;
    std::vector<std::pair<size_t, size_t>> indices;
};

/** Constant constraints for values in query command */
class ConstConstraint {
public:
    /** Constructor */
    ConstConstraint() = default;

    /** Destructor */
    ~ConstConstraint() = default;

    /** Add constant constraint at the end of constConstrs vector */
    void push_back(std::pair<std::pair<size_t, size_t>, RamDomain> constr) {
        constConstrs.push_back(constr);
    }

    /** Verify if the query product satisfies constant constraint */
    bool verify(const std::vector<tuple>& product) const {
        return std::all_of(constConstrs.begin(), constConstrs.end(), [&product](auto constr) {
            return product[constr.first.first][constr.first.second] == constr.second;
        });
    }

    /** Get the constant constraint vector */
    std::vector<std::pair<std::pair<size_t, size_t>, RamDomain>>& getConstraints() {
        return constConstrs;
    }

private:
    std::vector<std::pair<std::pair<size_t, size_t>, RamDomain>> constConstrs;
};

/** utility function to split a string */
inline std::vector<std::string> split(const std::string& s, char delim, int times = -1) {
    std::vector<std::string> v;
    std::stringstream ss(s);
    std::string item;

    while ((times > 0 || times <= -1) && std::getline(ss, item, delim)) {
        v.push_back(item);
        times--;
    }

    if (ss.peek() != EOF) {
        std::string remainder;
        std::getline(ss, remainder);
        v.push_back(remainder);
    }

    return v;
}

class ExplainProvenance {
public:
    ExplainProvenance(SouffleProgram& prog, bool useSublevels)
            : prog(prog), symTable(prog.getSymbolTable()), useSublevels(useSublevels) {}
    virtual ~ExplainProvenance() = default;

    virtual void setup() = 0;

    virtual std::unique_ptr<TreeNode> explain(
            std::string relName, std::vector<std::string> tuple, size_t depthLimit) = 0;

    virtual std::unique_ptr<TreeNode> explainSubproof(
            std::string relName, RamDomain label, size_t depthLimit) = 0;

    virtual std::vector<std::string> explainNegationGetVariables(
            std::string relName, std::vector<std::string> args, size_t ruleNum) = 0;

    virtual std::unique_ptr<TreeNode> explainNegation(std::string relName, size_t ruleNum,
            const std::vector<std::string>& tuple, std::map<std::string, std::string>& bodyVariables) = 0;

    virtual std::string getRule(std::string relName, size_t ruleNum) = 0;

    virtual std::vector<std::string> getRules(const std::string& relName) = 0;

    virtual std::string measureRelation(std::string relName) = 0;

    virtual void printRulesJSON(std::ostream& os) = 0;

    /**
     * Process query with given arguments
     * @param rels, vector of relation, argument pairs
     * */
    virtual void queryProcess(const std::vector<std::pair<std::string, std::vector<std::string>>>& rels) = 0;

protected:
    SouffleProgram& prog;
    SymbolTable& symTable;
    bool useSublevels;

    std::vector<RamDomain> argsToNums(
            const std::string& relName, const std::vector<std::string>& args) const {
        std::vector<RamDomain> nums;

        std::cout << "transform to tuple: " << symTable << std::endl;

        auto rel = prog.getRelation(relName);
        if (rel == nullptr) {
            return nums;
        }

        for (size_t i = 0; i < args.size(); i++) {
            switch (*(rel->getAttrType(i))) {
                case 's':
                    if (args[i].size() >= 2 && args[i][0] == '"' && args[i][args[i].size() - 1] == '"') {
                        auto originalStr = args[i].substr(1, args[i].size() - 2);
                        nums.push_back(symTable.lookup(originalStr));
                    }
                    break;
                case 'f':
                    nums.push_back(ramBitCast(RamFloatFromString(args[i])));
                    break;
                case 'u':
                    nums.push_back(ramBitCast(RamUnsignedFromString(args[i])));
                    break;
                default:
                    nums.push_back(RamSignedFromString(args[i]));
                    break;
            }
        }

        std::cout << nums <<std::endl;
        return nums;
    }

    /**
     * Decode arguments from their ram representation and return as strings.
     **/
    std::vector<std::string> decodeArguments(
            const std::string& relName, const std::vector<RamDomain>& nums) const {
        std::vector<std::string> args;

        auto rel = prog.getRelation(relName);
        if (rel == nullptr) {
            return args;
        }

        for (size_t i = 0; i < nums.size(); i++) {
            args.push_back(RamDomainToArgument(*rel->getAttrType(i), nums[i]));
        }

        return args;
    }

    std::string RamDomainToArgument(const char type, const RamDomain num) const {
        std::string arg;
        switch (type) {
            case 's':
                arg = "\"" + std::string(symTable.resolve(num)) + "\"";
                break;
            case 'f':
                arg = std::to_string(ramBitCast<RamFloat>(num));
                break;
            case 'u':
                arg = std::to_string(ramBitCast<RamUnsigned>(num));
                break;
            default:
                arg = std::to_string(ramBitCast<RamSigned>(num));
        }
        return arg;
    }
};

}  // end of namespace souffle
