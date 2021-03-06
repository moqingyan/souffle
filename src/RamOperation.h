/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "RamCondition.h"
#include "RamExpression.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "RamTypes.h"
#include "RamUtils.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace souffle {

/**
 * @class RamOperation
 * @brief Abstract class for a relational algebra operation
 */
class RamOperation : public RamNode {
public:
    RamOperation* clone() const override = 0;

protected:
    void print(std::ostream& os) const override {
        print(os, 0);
    }
    /** @brief Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** @brief Pretty print jump-bed */
    static void print(const RamOperation* operation, std::ostream& os, int tabpos) {
        operation->print(os, tabpos);
    }

    friend class RamQuery;
};

/**
 * @class RamAbstractParallel
 * @brief Abstract class for parallel operation
 */
class RamAbstractParallel {};

/**
 * @class RamNestedOperation
 * @brief Abstract class for a nesting operations in a loop-nest
 *
 * In the following example, the nested operation
 * of "IF C1" is "IF C2":
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1
 *     IF C2
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * TODO (b-scholz): profile text is awkward; perhaps there is a better way of
 *                  storing profile information for RAM operations since
 *                  it is not always used for all RAM operations.
 */
class RamNestedOperation : public RamOperation {
public:
    RamNestedOperation(std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : nestedOperation(std::move(nested)), profileText(std::move(profileText)) {
        assert(nullptr != nestedOperation);
    }

    /** @brief Get nested operation */
    RamOperation& getOperation() const {
        return *nestedOperation;
    }

    /** @brief Get profile text */
    const std::string& getProfileText() const {
        return profileText;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get()};
    }

    void apply(const RamNodeMapper& map) override {
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        RamOperation::print(nestedOperation.get(), os, tabpos);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamNestedOperation&>(node);
        return equal_ptr(nestedOperation, other.nestedOperation) && profileText == other.profileText;
    }

    /** Nested operation */
    std::unique_ptr<RamOperation> nestedOperation;

    /** Text used by the profiler */
    const std::string profileText;
};

/**
 * @class RamTupleOperation
 * @brief Abstract class for relation searches and lookups
 */
