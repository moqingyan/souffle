/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file WriteStream.h
 *
 ***********************************************************************/

#pragma once

#include "RWOperation.h"
#include "RamTypes.h"
#include "RecordTable.h"
#include "SymbolTable.h"
#include "json11.h"

#include <cassert>
#include <string>
#include <vector>

namespace souffle {

using json11::Json;

class WriteStream {
public:
    WriteStream(
            const RWOperation& rwOperation, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : symbolTable(symbolTable), recordTable(recordTable),
              summary(rwOperation.get("IO") == "stdoutprintsize") {
        const std::string& relationName{rwOperation.get("name")};

        std::string parseErrors;

        types = Json::parse(rwOperation.get("types"), parseErrors);

        assert(parseErrors.size() == 0 && "Internal JSON parsing failed.");

        arity = static_cast<size_t>(types[relationName]["arity"].long_value());

        for (size_t i = 0; i < arity; ++i) {
            std::string type = types[relationName]["types"][i].string_value();
            typeAttributes.push_back(std::move(type));
        }
    }

    template <typename T>
    void writeAll(const T& relation) {
        if (summary) {
            return writeSize(relation.size());
        }
        auto lease = symbolTable.acquireLock();
        (void)lease;  // silence "unused variable" warning
        if (arity == 0) {
            if (relation.begin() != relation.end()) {
                writeNullary();
            }
            return;
        }
        for (const auto& current : relation) {
            writeNext(current);
        }
    }

    template <typename T>
    void writeSize(const T& relation) {
        writeSize(relation.size());
    }

    virtual ~WriteStream() = default;

protected:
    const SymbolTable& symbolTable;
    const RecordTable& recordTable;

    Json types;
    std::vector<std::string> typeAttributes;

    const bool summary;
    size_t arity;

    virtual void writeNullary() = 0;
    virtual void writeNextTuple(const RamDomain* tuple) = 0;
    virtual void writeSize(std::size_t) {
        assert(false && "attempting to print size of a write operation");
    }
    template <typename Tuple>
    void writeNext(const Tuple tuple) {
        writeNextTuple(tuple.data);
    }

    void outputRecord(std::ostream& destination, const RamDomain value, const std::string& name) {
        Json recordInfo = types["records"][name];

        // Check if record type information are present
        assert(!recordInfo.is_null() && "Missing record type information");

        // Check for nil
        if (value == 0) {
            destination << "nil";
            return;
        }

        const Json recordTypes = recordInfo["types"];
        const size_t recordArity = recordInfo["arity"].long_value();

        const RamDomain* tuplePtr = recordTable.unpack(value, recordArity);

        destination << "[";

        // print record's elements
        for (size_t i = 0; i < recordArity; ++i) {
            if (i > 0) {
                destination << ", ";
            }

            const std::string& recordType = recordTypes[i].string_value();
            const RamDomain recordValue = tuplePtr[i];

            switch (recordType[0]) {
                case 'i':
                    destination << recordValue;
                    break;
                case 'f':
                    destination << ramBitCast<RamFloat>(recordValue);
                    break;
                case 'u':
                    destination << ramBitCast<RamUnsigned>(recordValue);
                    break;
                case 's':
                    destination << symbolTable.unsafeResolve(recordValue);
                    break;
                case 'r':
                    outputRecord(destination, recordValue, recordType);
                    break;
                default:
                    assert(false && "Unsupported type attribute.");
            }
        }
        destination << "]";
    }
};

class WriteStreamFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(const RWOperation& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) = 0;

    virtual const std::string& getName() const = 0;
    virtual ~WriteStreamFactory() = default;
};

template <>
inline void WriteStream::writeNext(const RamDomain* tuple) {
    writeNextTuple(tuple);
}

} /* namespace souffle */
