//===-- StatsTracker.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_STATSTRACKER_H
#define KLEE_STATSTRACKER_H

#include "CallPathManager.h"
#include "klee/System/Time.h"

#include <memory>
#include <set>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/IR/BasicBlock.h"

namespace llvm {
  class BranchInst;
  class Function;
  class Instruction;
  class raw_fd_ostream;
}

namespace klee {
  class ExecutionState;
  class Executor;
  class InstructionInfoTable;
  class InterpreterHandler;
  struct KInstruction;
  struct StackFrame;

  class StatsTracker {
    friend class WriteStatsTimer;
    friend class WriteIStatsTimer;

    Executor &executor;
    std::string objectFilename;

    std::unique_ptr<llvm::raw_fd_ostream> istatsFile;

    /// @brief [Empc]: All the visited basic blocks
    std::unordered_map<const llvm::BasicBlock *,
                       std::pair<std::string, std::size_t>>
        visitedBasicBlocks;

    /// @brief [Empc]: The added visited basic blocks in this time interval
    std::unordered_map<const llvm::BasicBlock *,
                       std::pair<std::string, std::size_t>>
        addedVisitedBasicBlocks;

    /// @brief [Empc]: All the visited node lines
    std::unordered_set<std::string> visitedLines;

    /// @brief [Empc]: The added visited lines in this time interval
    std::unordered_set<std::string> addedVisitedLines;

    /// @brief [Empc]: All the visited basic blocks in defined functions
    std::unordered_map<const llvm::BasicBlock *,
                       std::pair<std::string, std::size_t>>
        visitedDefinedBasicBlocks;

    /// @brief [Empc]: The added visited basic blocks in defined functions in this
    /// time interval
    std::unordered_map<const llvm::BasicBlock *,
                       std::pair<std::string, std::size_t>>
        addedVisitedDefinedBasicBlocks;

    /// @brief [Empc]: All the visited node lines in defined functions
    std::unordered_set<std::string> visitedDefinedLines;

    /// @brief [Empc]: The added visited lines in defined functions in this time
    /// interval
    std::unordered_set<std::string> addedVisitedDefinedLines;

    /// @brief [Empc]: File handler for bc-stats
    std::unique_ptr<llvm::raw_fd_ostream> bcStatsFile;

    ::sqlite3 *statsFile = nullptr;
    ::sqlite3_stmt *transactionBeginStmt = nullptr;
    ::sqlite3_stmt *transactionEndStmt = nullptr;
    ::sqlite3_stmt *insertStmt = nullptr;
    std::uint32_t statsCommitEvery;
    std::uint32_t statsWriteCount = 0;
    time::Point startWallTime;

    unsigned numBranches;
    unsigned fullBranches, partialBranches;

    CallPathManager callPathManager;

    bool updateMinDistToUncovered;

  public:
    static bool useStatistics();
    static bool useIStats();

  private:
    void updateStateStatistics(uint64_t addend);
    void writeStatsHeader();
    void writeStatsLine();
    void writeIStats();
    /// @brief [Empc]: Write bc-stats
    void writeBCStats();

  public:
    StatsTracker(Executor &_executor, std::string _objectFilename,
                 bool _updateMinDistToUncovered);
    ~StatsTracker();

    StatsTracker(const StatsTracker &other) = delete;
    StatsTracker(StatsTracker &&other) noexcept = delete;
    StatsTracker &operator=(const StatsTracker &other) = delete;
    StatsTracker &operator=(StatsTracker &&other) noexcept = delete;

    // called after a new StackFrame has been pushed (for callpath tracing)
    void framePushed(ExecutionState &es, StackFrame *parentFrame);

    // called after a StackFrame has been popped
    void framePopped(ExecutionState &es);

    // called when some side of a branch has been visited. it is
    // imperative that this be called when the statistics index is at
    // the index for the branch itself.
    void markBranchVisited(ExecutionState *visitedTrue,
                           ExecutionState *visitedFalse);

    // called when execution is done and stats files should be flushed
    void done();

    // process stats for a single instruction step, es is the state
    // about to be stepped
    void stepInstruction(ExecutionState &es);

    /// Return duration since execution start.
    time::Span elapsed();

    void computeReachableUncovered();
  };

  uint64_t computeMinDistToUncovered(const KInstruction *ki,
                                     uint64_t minDistAtRA);

}

#endif /* KLEE_STATSTRACKER_H */