class RamTupleOperation : public RamNestedOperation {
public:
    RamTupleOperation(int ident, std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamNestedOperation(std::move(nested), std::move(profileText)), identifier(ident) {}

    /** @brief Get identifier */
    int getTupleId() const {
        return identifier;
    }

    /** @brief Set identifier */
    void setTupleId(int id) {
        identifier = id;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return RamNestedOperation::getChildNodes();
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamTupleOperation&>(node);
        return RamNestedOperation::equal(other) && identifier == other.identifier;
    }

    /**
     * Identifier for the tuple, corresponding to
     * its position in the loop nest
     */
    int identifier;
};

/**
 * @class RamRelationOperation
 * @brief Abstract class for operations on relations
 *
 * One such operation is a for loop
 */
class RamRelationOperation : public RamTupleOperation {
public:
    RamRelationOperation(std::unique_ptr<RamRelationReference> relRef, int ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamTupleOperation(ident, std::move(nested), std::move(profileText)),
              relationRef(std::move(relRef)) {
        assert(relationRef != nullptr && "relation reference is a null-pointer");
    }

    /** @brief Get search relation */
    const RamRelation& getRelation() const {
        return *relationRef->get();
    }

    void apply(const RamNodeMapper& map) override {
        RamTupleOperation::apply(map);
        relationRef = map(std::move(relationRef));
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamTupleOperation::getChildNodes();
        res.push_back(relationRef.get());
        return res;
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamRelationOperation&>(node);
        return RamTupleOperation::equal(other) && equal_ptr(relationRef, other.relationRef);
    }

    /** Search relation */
    std::unique_ptr<RamRelationReference> relationRef;
};

/**
 * @class RamScan
 * @brief Iterate all tuples of a relation
 *
 * The following example iterates over all tuples
 * in the set A:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   FOR t0 IN A
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamScan : public RamRelationOperation {
public:
    RamScan(std::unique_ptr<RamRelationReference> rel, int ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationOperation(std::move(rel), ident, std::move(nested), std::move(profileText)) {}

    RamScan* clone() const override {
        return new RamScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId();
        os << " IN " << getRelation().getName() << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamParallelScan
 * @brief Iterate all tuples of a relation in parallel
 *
 * An example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   PARALLEL FOR t0 IN A
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelScan : public RamScan, public RamAbstractParallel {
public:
    RamParallelScan(std::unique_ptr<RamRelationReference> rel, int ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamScan(std::move(rel), ident, std::move(nested), profileText) {}

    RamParallelScan* clone() const override {
        return new RamParallelScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId();
        os << " IN " << getRelation().getName() << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }
};

/**
 * @class Relation Scan with Index
 * @brief An abstract class for performing indexed operations
 */
class RamIndexOperation : public RamRelationOperation {
public:
    RamIndexOperation(std::unique_ptr<RamRelationReference> r, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationOperation(std::move(r), ident, std::move(nested), std::move(profileText)),
              queryPattern(std::move(queryPattern)) {
        assert(getRangePattern().size() == getRelation().getArity());
        for (const auto& pattern : queryPattern) {
            assert(pattern != nullptr && "pattern is a null-pointer");
        }
    }

    /**
     * @brief Get range pattern
     * @return A std::vector of pointers to RamExpression objects
     */
    std::vector<RamExpression*> getRangePattern() const {
        return toPtrVector(queryPattern);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationOperation::getChildNodes();
        for (auto& pattern : queryPattern) {
            res.push_back(pattern.get());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        for (auto& pattern : queryPattern) {
            pattern = map(std::move(pattern));
        }
    }

protected:
    /** @brief Helper method for printing */
    void printIndex(std::ostream& os) const {
        const auto& attrib = getRelation().getAttributeNames();
        bool first = true;
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            if (!isRamUndefValue(queryPattern[i].get())) {
                if (first) {
                    os << " ON INDEX ";
                    first = false;
                } else {
                    os << " AND ";
                }
                os << "t" << getTupleId() << ".";
                os << attrib[i] << " = ";
                os << *queryPattern[i];
            }
        }
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIndexOperation&>(node);
        return RamRelationOperation::equal(other) && equal_targets(queryPattern, other.queryPattern);
    }

    /** Values of index per column of table (if indexable) */
    std::vector<std::unique_ptr<RamExpression>> queryPattern;
};

/**
 * @class RamIndexScan
 * @brief Search for tuples of a relation matching a criteria
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	 FOR t1 IN X ON INDEX t1.c = t0.0
 *	 ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamIndexScan : public RamIndexOperation {
public:
    RamIndexScan(std::unique_ptr<RamRelationReference> r, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexOperation(std::move(r), ident, std::move(queryPattern), std::move(nested),
                      std::move(profileText)) {}

    RamIndexScan* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        return new RamIndexScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::move(resQueryPattern), std::unique_ptr<RamOperation>(getOperation().clone()),
                getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId() << " IN ";
        os << rel.getName();
        printIndex(os);
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamParallelIndexScan
 * @brief Search for tuples of a relation matching a criteria
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	   PARALLEL FOR t1 IN X ON INDEX t1.c = t0.0
 *	     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelIndexScan : public RamIndexScan, public RamAbstractParallel {
public:
    RamParallelIndexScan(std::unique_ptr<RamRelationReference> rel, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexScan(std::move(rel), ident, std::move(queryPattern), std::move(nested), profileText) {}

    RamParallelIndexScan* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        return new RamParallelIndexScan(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId() << " IN ";
        os << rel.getName();
        printIndex(os);
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamAbstractChoice
 * @brief Abstract class for a choice operation
 *
 * Finding a single tuple, if it exists, such that a condition holds.
 */
class RamAbstractChoice {
public:
    RamAbstractChoice(std::unique_ptr<RamCondition> cond) : condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
    }

