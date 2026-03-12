//===-- SearcherHelper.h ----------------------------------------*- C++ -*-===//
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

#ifndef EMPC_SEARCHERHELPER_H_
#define EMPC_SEARCHERHELPER_H_

#include "SearcherData.h"
#include "SearcherDefs.h"
#include "SearcherGraph.h"

#include <map>
#include <unordered_map>
#include <unordered_set>

#include "ExecutionState.h"

#include "klee/ADT/DiscretePDF.h"
#include "klee/Module/KInstIterator.h"

namespace klee {
namespace Empc {
namespace Utility {
llvm::Instruction *convert(KInstIterator instrIter);
llvm::BasicBlock *convert(llvm::Instruction *instr);
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
template <typename _Tp1, typename _Tp2> class MutualMap {
protected:
  std::unordered_map<_Tp1, std::unordered_set<_Tp2>> map12;
  std::unordered_map<_Tp2, std::unordered_set<_Tp1>> map21;

  std::unordered_set<_Tp2> empty2;
  std::unordered_set<_Tp1> empty1;

public:
  MutualMap() = default;
  ~MutualMap() = default;

  void add(const _Tp1 &value1, const _Tp2 &value2);
  void add(const _Tp1 &baseValue, const std::vector<_Tp1> &addedValues,
           int __x);
  void add(const _Tp2 &baseValue, const std::vector<_Tp2> &addedValues, int __x,
           int __y);

  void remove(const _Tp1 &value1);
  void remove(const _Tp2 &value2);
  void remove(const _Tp1 &value1, const _Tp2 &value2);

  bool find(const _Tp1 &value1) const {
    return map12.find(value1) != map12.end();
  }
  bool find(const _Tp2 &value2) const {
    return map21.find(value2) != map21.end();
  }

  const std::unordered_set<_Tp2> &at(const _Tp1 &value1) const {
    return map12.find(value1) == map12.end() ? empty2 : map12.at(value1);
  }
  const std::unordered_set<_Tp1> &at(const _Tp2 &value2) const {
    return map21.find(value2) == map21.end() ? empty1 : map21.at(value2);
  }
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
class ExtendedExecutionState {
private:
  static std::unordered_map<const ExecutionState *,
                            std::unique_ptr<ExtendedExecutionState>>
      stateMap;

public:
  ExecutionState *rawState;

  InterProcGraph::PathReservedInfo pathReservedInfo;

  const llvm::BasicBlock *prevBasicBlock;
  const llvm::BasicBlock *nextBasicBlock;

public:
  static ExtendedExecutionState *addExecutionState(ExecutionState *kleeState);

  static void removeExecutionState(const ExecutionState *kleeState);

  static bool findExecutionState(const ExecutionState *kleeState);

public:
  ExtendedExecutionState(ExecutionState *_rawState)
      : rawState(_rawState), prevBasicBlock(nullptr), nextBasicBlock(nullptr) {}
  ~ExtendedExecutionState() = default;

  /// @brief Provide priority choice for states on the same group (e.g. states
  /// on MPC group selection / states on data dep selection). Use nurs:rp here.
  /// @return
  double priority() { return std::pow(0.6, rawState->depth); }
};

class StateTravBlockMap
    : public MutualMap<ExtendedExecutionState *, const llvm::BasicBlock *> {
private:
  const llvm::Function *func;

public:
  StateTravBlockMap() = delete;
  StateTravBlockMap(const llvm::Function *func)
      : MutualMap<ExtendedExecutionState *, const llvm::BasicBlock *>(),
        func(func) {}
  StateTravBlockMap(const StateTravBlockMap &) = delete;
  StateTravBlockMap &operator=(const StateTravBlockMap &) = delete;
  ~StateTravBlockMap() = default;

  void add(ExtendedExecutionState *_state);
  void add(ExtendedExecutionState *_state,
           const std::vector<ExtendedExecutionState *> &_states);
};

class BranchStateMap {

private:
private:
  std::unordered_map<const llvm::Function *, std::shared_ptr<StateTravBlockMap>>
      funcStateTravMap;

  std::function<bool(klee::KInstIterator, const llvm::BasicBlock *)>
      reachCheckFunction;

  MutualMap<const llvm::BasicBlock *, ExtendedExecutionState *> brBlockStateMap;

  std::unordered_map<const llvm::BasicBlock *,
                     std::unordered_set<const llvm::BasicBlock *>>
      dep1InfeBlockMap;

  std::unordered_map<const llvm::BasicBlock *,
                     std::unordered_set<const llvm::BasicBlock *>>
      dep2InfeBlockMap;

  std::list<ExtendedExecutionState *> firstLevelDepStates;

  std::list<ExtendedExecutionState *> secondLevelDepStates;

  uint64_t selectionTimes;

  std::shared_ptr<StateTravBlockMap> get(const llvm::Function *func,
                                         bool store = false);

public:
  BranchStateMap() = delete;
  BranchStateMap(
      std::function<bool(klee::KInstIterator, const llvm::BasicBlock *)>
          checkReach)
      : reachCheckFunction(checkReach), selectionTimes(0) {}
  BranchStateMap(const BranchStateMap &) = delete;
  BranchStateMap &operator=(const BranchStateMap &) = delete;
  ~BranchStateMap() = default;

  void
  add(const llvm::BasicBlock *infeBlock,
      const std::unordered_set<const llvm::BasicBlock *> &firstLevelDependee,
      const std::unordered_set<const llvm::BasicBlock *> &secondLevelDependee);

  void remove(const llvm::BasicBlock *infeBlock);

  ExtendedExecutionState *select();

  void addStates(ExtendedExecutionState *_state,
                 const std::vector<ExtendedExecutionState *> &_states);

  void removeState(ExtendedExecutionState *removedState);

  void updateState(ExtendedExecutionState *extState, StateStepType stepType);
};

struct ExtStateIDCompare {
  bool operator()(const ExtendedExecutionState *a,
                  const ExtendedExecutionState *b) const {
    if (!a->rawState || !b->rawState)
      return false;
    else
      return a->rawState->id < b->rawState->id;
  }
};

class SearcherHelper {
private:
  struct ExtStatePropRecord {
    std::unique_ptr<DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>
        coveredNewFeasibleStates;
    std::unique_ptr<DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>
        feasiblePathStates;
    std::unique_ptr<DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>
        coveredNewInfeasibleStates;
    std::unique_ptr<DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>
        infeasiblePathStates;

    MutualMap<ExtendedExecutionState *, const llvm::BasicBlock *>
        uncoveredStateBlockMap;

    ExtStatePropRecord()
        : coveredNewFeasibleStates(
              std::make_unique<
                  DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>()),
          feasiblePathStates(
              std::make_unique<
                  DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>()),
          coveredNewInfeasibleStates(
              std::make_unique<
                  DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>()),
          infeasiblePathStates(
              std::make_unique<
                  DiscretePDF<ExtendedExecutionState *, ExtStateIDCompare>>()) {
    }

    void add(ExtendedExecutionState *extState,
             const llvm::BasicBlock *uncoveredBlock = nullptr,
             bool feasiblePath = false);
    void erase(ExtendedExecutionState *extState);
    void erase(ExecutionState *state);
    void erase(const llvm::BasicBlock *coveredBlock);
    ExtendedExecutionState *select(unsigned rnd);
  };

private:
  std::shared_ptr<InterProcGraph> iCFG;
  std::shared_ptr<InterProcDataAnalyzer> iPDA;
  std::function<unsigned()> randomGenerator;

  std::unordered_set<ExecutionState *> generalStateSet;

  /// @brief Current selected state in our record
  klee::ExecutionState *currSelectedState;

  /// @brief Flag for reselection
  bool reSelectFlag;

  unsigned selectionTimes;

  /// @brief Visited basic blocks
  std::unordered_set<const llvm::BasicBlock *> visitedBasicBlocks;

  std::unique_ptr<BranchStateMap> branchDepStateMap;

  ExtStatePropRecord extStatesRecord;

  /// @brief An ordered map for state reselection
  std::map<uint32_t, std::unordered_set<ExtendedExecutionState *>>
      orderedStateMap;

  std::unique_ptr<DiscretePDF<ExecutionState *, ExecutionStateIDCompare>>
      parsedStates;

  bool isCoveredBranch(
      const llvm::BasicBlock *bblock,
      const std::unordered_set<const llvm::BasicBlock *> &successors);

public:
  SearcherHelper() = delete;
  SearcherHelper(std::shared_ptr<InterProcGraph> _iCFG,
                 std::shared_ptr<InterProcDataAnalyzer> _iPDA,
                 std::function<unsigned()> _rndGen);
  SearcherHelper(const SearcherHelper &) = delete;
  SearcherHelper &operator=(const SearcherHelper &) = delete;
  ~SearcherHelper() = default;

  bool empty() const { return generalStateSet.empty(); }

  void update(klee::ExecutionState *current,
              const std::vector<klee::ExecutionState *> &addedStates,
              const std::vector<klee::ExecutionState *> &removedStates);

  klee::ExecutionState &selectState();
};
} // namespace Empc
} // namespace klee

#endif