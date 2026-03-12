//===-- SearcherLog.cpp -----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//  Empc: Effective Path Prioritization for Symbolic Execution with Path Cover
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright (c) 2024-2025 Shuangjie (Joshua) Yao.
// All rights reserved.
//
//===----------------------------------------------------------------------===//

#include "SearcherLog.h"

#include "llvm/Support/CommandLine.h"

#include "klee/Support/ErrorHandling.h"

namespace klee {
llvm::cl::OptionCategory
    EmpcSearcherLogCat("Searcher logging options",
                       "These options control the Empc searcher logging "
                       "including graphs and other information.");

llvm::cl::opt<bool> EmpcSearcherLoggingAll(
    "empc-searcher-logging-all", llvm::cl::init(false),
    llvm::cl::desc("Write logs about all the Empc information (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));

llvm::cl::opt<bool> EmpcSearcherLoggingGraphs(
    "empc-searcher-logging-graphs", llvm::cl::init(false),
    llvm::cl::desc("Write logs about graph analysis (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));

llvm::cl::opt<bool> EmpcSearcherLoggingData(
    "empc-searcher-logging-data", llvm::cl::init(false),
    llvm::cl::desc("Write logs about data analysis (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));

llvm::cl::opt<bool> EmpcSearcherLoggingStates(
    "empc-searcher-logging-states", llvm::cl::init(false),
    llvm::cl::desc(
        "Write logs about state selection and update (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));

llvm::cl::opt<bool> EmpcSearcherLoggingDebug(
    "empc-searcher-logging-debug", llvm::cl::init(false),
    llvm::cl::desc("Write logs about debugging information (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));

llvm::cl::opt<bool> EmpcSearcherLoggingOther(
    "empc-searcher-logging-other", llvm::cl::init(false),
    llvm::cl::desc("Write logs about other information (default=false)"),
    llvm::cl::cat(EmpcSearcherLogCat));
} // namespace klee

namespace klee {
namespace Empc {
Logging::Type Logging::reservedType = Logging::Type::OTHER;

std::unordered_map<Logging::Type, bool> Logging::loggingCheckMap = {
    {Type::GRAPH, false}, {Type::DATA, false},  {Type::STATE, false},
    {Type::DEBUG, false}, {Type::OTHER, false},
};

std::unordered_map<Logging::Type, std::unique_ptr<llvm::raw_fd_ostream>>
    Logging::loggingFileMap;

std::mutex Logging::mutex;

void Logging::init(
    std::function<std::unique_ptr<llvm::raw_fd_ostream>(const std::string &)>
        openFileHandler) {

  // Graph analysis
  if (EmpcSearcherLoggingAll || EmpcSearcherLoggingGraphs) {
    loggingCheckMap[Type::GRAPH] = true;

    auto fileHandler = openFileHandler("empc-searcher.graph.log");
    if (!fileHandler) {
      klee_error(
          "Unable to open state information file (empc-searcher.graph.log).");
    }

    loggingFileMap[Type::GRAPH] = std::move(fileHandler);
  } else {
    loggingFileMap[Type::GRAPH] = nullptr;
  }

  // Data analysis
  if (EmpcSearcherLoggingAll || EmpcSearcherLoggingData) {
    loggingCheckMap[Type::DATA] = true;

    auto fileHandler = openFileHandler("empc-searcher.data.log");
    if (!fileHandler) {
      klee_error(
          "Unable to open state information file (empc-searcher.data.log).");
    }

    loggingFileMap[Type::DATA] = std::move(fileHandler);
  } else {
    loggingFileMap[Type::DATA] = nullptr;
  }

  // States
  if (EmpcSearcherLoggingAll || EmpcSearcherLoggingStates) {
    loggingCheckMap[Type::STATE] = true;

    auto fileHandler = openFileHandler("empc-searcher.state.log");
    if (!fileHandler) {
      klee_error(
          "Unable to open state information file (empc-searcher.state.log).");
    }

    loggingFileMap[Type::STATE] = std::move(fileHandler);
  } else {
    loggingFileMap[Type::STATE] = nullptr;
  }

  // Debug
  if (EmpcSearcherLoggingAll || EmpcSearcherLoggingDebug) {
    loggingCheckMap[Type::DEBUG] = true;

    auto fileHandler = openFileHandler("empc-searcher.debug.log");
    if (!fileHandler) {
      klee_error(
          "Unable to open state information file (empc-searcher.debug.log).");
    }

    loggingFileMap[Type::DEBUG] = std::move(fileHandler);
  } else {
    loggingFileMap[Type::DEBUG] = nullptr;
  }

  // Other
  if (EmpcSearcherLoggingAll || EmpcSearcherLoggingOther) {
    loggingCheckMap[Type::OTHER] = true;

    auto fileHandler = openFileHandler("empc-searcher.other.log");
    if (!fileHandler) {
      klee_error(
          "Unable to open state information file (empc-searcher.other.log).");
    }

    loggingFileMap[Type::OTHER] = std::move(fileHandler);
  } else {
    loggingFileMap[Type::OTHER] = nullptr;
  }
}

bool Logging::check(Logging::Type _type) { return loggingCheckMap.at(_type); }

void Logging::start(Logging::Type _type, const std::string &_title) {
  assert(loggingCheckMap.at(_type));

  reservedType = _type;
  llvm::raw_fd_ostream &outStream = *loggingFileMap.at(_type);
  //   uint64_t elapsedTimeCount = 2394048402;
  //   outStream << "\n[Time] " << elapsedTimeCount / 1000000U << "."
  //             << (elapsedTimeCount % 1000000U) / 100U << " (s)\n";
  outStream << "[Desc] " << _title << "\n";
}

void Logging::stop() {
  assert(loggingCheckMap.at(reservedType));

  llvm::raw_fd_ostream &outStream = *loggingFileMap.at(reservedType);
  outStream.flush();
  reservedType = Type::OTHER;
}

void Logging::log(const std::string &_log) {
  assert(loggingCheckMap.at(reservedType));

  llvm::raw_fd_ostream &outStream = *loggingFileMap.at(reservedType);
  outStream << _log << "\n";
}

llvm::raw_fd_ostream &Logging::log() {
  assert(loggingCheckMap.at(reservedType));

  return *loggingFileMap.at(reservedType);
}

void Logging::all(Type _type, const std::string &_title,
                  const std::string &_log) {
  assert(loggingCheckMap.at(_type));

  std::unique_lock<std::mutex> lock(mutex);
  llvm::raw_fd_ostream &outStream = *loggingFileMap.at(_type);
  outStream << "[Desc] " << _title << "\n" << _log << "\n";
  outStream.flush();
}
} // namespace Empc
} // namespace klee