    /** @brief Getter for the condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "condition of choice is a null-pointer");
        return *condition;
    }

    void apply(const RamNodeMapper& map) {
        condition = map(std::move(condition));
    }

    std::vector<const RamNode*> getChildNodes() const {
        return {condition.get()};
    }

protected:
    bool equal(const RamNode& node) const {
        const auto& other = dynamic_cast<const RamAbstractChoice&>(node);
        return equal_ptr(condition, other.condition);
    }

    /** Condition for which a tuple in the relation may hold */
    std::unique_ptr<RamCondition> condition;
};

/**
 * @class RamChoice
 * @brief Find a tuple in a relation such that a given condition holds.
 *
 * Only one tuple is returned (if one exists), even
 * if multiple tuples satisfying the condition exist.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    CHOICE t1 IN A WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamChoice : public RamRelationOperation, public RamAbstractChoice {
public:
    RamChoice(std::unique_ptr<RamRelationReference> rel, size_t ident, std::unique_ptr<RamCondition> cond,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamRelationOperation(std::move(rel), ident, std::move(nested), std::move(profileText)),
              RamAbstractChoice(std::move(cond)) {}

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        RamAbstractChoice::apply(map);
    }

    RamChoice* clone() const override {
        return new RamChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get(), relationRef.get(), RamAbstractChoice::getChildNodes().at(0)};
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "CHOICE t" << getTupleId();
        os << " IN " << getRelation().getName();
        os << " WHERE " << getCondition();
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamChoice&>(node);
        return RamRelationOperation::equal(other) && RamAbstractChoice::equal(other);
    }
};

/**
 * @class ParallelRamChoice
 * @brief Find a tuple in a relation such that a given condition holds in parallel.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARALLEL CHOICE t1 IN A WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelChoice : public RamChoice, public RamAbstractParallel {
public:
    RamParallelChoice(std::unique_ptr<RamRelationReference> rel, size_t ident,
            std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamChoice(std::move(rel), ident, std::move(cond), std::move(nested), profileText) {}

    RamParallelChoice* clone() const override {
        return new RamParallelChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL CHOICE t" << getTupleId();
        os << " IN " << getRelation().getName();
        os << " WHERE " << getCondition();
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamIndexChoice
 * @brief Use an index to find a tuple in a relation such that a given condition holds.
 *
 * Only one tuple is returned (if one exists), even
 * if multiple tuples satisfying the condition exist.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    CHOICE A AS t1 ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamIndexChoice : public RamIndexOperation, public RamAbstractChoice {
public:
    RamIndexChoice(std::unique_ptr<RamRelationReference> r, int ident, std::unique_ptr<RamCondition> cond,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexOperation(std::move(r), ident, std::move(queryPattern), std::move(nested),
                      std::move(profileText)),
              RamAbstractChoice(std::move(cond)) {
        assert(getRangePattern().size() == getRelation().getArity());
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        for (auto& pattern : queryPattern) {
            pattern = map(std::move(pattern));
        }
        RamAbstractChoice::apply(map);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamIndexOperation::getChildNodes();
        res.push_back(RamAbstractChoice::getChildNodes().at(0));
        return res;
    }

    RamIndexChoice* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (size_t i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        auto* res = new RamIndexChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::unique_ptr<RamCondition>(condition->clone()), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "CHOICE " << rel.getName() << " AS t" << getTupleId();
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIndexChoice&>(node);
        return RamIndexOperation::equal(other) && RamAbstractChoice::equal(other);
    }
};

/**
 * @class RamParallelIndexChoice
 * @brief Use an index to find a tuple in a relation such that a given condition holds in parallel.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARALLEL CHOICE A AS t1 ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelIndexChoice : public RamIndexChoice, public RamAbstractParallel {
public:
    RamParallelIndexChoice(std::unique_ptr<RamRelationReference> r, int ident,
            std::unique_ptr<RamCondition> cond, std::vector<std::unique_ptr<RamExpression>> queryPattern,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamIndexChoice(std::move(r), ident, std::move(cond), std::move(queryPattern), std::move(nested),
                      profileText) {}

    RamParallelIndexChoice* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (size_t i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        auto* res = new RamParallelIndexChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::unique_ptr<RamCondition>(condition->clone()), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL CHOICE " << rel.getName() << " AS t" << getTupleId();
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamAbstractAggregate
 * @brief Abstract class for aggregation
 *
 * A particular function (e.g. MIN) is applied given a
 * that a condition holds
 */
class RamAbstractAggregate {
public:
    RamAbstractAggregate(
            AggregateOp fun, std::unique_ptr<RamExpression> expr, std::unique_ptr<RamCondition> cond)
            : function(fun), expression(std::move(expr)), condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
        assert(expression != nullptr && "Expression is a null-pointer");
    }

    virtual ~RamAbstractAggregate() = default;

    /** @brief Get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "Condition of aggregate is a null-pointer");
        return *condition;
    }

    /** @brief Get aggregation function */
    AggregateOp getFunction() const {
        return function;
    }

    /** @brief Get target expression */
    const RamExpression& getExpression() const {
        assert(expression != nullptr && "Expression of aggregate is a null-pointer");
        return *expression;
    }

