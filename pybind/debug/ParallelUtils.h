/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelUtils.h
 *
 * A set of utilities abstracting from the underlying parallel library.
 * Currently supported APIs: OpenMP and Cilk
 *
 ***********************************************************************/

#pragma once

#include <atomic>

#ifdef _OPENMP

/**
 * Implementation of parallel control flow constructs utilizing OpenMP
 */

#include <omp.h>

#ifdef __APPLE__
#define pthread_yield pthread_yield_np
#endif

// support for a parallel region
#define PARALLEL_START _Pragma("omp parallel") {
#define PARALLEL_END }

// support for parallel loops
#define pfor _Pragma("omp for schedule(dynamic)") for

// spawn and sync are processed sequentially (overhead to expensive)
#define task_spawn
#define task_sync

// section start / end => corresponding OpenMP pragmas
// NOTE: disabled since it causes performance losses
//#define SECTIONS_START _Pragma("omp parallel sections") {
// NOTE: we stick to flat-level parallelism since it is faster due to thread pooling
#define SECTIONS_START {
#define SECTIONS_END }

// the markers for a single section
//#define SECTION_START _Pragma("omp section") {
#define SECTION_START {
#define SECTION_END }

// a macro to create an operation context
#define CREATE_OP_CONTEXT(NAME, INIT) auto NAME = INIT;
#define READ_OP_CONTEXT(NAME) NAME

#else

// support for a parallel region => sequential execution
#define PARALLEL_START {
#define PARALLEL_END }

// support for parallel loops => simple sequential loop
#define pfor for

// spawn and sync not supported
#define task_spawn
#define task_sync

// sections are processed sequentially
#define SECTIONS_START {
#define SECTIONS_END }

// sections are inlined
#define SECTION_START {
#define SECTION_END }

// a macro to create an operation context
#define CREATE_OP_CONTEXT(NAME, INIT) auto NAME = INIT;
#define READ_OP_CONTEXT(NAME) NAME

// mark es sequential
#define IS_SEQUENTIAL

#endif

#ifndef IS_SEQUENTIAL
#define IS_PARALLEL
#endif

#ifdef IS_PARALLEL
#define MAX_THREADS (omp_get_max_threads())
#else
#define MAX_THREADS (1)
#endif

#ifdef IS_PARALLEL

#include <mutex>
#include <stdio.h>

/**
 * A small utility class for implementing simple locks.
 */
class Lock {
    // the underlying mutex
    std::mutex mux;

public:
    struct Lease {
        Lease(std::mutex& mux) : mux(&mux) {
            printf("Getting lease from std::mutex %lx\n", (long int) &mux);
            mux.lock();
            printf("Mutex Locked\n");
        }
        Lease(Lease&& other) : mux(other.mux) {
            other.mux = nullptr;
        }
        Lease(const Lease& other) = delete;
        ~Lease() {
            if (mux != nullptr) {
                mux->unlock();
            }
        }

    protected:
        std::mutex* mux;
    };

    // acquired the lock for the live-cycle of the returned guard
    Lease acquire() {
        return Lease(mux);
    }

    void lock() {
        mux.lock();
    }

    bool try_lock() {
        return mux.try_lock();
    }

    void unlock() {
        mux.unlock();
    }

#endif