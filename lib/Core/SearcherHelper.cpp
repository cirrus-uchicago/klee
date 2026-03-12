//===-- SearcherHelpher.cpp -------------------------------------*- C++ -*-===//
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

#include "SearcherHelper.h"
#include "SearcherLog.h"

#include <iterator>

#include "klee/Module/KInstruction.h"

namespace klee {
namespace Empc {
namespace Utility {
llvm::Instruction *convert(KInstIterator instrIter) {
  return instrIter ? instrIter->inst : nullptr;
}

llvm::BasicBlock *convert(llvm::Instruction *instr) {
  return instr ? instr->getParent() : nullptr;
}
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::add(const _Tp1 &value1, const _Tp2 &value2) {
  map12[value1].emplace(value2);
  map21[value2].emplace(value1);
}

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::add(const _Tp1 &baseValue,
                                const std::vector<_Tp1> &addedValues, int __x) {
  if (map12.find(baseValue) == map12.end())
    return;

  for (const auto &value2 : map12.at(baseValue)) {
    for (const auto &value1 : addedValues) {
      add(value1, value2);
    }
  }
}

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::add(const _Tp2 &baseValue,
                                const std::vector<_Tp2> &addedValues, int __x,
                                int __y) {
  if (map21.find(baseValue) == map21.end())
    return;

  for (const auto &value1 : map21.at(baseValue)) {
    for (const auto &value2 : addedValues) {
      add(value1, value2);
    }
  }
}

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::remove(const _Tp1 &value1) {
  if (map12.find(value1) == map12.end())
    return;
  for (const auto &value2 : map12.at(value1)) {
    auto iter = map21.find(value2);
    if (iter != map21.end()) {
      iter->second.erase(value1);
      if (iter->second.empty())
        map21.erase(iter);
    }
  }
  map12.erase(value1);
}

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::remove(const _Tp2 &value2) {
  if (map21.find(value2) == map21.end())
    return;
  for (const auto &value1 : map21.at(value2)) {
    auto iter = map12.find(value1);
    if (iter != map12.end()) {
      iter->second.erase(value2);
      if (iter->second.empty())
        map12.erase(iter);
    }
  }
  map21.erase(value2);
}

template <typename _Tp1, typename _Tp2>
void MutualMap<_Tp1, _Tp2>::remove(const _Tp1 &value1, const _Tp2 &value2) {
  auto iter1 = map12.find(value1);
  auto iter2 = map21.find(value2);
  if (iter1 != map12.end()) {
    iter1->second.erase(value2);
    if (iter1->second.empty())
      map12.erase(iter1);
  }
  if (iter2 != map21.end()) {
    iter2->second.erase(value1);
    if (iter2->second.empty())
      map21.erase(iter2);
  }
}

std::unordered_map<const ExecutionState *,
                   std::unique_ptr<ExtendedExecutionState>>
    ExtendedExecutionState::stateMap;

ExtendedExecutionState *
ExtendedExecutionState::addExecutionState(ExecutionState *kleeState) {
  assert(kleeState);

  if (stateMap.find(kleeState) == stateMap.end()) {
    stateMap[kleeState] = std::make_unique<ExtendedExecutionState>(kleeState);
  }

  ExtendedExecutionState *resultState = stateMap.at(kleeState).get();

  resultState->prevBasicBlock =
      Utility::convert(Utility::convert(resultState->rawState->prevPC));
  resultState->nextBasicBlock =
      Utility::convert(Utility::convert(resultState->rawState->pc));

  return resultState;
}

void ExtendedExecutionState::removeExecutionState(
    const ExecutionState *kleeState) {
  stateMap.erase(kleeState);
}

bool ExtendedExecutionState::findExecutionState(
    const ExecutionState *kleeState) {
  return stateMap.find(kleeState) != stateMap.end();
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

void StateTravBlockMap::add(ExtendedExecutionState *_state) {
  if (!_state || !_state->nextBasicBlock ||
      (func && _state->nextBasicBlock->getParent() != func))
    return;
  MutualMap<ExtendedExecutionState *, const llvm::BasicBlock *>::add(
      _state, _state->nextBasicBlock);
}

void StateTravBlockMap::add(
    ExtendedExecutionState *_state,
    const std::vector<ExtendedExecutionState *> &_states) {
  // Program init
  if (!_state) {
    for (auto state : _states)
      add(state);
  } else {
    for (const auto &state : _states) {
      MutualMap<ExtendedExecutionState *, const llvm::BasicBlock *>::add(
          _state, _states, 0);
      add(state);
    }
    add(_state);
  }
}

std::shared_ptr<StateTravBlockMap>
BranchStateMap::get(const llvm::Function *func, bool store) {
  if (!store) {
    if (!func || funcStateTravMap.find(func) == funcStateTravMap.end())
      return nullptr;
    else
      return funcStateTravMap.at(func);
  } else {
    assert(func);

    std::shared_ptr<StateTravBlockMap> result = nullptr;
    if (funcStateTravMap.find(func) == funcStateTravMap.end()) {
      result = std::make_shared<StateTravBlockMap>(func);
      funcStateTravMap[func] = result;
    } else {
      result = funcStateTravMap.at(func);
    }

    return result;
  }
}

void BranchStateMap::add(
    const llvm::BasicBlock *infeBlock,
    const std::unordered_set<const llvm::BasicBlock *> &firstLevelDependee,
    const std::unordered_set<const llvm::BasicBlock *> &secondLevelDependee) {
  if (brBlockStateMap.find(infeBlock))
    return;

  std::unordered_set<const llvm::BasicBlock *> simple2LevelDependence;
  Utility::difference(simple2LevelDependence, firstLevelDependee,
                      secondLevelDependee);

  for (auto block : firstLevelDependee)
    dep1InfeBlockMap[block].emplace(infeBlock);
  for (auto block : secondLevelDependee)
    dep2InfeBlockMap[block].emplace(infeBlock);

  // Find available states
  for (auto depBlock : firstLevelDependee) {
    if (auto thisTravMap = get(depBlock->getParent())) {
      for (auto state : thisTravMap->at(depBlock)) {
        // // [DEBUG]
        // std::cout << "Before Checking" << std::endl;

        // Check reachability
        if (!state->rawState)
          continue;
        else if (reachCheckFunction(state->rawState->pc, infeBlock)) {
          // Add to recording
          brBlockStateMap.add(infeBlock, state);
          if (!Utility::find(firstLevelDepStates, state))
            firstLevelDepStates.push_front(state);
        }

        // std::cout << "After Checking" << std::endl;
      }
    }
  }

  for (auto depBlock : simple2LevelDependence) {
    if (auto thisTravMap = get(depBlock->getParent())) {
      for (auto state : thisTravMap->at(depBlock)) {
        // // [DEBUG]
        // std::cout << "Before Checking" << std::endl;

        if (!state->rawState)
          continue;
        else if (reachCheckFunction(state->rawState->pc, infeBlock)) {
          // Add to recording
          brBlockStateMap.add(infeBlock, state);
          if (!Utility::find(secondLevelDepStates, state))
            firstLevelDepStates.push_front(state);
        }

        // std::cout << "After Checking" << std::endl;
      }
    }
  }
}

void BranchStateMap::remove(const llvm::BasicBlock *infeBlock) {
  if (!infeBlock)
    return;

  auto removedStates = brBlockStateMap.at(infeBlock);
  brBlockStateMap.remove(infeBlock);
  for (auto state : removedStates) {
    if (brBlockStateMap.at(state).empty()) {
      firstLevelDepStates.remove(state);
      secondLevelDepStates.remove(state);
    }
  }
}

ExtendedExecutionState *BranchStateMap::select() {
  ++selectionTimes;
  if (selectionTimes % 4 && !firstLevelDepStates.empty()) {
    firstLevelDepStates.push_back(firstLevelDepStates.front());
    firstLevelDepStates.pop_front();
    return firstLevelDepStates.back();
  }
  if (!secondLevelDepStates.empty()) {
    secondLevelDepStates.push_back(secondLevelDepStates.front());
    secondLevelDepStates.pop_front();
    return secondLevelDepStates.back();
  }
  return nullptr;
}

void BranchStateMap::addStates(
    ExtendedExecutionState *_state,
    const std::vector<ExtendedExecutionState *> &_states) {
  if (_states.empty())
    return;
  else if (!_state)
    return;
  else {
    // Add to branch block map
    brBlockStateMap.add(_state, _states, 0, 0);

    // Add to function traverse block state map
    if (_state->nextBasicBlock) {
      if (auto thisTravMap = get(_state->nextBasicBlock->getParent()))
        thisTravMap->add(_state, _states);
    }
  }
}

void BranchStateMap::removeState(ExtendedExecutionState *removedState) {
  if (!removedState)
    return;

  // Remove from branch block map
  brBlockStateMap.remove(removedState);

  // Remove from the lists
  firstLevelDepStates.remove(removedState);
  secondLevelDepStates.remove(removedState);

  // Remove from func-trav map
  for (auto &funcTravPair : funcStateTravMap) {
    funcTravPair.second->remove(removedState);
  }
}

void BranchStateMap::updateState(ExtendedExecutionState *extState,
                                 StateStepType stepType) {

  if (!extState)
    return;

  /// Change some dependence info according to reachability
  auto removeStateDepInfo = [&]() {
    std::unordered_set<const llvm::BasicBlock *> removedBlocks;
    for (auto block : this->brBlockStateMap.at(extState)) {
      if (!this->reachCheckFunction(extState->rawState->pc, block)) {
        removedBlocks.emplace(block);
      }
    }
    for (auto block : removedBlocks) {
      this->brBlockStateMap.remove(block, extState);
    }

    if (this->brBlockStateMap.at(extState).empty()) {
      this->firstLevelDepStates.remove(extState);
      this->secondLevelDepStates.remove(extState);
    }
  };

  // Add some state dependence info
  auto addStateDepInfo = [&]() {
    if (!extState->nextBasicBlock)
      return;

    auto iter1 = dep1InfeBlockMap.find(extState->nextBasicBlock);
    auto iter2 = dep2InfeBlockMap.find(extState->nextBasicBlock);
    if (iter1 != this->dep1InfeBlockMap.end()) {
      for (auto infeBlock : iter1->second) {
        this->brBlockStateMap.add(infeBlock, extState);
      }
      if (Utility::find(this->firstLevelDepStates, extState))
        this->firstLevelDepStates.remove(extState);
      this->firstLevelDepStates.push_front(extState);
    }
    if (iter2 != this->dep2InfeBlockMap.end()) {
      for (auto infeBlock : iter2->second) {
        this->brBlockStateMap.add(infeBlock, extState);
      }

      if (Utility::find(this->secondLevelDepStates, extState))
        this->secondLevelDepStates.remove(extState);
      this->secondLevelDepStates.push_front(extState);
    }
  };

  if (stepType == StateStepType::PUSH || stepType == StateStepType::COMMON) {
    assert(extState->nextBasicBlock);

    // Change some state dependence info
    removeStateDepInfo();

    // Add traverse info
    get(extState->nextBasicBlock->getParent(), true)->add(extState);

    // Add dependence info
    addStateDepInfo();
  } else if (stepType == StateStepType::POP) {
    assert(extState->prevBasicBlock);

    // Remove the traverse info in the previous function
    {
      if (auto prevTravMap = get(extState->prevBasicBlock->getParent()))
        prevTravMap->remove(extState);
    }

    // Change some state dependence info
    removeStateDepInfo();
  }
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

void SearcherHelper::ExtStatePropRecord::add(
    klee::Empc::ExtendedExecutionState *extState,
    const llvm::BasicBlock *uncoveredBlock, bool feasiblePath) {
  if (coveredNewFeasibleStates->inTree(extState))
    coveredNewFeasibleStates->remove(extState);
  if (feasiblePathStates->inTree(extState))
    feasiblePathStates->remove(extState);
  if (coveredNewInfeasibleStates->inTree(extState))
    coveredNewInfeasibleStates->remove(extState);
  if (infeasiblePathStates->inTree(extState))
    infeasiblePathStates->remove(extState);

  if (uncoveredBlock && feasiblePath) {
    coveredNewFeasibleStates->insert(extState, extState->priority());
    feasiblePathStates->insert(extState, extState->priority());
    uncoveredStateBlockMap.add(extState, uncoveredBlock);
  } else if (uncoveredBlock) {
    coveredNewInfeasibleStates->insert(extState, extState->priority());
    infeasiblePathStates->insert(extState, extState->priority());
    uncoveredStateBlockMap.add(extState, uncoveredBlock);
  } else if (feasiblePath) {
    feasiblePathStates->insert(extState, extState->priority());
  } else {
    infeasiblePathStates->insert(extState, extState->priority());
  }
}

void SearcherHelper::ExtStatePropRecord::erase(
    ExtendedExecutionState *extState) {
  if (!extState)
    return;
  if (coveredNewFeasibleStates->inTree(extState))
    coveredNewFeasibleStates->remove(extState);
  if (feasiblePathStates->inTree(extState))
    feasiblePathStates->remove(extState);
  if (coveredNewInfeasibleStates->inTree(extState))
    coveredNewInfeasibleStates->remove(extState);
  if (infeasiblePathStates->inTree(extState))
    infeasiblePathStates->remove(extState);
}

void SearcherHelper::ExtStatePropRecord::erase(ExecutionState *state) {
  if (!state)
    return;
  erase(ExtendedExecutionState::addExecutionState(state));
}

void SearcherHelper::ExtStatePropRecord::erase(
    const llvm::BasicBlock *coveredBlock) {
  if (!coveredBlock)
    return;
  for (ExtendedExecutionState *extState :
       uncoveredStateBlockMap.at(coveredBlock)) {
    if (coveredNewFeasibleStates->inTree(extState)) {
      coveredNewFeasibleStates->remove(extState);
    }
    if (coveredNewInfeasibleStates->inTree(extState)) {
      coveredNewInfeasibleStates->remove(extState);
    }
  }
  uncoveredStateBlockMap.remove(coveredBlock);
}

ExtendedExecutionState *
SearcherHelper::ExtStatePropRecord::select(unsigned rnd) {
  double fRnd = (double)(rnd % UINT16_MAX) / UINT16_MAX;
  if (!coveredNewFeasibleStates->empty())
    return coveredNewFeasibleStates->choose(fRnd);
  if (!coveredNewInfeasibleStates->empty())
    return coveredNewInfeasibleStates->choose(fRnd);
  if (!feasiblePathStates->empty())
    return feasiblePathStates->choose(fRnd);
  if (!infeasiblePathStates->empty())
    return infeasiblePathStates->choose(fRnd);
  return nullptr;
}

SearcherHelper::SearcherHelper(std::shared_ptr<InterProcGraph> _iCFG,
                               std::shared_ptr<InterProcDataAnalyzer> _iPDA,
                               std::function<unsigned()> _rndGen)
    : iCFG(_iCFG), iPDA(_iPDA), randomGenerator(_rndGen),
      currSelectedState(nullptr), reSelectFlag(true), selectionTimes(0),
      parsedStates(std::make_unique<
                   DiscretePDF<ExecutionState *, ExecutionStateIDCompare>>()) {
  branchDepStateMap = std::make_unique<BranchStateMap>(
      [&](klee::KInstIterator instIter,
          const llvm::BasicBlock *bblock) -> bool {
        auto *inst = Utility::convert(instIter);
        if (!inst || !bblock)
          return false;
        return this->iPDA->isRechable(inst, bblock->getTerminator());
      });
}

bool SearcherHelper::isCoveredBranch(
    const llvm::BasicBlock *bblock,
    const std::unordered_set<const llvm::BasicBlock *> &successors) {
  if (!bblock)
    return true;
  auto realSuccessors = iPDA->getSuccessors(bblock);
  Utility::difference(realSuccessors, successors);
  for (auto succ : realSuccessors) {
    if (visitedBasicBlocks.find(succ) == visitedBasicBlocks.end())
      return false;
  }
  return true;
}

void SearcherHelper::update(
    klee::ExecutionState *current,
    const std::vector<klee::ExecutionState *> &addedStates,
    const std::vector<klee::ExecutionState *> &removedStates) {

  std::unordered_set<klee::ExecutionState *> reallyRemovedStates(
      removedStates.cbegin(), removedStates.cend());
  std::unordered_set<ExtendedExecutionState *> handlingExtStates;
  std::vector<ExtendedExecutionState *> addedExtStates;
  ExtendedExecutionState *currExtState = nullptr;
  std::string logStr;
  bool hasSteppedStates = false;
  bool isInDefinedFunctions = false;

  // Add states
  if (Logging::check(Logging::Type::STATE)) {
    logStr += "Added States (" + std::to_string(addedStates.size()) + ")\n";
  }
  for (auto addedState : addedStates) {
    generalStateSet.emplace(addedState);
    auto addedExtState = ExtendedExecutionState::addExecutionState(addedState);

    handlingExtStates.emplace(addedExtState);
    addedExtStates.push_back(addedExtState);

    uint32_t statePrior = addedExtState->rawState->depth;
    auto mapIter = orderedStateMap.find(statePrior);
    if (mapIter == orderedStateMap.end()) {
      auto insertResult = orderedStateMap.insert(std::make_pair(
          statePrior, std::unordered_set<ExtendedExecutionState *>()));
      assert(insertResult.second);
      mapIter = insertResult.first;
    }
    mapIter->second.emplace(addedExtState);

    // Discrete
    parsedStates->insert(addedState, std::pow(0.6, statePrior));

    if (Logging::check(Logging::Type::STATE)) {
      logStr +=
          "(" + std::to_string(addedState->id) + ") Prev: " +
          Utility::getBasicBlockName(addedExtState->prevBasicBlock, true) +
          " | Next: " +
          Utility::getBasicBlockName(addedExtState->nextBasicBlock, true) +
          "\n";
    }
  }

  // Pre-Evolve
  if (!currSelectedState && !current) {
    reSelectFlag = true;
  } else if (!currSelectedState) {
    reallyRemovedStates.emplace(current);
    reSelectFlag = true;
  } else if (!current) {
    reallyRemovedStates.emplace(currSelectedState);
    currSelectedState = nullptr;
    reSelectFlag = true;
  } else if (current != currSelectedState) {
    reallyRemovedStates.emplace(currSelectedState);
    reallyRemovedStates.emplace(current);
    currSelectedState = nullptr;
    reSelectFlag = true;
  } else if (reallyRemovedStates.find(currSelectedState) !=
             reallyRemovedStates.end()) {
    currSelectedState = nullptr;
    reSelectFlag = true;
  } else {
    currExtState = ExtendedExecutionState::addExecutionState(currSelectedState);
    handlingExtStates.emplace(currExtState);

    reSelectFlag = false;
    currSelectedState = nullptr;
  }

  //
  // Evolve

  std::unordered_set<ExtendedExecutionState *> choosenExtStates =
      handlingExtStates;
  const llvm::BasicBlock *prevBasicBlock = nullptr;
  std::unordered_set<const llvm::BasicBlock *> steppedSuccessors;

  // Check whether the state is in defined functions
  if (currExtState) {
    if (currExtState->nextBasicBlock &&
        !iCFG->realEmpty(currExtState->nextBasicBlock->getParent()))
      isInDefinedFunctions = true;
    if (currExtState->prevBasicBlock &&
        !iCFG->realEmpty(currExtState->prevBasicBlock->getParent()))
      isInDefinedFunctions = true;
  }

  // Add states to data dependence recording
  if (currExtState && isInDefinedFunctions) {
    branchDepStateMap->addStates(currExtState, addedExtStates);
    prevBasicBlock = currExtState->prevBasicBlock;
  }

  // Step basic block
  for (ExtendedExecutionState *extState : handlingExtStates) {
    if (extState->prevBasicBlock != extState->nextBasicBlock) {
      hasSteppedStates = true;
      if (extState->prevBasicBlock) {
        // Add to visited block set
        visitedBasicBlocks.emplace(extState->prevBasicBlock);

        // Remove in state priority set
        extStatesRecord.erase(extState->prevBasicBlock);
      }
      if (extState->nextBasicBlock)
        steppedSuccessors.emplace(extState->nextBasicBlock);

      // [DEBUG]
      if (Logging::check(Logging::Type::STATE)) {
        logStr += "Query (" + std::to_string(extState->rawState->id) + ")\n";
      }

      bool unvisited = extState->nextBasicBlock &&
                       visitedBasicBlocks.find(extState->nextBasicBlock) ==
                           visitedBasicBlocks.end();
      bool success = iCFG->queryStepBasicBlock(
          extState->prevBasicBlock, extState->nextBasicBlock,
          extState->pathReservedInfo, extState->rawState->mpcStateStepType);
      if (!success) {
        choosenExtStates.erase(extState);
      }

      iCFG->ensureStepBasicBlock(extState->nextBasicBlock
                                     ? extState->nextBasicBlock
                                     : extState->prevBasicBlock,
                                 extState->pathReservedInfo, true);

      extStatesRecord.add(
          extState, unvisited ? extState->nextBasicBlock : nullptr, success);

      // Update state in data dependence recording
      if (isInDefinedFunctions)
        branchDepStateMap->updateState(extState,
                                       extState->rawState->mpcStateStepType);

      // Add indirect call in data dependence recording
      if (extState->rawState->mpcStateStepType == StateStepType::PUSH)
        iPDA->addCallee(extState->prevBasicBlock, extState->nextBasicBlock);
    }

    // Refresh step type
    extState->rawState->mpcStateStepType = Empc::StateStepType::COMMON;
  }

  // Find an infeasible branch to add to branch map
  if (isInDefinedFunctions && prevBasicBlock && hasSteppedStates &&
      !iCFG->empty(prevBasicBlock->getParent())) {
    //
    if (!isCoveredBranch(prevBasicBlock, steppedSuccessors)) {
      if (auto *bblockData =
              BasicBlockData::getBasicBlockData(prevBasicBlock)) {
        auto firstLevelDep = bblockData->firstLevelDepBlocks;
        auto secondLevelDep = bblockData->secondLevelDepBlocks;

        for (const auto &varDepPair : bblockData->firstLevelDependence) {
          if (varDepPair.first->kind == Variable::Kind::RETURN) {
            auto var = varDepPair.first;
            if (auto *inst =
                    llvm::dyn_cast<llvm::Instruction>(var->definition)) {
              if (auto *callerData = CallerData::getCallerData(inst)) {
                Utility::merge(secondLevelDep, callerData->depBlocks);
              }
              if (auto *funcData =
                      FunctionData::getFunctionData(var->calleeFunc)) {
                Utility::merge(secondLevelDep, funcData->depBlocks);
              }
            }
          }
        }

        if (Logging::check(Logging::Type::DEBUG)) {
          Logging::all(Logging::Type::DEBUG, "Check adding branch for tracing",
                       "Added: " + Utility::getBasicBlockName(prevBasicBlock));
        }

        branchDepStateMap->add(prevBasicBlock, firstLevelDep, secondLevelDep);
      }
    } else {
      branchDepStateMap->remove(prevBasicBlock);
    }
  }

  // Choose from an available group
  if (choosenExtStates.empty()) {
    reSelectFlag = true;
    currSelectedState = nullptr;
  } else {
    auto selectedExtStateIter = choosenExtStates.begin();
    std::advance(selectedExtStateIter,
                 randomGenerator() % choosenExtStates.size());

    auto selectedExtState = *selectedExtStateIter;
    currSelectedState = selectedExtState->rawState;

    extStatesRecord.erase(selectedExtState);
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::STATE)) {
    logStr += "Selected State: ";
    if (reSelectFlag)
      logStr += "null (reselection)";
    else
      logStr += "(" + std::to_string(currSelectedState->id) + ")";
    logStr += "\n";
  }

  // Remove
  // [DEBUG]
  if (Logging::check(Logging::Type::STATE)) {
    logStr +=
        "Removed States (" + std::to_string(reallyRemovedStates.size()) + ")\n";
  }
  for (auto removedState : reallyRemovedStates) {
    extStatesRecord.erase(removedState);

    // Remove from dependence recording
    if (ExtendedExecutionState::findExecutionState(removedState))
      branchDepStateMap->removeState(
          ExtendedExecutionState::addExecutionState(removedState));

    ExtendedExecutionState::removeExecutionState(removedState);
    generalStateSet.erase(removedState);

    // Discrete
    parsedStates->remove(removedState);

    // [DEBUG]
    if (Logging::check(Logging::Type::STATE)) {
      logStr += "(" + std::to_string(removedState->id) + ")\n";
    }
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::STATE)) {
    if (!addedStates.empty() || hasSteppedStates ||
        !reallyRemovedStates.empty()) {
      Logging::start(Logging::Type::STATE, "UPDATE");
      Logging::log(logStr);
      Logging::stop();
    }
  }
}

klee::ExecutionState &SearcherHelper::selectState() {
  if (!reSelectFlag)
    return *currSelectedState;

  // Re-select
  // auto mapIter = orderedStateMap.begin();
  ExtendedExecutionState *currExtState = nullptr;
  ++selectionTimes;

  // Find a state from branch state dependence record
  if (!currExtState && selectionTimes % 2)
    currExtState = branchDepStateMap->select();

  // Find a state covering unvisited blocks with a feasible path
  if (!currExtState)
    currExtState = extStatesRecord.select(randomGenerator());

  if (!currExtState)
    currExtState =
        ExtendedExecutionState::addExecutionState(parsedStates->choose(
            (double)(randomGenerator() % UINT16_MAX) / UINT16_MAX));

  assert(currExtState);

  if (Logging::check(Logging::Type::STATE)) {
    if (reSelectFlag) {
      Logging::start(Logging::Type::STATE, "SELECT");
      Logging::log() << "(" << currExtState->rawState->id << ") Prev: "
                     << Utility::getBasicBlockName(currExtState->prevBasicBlock,
                                                   true)
                     << " | Next: "
                     << Utility::getBasicBlockName(currExtState->nextBasicBlock,
                                                   true)
                     << "\n";
      Logging::stop();
    }
  }

  currSelectedState = currExtState->rawState;
  reSelectFlag = false;

  return *currSelectedState;
}
} // namespace Empc
} // namespace klee