    std::vector<const RamNode*> getChildNodes() const {
        return {expression.get(), condition.get()};
    }

protected:
    void print(std::ostream& os, int /* tabpos */) const {
        switch (function) {
            case AggregateOp::MIN:
            case AggregateOp::FMIN:
            case AggregateOp::UMIN:
                os << "min ";
                break;
            case AggregateOp::MAX:
            case AggregateOp::UMAX:
            case AggregateOp::FMAX:
                os << "max ";
                break;
            case AggregateOp::SUM:
            case AggregateOp::FSUM:
            case AggregateOp::USUM:
                os << "sum ";
                break;
            case AggregateOp::COUNT:
                os << "count ";
                break;
            case AggregateOp::MEAN:
                os << "mean ";
                break;
        }
        if (function != AggregateOp::COUNT) {
            os << *expression << " ";
        }
    }

protected:
    bool equal(const RamNode& node) const {
        const auto& other = dynamic_cast<const RamAbstractAggregate&>(node);
        return function == other.function && equal_ptr(expression, other.expression) &&
               equal_ptr(condition, other.condition);
    }

    /** Aggregation function */
    AggregateOp function;

    /** Aggregation expression */
    std::unique_ptr<RamExpression> expression;

    /** Aggregation tuple condition */
    std::unique_ptr<RamCondition> condition;
};

/**
 * @class RamAggregate
 * @brief Aggregation function applied on some relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function COUNT to determine the number
 * of elements in A.
 */
class RamAggregate : public RamRelationOperation, public RamAbstractAggregate {
public:
    RamAggregate(std::unique_ptr<RamOperation> nested, AggregateOp fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, int ident)
            : RamRelationOperation(std::move(relRef), ident, std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamAggregate* clone() const override {
        return new RamAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), function,
                std::unique_ptr<RamRelationReference>(relationRef->clone()),
                std::unique_ptr<RamExpression>(expression->clone()),
                std::unique_ptr<RamCondition>(condition->clone()), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << "FOR ALL t" << getTupleId() << " ∈ " << getRelation().getName();
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAggregate&>(node);
        return RamRelationOperation::equal(other) && RamAbstractAggregate::equal(node);
    }
};

/**
 * @class RamIndexAggregate
 * @brief Indexed aggregation on a relation
 */
class RamIndexAggregate : public RamIndexOperation, public RamAbstractAggregate {
public:
    RamIndexAggregate(std::unique_ptr<RamOperation> nested, AggregateOp fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, std::vector<std::unique_ptr<RamExpression>> queryPattern,
            int ident)
            : RamIndexOperation(std::move(relRef), ident, std::move(queryPattern), std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamIndexOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamIndexAggregate* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> pattern;
        for (auto const& e : queryPattern) {
            pattern.push_back(std::unique_ptr<RamExpression>(e->clone()));
        }
        return new RamIndexAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), function,
                std::unique_ptr<RamRelationReference>(relationRef->clone()),
                std::unique_ptr<RamExpression>(expression->clone()),
                std::unique_ptr<RamCondition>(condition->clone()), std::move(pattern), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamIndexOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << "SEARCH t" << getTupleId() << " ∈ " << getRelation().getName();
        printIndex(os);
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIndexAggregate&>(node);
        return RamIndexOperation::equal(other) && RamAbstractAggregate::equal(other);
    }
};

/**
 * @class RamUnpackRecord
 * @brief Record lookup
 *
 * Looks up a record with respect to an expression
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * UNPACK t0.0 INTO t1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamUnpackRecord : public RamTupleOperation {
public:
    RamUnpackRecord(std::unique_ptr<RamOperation> nested, int ident, std::unique_ptr<RamExpression> expr,
            size_t arity)
            : RamTupleOperation(ident, std::move(nested)), expression(std::move(expr)), arity(arity) {
        assert(expression != nullptr && "Expression is a null-pointer");
    }

    /** @brief Get record expression */
    const RamExpression& getExpression() const {
        assert(expression != nullptr && "Expression of unpack-record is a null-pointer");
        return *expression;
    }

    /** @brief Get arity of record */
    std::size_t getArity() const {
        return arity;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamTupleOperation::getChildNodes();
        res.push_back(expression.get());
        return res;
    }

    RamUnpackRecord* clone() const override {
        return new RamUnpackRecord(std::unique_ptr<RamOperation>(getOperation().clone()), getTupleId(),
                std::unique_ptr<RamExpression>(getExpression().clone()), arity);
    }

    void apply(const RamNodeMapper& map) override {
        RamTupleOperation::apply(map);
        expression = map(std::move(expression));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamUnpackRecord&>(node);
        return RamTupleOperation::equal(other) && equal_ptr(expression, other.expression) &&
               arity == other.arity;
    }

    /** Expression for record reference */
    std::unique_ptr<RamExpression> expression;

    /** Arity of the unpacked tuple */
    const size_t arity;
};

/**
 * @class RamAbstractConditional
 * @brief Abstract conditional statement
 */
class RamAbstractConditional : public RamNestedOperation {
public:
    RamAbstractConditional(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamNestedOperation(std::move(nested), std::move(profileText)), condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
    }

    /** @brief Get condition that must be satisfied */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "condition of conditional operation is a null-pointer");
        return *condition;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamNestedOperation::getChildNodes();
        res.push_back(condition.get());
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        RamNestedOperation::apply(map);
        condition = map(std::move(condition));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAbstractConditional&>(node);
        return RamNestedOperation::equal(node) && equal_ptr(condition, other.condition);
    }

    /** Condition */
    std::unique_ptr<RamCondition> condition;
};

/**
 * @class RamFilter
 * @brief Checks whether a given condition holds
 *
 * The RamFilter is essentially an "if" statement.
 *
 * The following example checks that both C1 and C2 hold
 * before proceeding deeper in the loop nest:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IF C1 AND C2
 *  ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamFilter : public RamAbstractConditional {
public:
    RamFilter(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamAbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    RamFilter* clone() const override {
        return new RamFilter(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << std::endl;
        RamNestedOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamBreak
 * @brief Breaks out of the loop if a condition holds
 *
 * The following example will break out of the inner-most
 * loop if the condition (t1.1 = 4) holds:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 in A
 *   FOR t1 in B
 *     IF t0.1 = 4 BREAK
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamBreak : public RamAbstractConditional {
public:
    RamBreak(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamAbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    RamBreak* clone() const override {
        return new RamBreak(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << " BREAK" << std::endl;
        RamNestedOperation::print(os, tabpos + 1);
    }
};

/**
 * @class RamProject
 * @brief Project a result into the target relation.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     PROJECT (t0.a, t0.b, t0.c) INTO @new_X
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamProject : public RamOperation {
public:
    RamProject(std::unique_ptr<RamRelationReference> relRef,
            std::vector<std::unique_ptr<RamExpression>> expressions)
            : relationRef(std::move(relRef)), expressions(std::move(expressions)) {
        assert(relationRef != nullptr && "Relation reference is a null-pointer");
        for (auto const& expr : expressions) {
            assert(expr != nullptr && "Expression is a null-pointer");
        }
    }

    /** @brief Get relation */
    const RamRelation& getRelation() const {
        return *relationRef->get();
    }

    /** @brief Get expressions */
    std::vector<RamExpression*> getValues() const {
        return toPtrVector(expressions);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        res.push_back(relationRef.get());
        for (const auto& expr : expressions) {
            res.push_back(expr.get());
        }
        return res;
    }

    RamProject* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->clone());
        }
        return new RamProject(
                std::unique_ptr<RamRelationReference>(relationRef->clone()), std::move(newValues));
    }

    void apply(const RamNodeMapper& map) override {
        relationRef = map(std::move(relationRef));
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PROJECT (" << join(expressions, ", ", print_deref<std::unique_ptr<RamExpression>>())
           << ") INTO " << getRelation().getName() << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamProject&>(node);
        return equal_ptr(relationRef, other.relationRef) && equal_targets(expressions, other.expressions);
    }

    /** Relation that values are projected into */
    std::unique_ptr<RamRelationReference> relationRef;

    /* Values (expressions) for projection */
    std::vector<std::unique_ptr<RamExpression>> expressions;
};

/**
 * @class RamSubroutineReturnValue
 * @brief A statement for returning from a ram subroutine
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   ...
 *     RETURN (t0.0, t0.1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamSubroutineReturnValue : public RamOperation {
public:
    RamSubroutineReturnValue(std::vector<std::unique_ptr<RamExpression>> vals)
            : expressions(std::move(vals)) {
        for (const auto& expr : expressions) {
            assert(expr != nullptr && "Expression is a null-pointer");
        }
    }

    /** @brief Getter for expressions */
    std::vector<RamExpression*> getValues() const {
        return toPtrVector(expressions);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& expr : expressions) {
            res.push_back(expr.get());
        }
        return res;
    }

    RamSubroutineReturnValue* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->clone());
        }
        return new RamSubroutineReturnValue(std::move(newValues));
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "RETURN (";
        for (auto val : getValues()) {
            os << *val;
            if (val != *(getValues().end() - 1)) {
                os << ", ";
            }
        }
        os << ")" << std::endl;
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamSubroutineReturnValue&>(node);
        return equal_targets(expressions, other.expressions);
    }

    /** Return expressions */
    std::vector<std::unique_ptr<RamExpression>> expressions;
};

}  // namespace souffle
