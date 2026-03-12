//===-- Searcher.cpp ------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Searcher.h"

#include "CoreStats.h"
#include "ExecutionState.h"
#include "ExecutionTree.h"
#include "Executor.h"
#include "MergeHandler.h"
#include "StatsTracker.h"
#include "SearcherDefs.h"

#include "klee/ADT/DiscretePDF.h"
#include "klee/ADT/RNG.h"
#include "klee/Statistics/Statistics.h"
#include "klee/Module/InstructionInfoTable.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Support/ErrorHandling.h"
#include "klee/System/Time.h"

#include "klee/Support/CompilerWarning.h"
DISABLE_WARNING_PUSH
DISABLE_WARNING_DEPRECATED_DECLARATIONS
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
DISABLE_WARNING_POP

#include <cassert>
#include <climits>
#include <cmath>

using namespace klee;
using namespace llvm;

namespace klee {
extern llvm::cl::OptionCategory SearchCat;
}

cl::opt<std::string> MaxReviveTime(
    "max-revive-time",
    cl::desc("Maximum time to spend reviving a pending state (default unlimited)"),
    cl::cat(SearchCat));

cl::opt<int> ZestiBound(
    "zesti-bound-mul",
    cl::init(2),
    cl::desc("Bounds multiplier for ZESTI bounded exploration (default=2)"),
    cl::cat(SearchCat));

///

ExecutionState &DFSSearcher::selectState() {
  return *states.back();
}

void DFSSearcher::update(ExecutionState *current,
                         const std::vector<ExecutionState *> &addedStates,
                         const std::vector<ExecutionState *> &removedStates) {
  // insert states
  states.insert(states.end(), addedStates.begin(), addedStates.end());

  // remove states
  for (const auto state : removedStates) {
    if (state == states.back()) {
      states.pop_back();
    } else {
      auto it = std::find(states.begin(), states.end(), state);
      assert(it != states.end() && "invalid state removed");
      states.erase(it);
    }
  }
}

bool DFSSearcher::empty() {
  return states.empty();
}

void DFSSearcher::printName(llvm::raw_ostream &os) {
  os << "DFSSearcher\n";
}


///

ExecutionState &BFSSearcher::selectState() {
  return *states.front();
}

void BFSSearcher::update(ExecutionState *current,
                         const std::vector<ExecutionState *> &addedStates,
                         const std::vector<ExecutionState *> &removedStates) {
  // update current state
  // Assumption: If new states were added KLEE forked, therefore states evolved.
  // constraints were added to the current state, it evolved.
  if (!addedStates.empty() && current &&
      std::find(removedStates.begin(), removedStates.end(), current) == removedStates.end()) {
    auto pos = std::find(states.begin(), states.end(), current);
    assert(pos != states.end());
    states.erase(pos);
    states.push_back(current);
  }

  // insert states
  states.insert(states.end(), addedStates.begin(), addedStates.end());

  // remove states
  for (const auto state : removedStates) {
    if (state == states.front()) {
      states.pop_front();
    } else {
      auto it = std::find(states.begin(), states.end(), state);
      assert(it != states.end() && "invalid state removed");
      states.erase(it);
    }
  }
}

bool BFSSearcher::empty() {
  return states.empty();
}

void BFSSearcher::printName(llvm::raw_ostream &os) {
  os << "BFSSearcher\n";
}


///

RandomSearcher::RandomSearcher(RNG &rng) : theRNG{rng} {}

ExecutionState &RandomSearcher::selectState() {
  return *states[theRNG.getInt32() % states.size()];
}

void RandomSearcher::update(ExecutionState *current,
                            const std::vector<ExecutionState *> &addedStates,
                            const std::vector<ExecutionState *> &removedStates) {
  // insert states
  states.insert(states.end(), addedStates.begin(), addedStates.end());

  // remove states
  for (const auto state : removedStates) {
    auto it = std::find(states.begin(), states.end(), state);
    assert(it != states.end() && "invalid state removed");
    states.erase(it);
  }
}

bool RandomSearcher::empty() {
  return states.empty();
}

void RandomSearcher::printName(llvm::raw_ostream &os) {
  os << "RandomSearcher\n";
}


///

WeightedRandomSearcher::WeightedRandomSearcher(WeightType type, RNG &rng)
  : states(std::make_unique<DiscretePDF<ExecutionState*, ExecutionStateIDCompare>>()),
    theRNG{rng},
    type(type) {

  switch(type) {
  case Depth:
  case RP:
    updateWeights = false;
    break;
  case InstCount:
  case CPInstCount:
  case QueryCost:
  case MinDistToUncovered:
  case CoveringNew:
    updateWeights = true;
    break;
  default:
    assert(0 && "invalid weight type");
  }
}

ExecutionState &WeightedRandomSearcher::selectState() {
  return *states->choose(theRNG.getDoubleL());
}

double WeightedRandomSearcher::getWeight(ExecutionState *es) {
  switch(type) {
    default:
    case Depth:
      return es->depth;
    case RP:
      return std::pow(0.5, es->depth);
    case InstCount: {
      uint64_t count = theStatisticManager->getIndexedValue(stats::instructions,
                                                            es->pc->info->id);
      double inv = 1. / std::max((uint64_t) 1, count);
      return inv * inv;
    }
    case CPInstCount: {
      StackFrame &sf = es->stack.back();
      uint64_t count = sf.callPathNode->statistics.getValue(stats::instructions);
      double inv = 1. / std::max((uint64_t) 1, count);
      return inv;
    }
    case QueryCost:
      return (es->queryMetaData.queryCost.toSeconds() < .1)
                 ? 1.
                 : 1. / es->queryMetaData.queryCost.toSeconds();
    case CoveringNew:
    case MinDistToUncovered: {
      uint64_t md2u = computeMinDistToUncovered(es->pc,
                                                es->stack.back().minDistToUncoveredOnReturn);

      double invMD2U = 1. / (md2u ? md2u : 10000);
      if (type == CoveringNew) {
        double invCovNew = 0.;
        if (es->instsSinceCovNew)
          invCovNew = 1. / std::max(1, (int) es->instsSinceCovNew - 1000);
        return (invCovNew * invCovNew + invMD2U * invMD2U);
      } else {
        return invMD2U * invMD2U;
      }
    }
  }
}

void WeightedRandomSearcher::update(ExecutionState *current,
                                    const std::vector<ExecutionState *> &addedStates,
                                    const std::vector<ExecutionState *> &removedStates) {

  // update current
  if (current && updateWeights &&
      std::find(removedStates.begin(), removedStates.end(), current) == removedStates.end())
    states->update(current, getWeight(current));

  // insert states
  for (const auto state : addedStates)
    states->insert(state, getWeight(state));

  // remove states
  for (const auto state : removedStates)
    states->remove(state);
}

bool WeightedRandomSearcher::empty() {
  return states->empty();
}

void WeightedRandomSearcher::printName(llvm::raw_ostream &os) {
  os << "WeightedRandomSearcher::";
  switch(type) {
    case Depth              : os << "Depth\n"; return;
    case RP                 : os << "RandomPath\n"; return;
    case QueryCost          : os << "QueryCost\n"; return;
    case InstCount          : os << "InstCount\n"; return;
    case CPInstCount        : os << "CPInstCount\n"; return;
    case MinDistToUncovered : os << "MinDistToUncovered\n"; return;
    case CoveringNew        : os << "CoveringNew\n"; return;
    default                 : os << "<unknown type>\n"; return;
  }
}


///

// Check if n is a valid pointer and a node belonging to us
#define IS_OUR_NODE_VALID(n)                                                   \
  (((n).getPointer() != nullptr) && (((n).getInt() & idBitMask) != 0))

RandomPathSearcher::RandomPathSearcher(InMemoryExecutionTree *executionTree, RNG &rng)
    : executionTree{executionTree}, theRNG{rng},
      idBitMask{static_cast<uint8_t>(executionTree ? executionTree->getNextId() : 0)} {
  assert(executionTree);
};

ExecutionState &RandomPathSearcher::selectState() {
  unsigned flips=0, bits=0;
  assert(executionTree->root.getInt() & idBitMask &&
         "Root should belong to the searcher");
  ExecutionTreeNode *n = executionTree->root.getPointer();
  while (!n->state) {
    if (!IS_OUR_NODE_VALID(n->left)) {
      assert(IS_OUR_NODE_VALID(n->right) && "Both left and right nodes invalid");
      assert(n != n->right.getPointer());
      n = n->right.getPointer();
    } else if (!IS_OUR_NODE_VALID(n->right)) {
      assert(IS_OUR_NODE_VALID(n->left) && "Both right and left nodes invalid");
      assert(n != n->left.getPointer());
      n = n->left.getPointer();
    } else {
      if (bits==0) {
        flips = theRNG.getInt32();
        bits = 32;
      }
      --bits;
      n = ((flips & (1U << bits)) ? n->left : n->right).getPointer();
    }
  }

  return *n->state;
}

void RandomPathSearcher::update(ExecutionState *current,
                                const std::vector<ExecutionState *> &addedStates,
                                const std::vector<ExecutionState *> &removedStates) {
  // insert states
  for (auto es : addedStates) {
    ExecutionTreeNode *etnode = es->executionTreeNode, *parent = etnode->parent;
    ExecutionTreeNodePtr *childPtr;

    childPtr = parent ? ((parent->left.getPointer() == etnode) ? &parent->left
                                                               : &parent->right)
                      : &executionTree->root;
    while (etnode && !IS_OUR_NODE_VALID(*childPtr)) {
      childPtr->setInt(childPtr->getInt() | idBitMask);
      etnode = parent;
      if (etnode)
        parent = etnode->parent;

      childPtr = parent
                     ? ((parent->left.getPointer() == etnode) ? &parent->left
                                                              : &parent->right)
                     : &executionTree->root;
    }
  }

  // remove states
  for (auto es : removedStates) {
    ExecutionTreeNode *etnode = es->executionTreeNode, *parent = etnode->parent;

    while (etnode && !IS_OUR_NODE_VALID(etnode->left) &&
           !IS_OUR_NODE_VALID(etnode->right)) {
      auto childPtr =
          parent ? ((parent->left.getPointer() == etnode) ? &parent->left
                                                          : &parent->right)
                 : &executionTree->root;
      assert(IS_OUR_NODE_VALID(*childPtr) &&
             "Removing executionTree child not ours");
      childPtr->setInt(childPtr->getInt() & ~idBitMask);
      etnode = parent;
      if (etnode)
        parent = etnode->parent;
    }
  }
}

bool RandomPathSearcher::empty() {
  return !IS_OUR_NODE_VALID(executionTree->root);
}

void RandomPathSearcher::printName(llvm::raw_ostream &os) {
  os << "RandomPathSearcher\n";
}


///

MergingSearcher::MergingSearcher(Searcher *baseSearcher)
  : baseSearcher{baseSearcher} {};

void MergingSearcher::pauseState(ExecutionState &state) {
  assert(std::find(pausedStates.begin(), pausedStates.end(), &state) == pausedStates.end());
  pausedStates.push_back(&state);
  baseSearcher->update(nullptr, {}, {&state});
}

void MergingSearcher::continueState(ExecutionState &state) {
  auto it = std::find(pausedStates.begin(), pausedStates.end(), &state);
  assert(it != pausedStates.end());
  pausedStates.erase(it);
  baseSearcher->update(nullptr, {&state}, {});
}

ExecutionState& MergingSearcher::selectState() {
  assert(!baseSearcher->empty() && "base searcher is empty");

  if (!UseIncompleteMerge)
    return baseSearcher->selectState();

  // Iterate through all MergeHandlers
  for (auto cur_mergehandler: mergeGroups) {
    // Find one that has states that could be released
    if (!cur_mergehandler->hasMergedStates()) {
      continue;
    }
    // Find a state that can be prioritized
    ExecutionState *es = cur_mergehandler->getPrioritizeState();
    if (es) {
      return *es;
    } else {
      if (DebugLogIncompleteMerge){
        llvm::errs() << "Preemptively releasing states\n";
      }
      // If no state can be prioritized, they all exceeded the amount of time we
      // are willing to wait for them. Release the states that already arrived at close_merge.
      cur_mergehandler->releaseStates();
    }
  }
  // If we were not able to prioritize a merging state, just return some state
  return baseSearcher->selectState();
}

void MergingSearcher::update(ExecutionState *current,
                             const std::vector<ExecutionState *> &addedStates,
                             const std::vector<ExecutionState *> &removedStates) {
  // We have to check if the current execution state was just deleted, as to
  // not confuse the nurs searchers
  if (std::find(pausedStates.begin(), pausedStates.end(), current) == pausedStates.end()) {
    baseSearcher->update(current, addedStates, removedStates);
  }
}

bool MergingSearcher::empty() {
  return baseSearcher->empty();
}

void MergingSearcher::printName(llvm::raw_ostream &os) {
  os << "MergingSearcher\n";
}


///

BatchingSearcher::BatchingSearcher(Searcher *baseSearcher,
                                   time::Span timeBudget,
                                   unsigned instructionBudget)
    : baseSearcher{baseSearcher}, timeBudgetEnabled{timeBudget},
      timeBudget{timeBudget}, instructionBudgetEnabled{instructionBudget > 0},
      instructionBudget{instructionBudget} {};

bool BatchingSearcher::withinTimeBudget() const {
  return !timeBudgetEnabled ||
         (time::getWallTime() - lastStartTime) <= timeBudget;
}

bool BatchingSearcher::withinInstructionBudget() const {
  return !instructionBudgetEnabled ||
         (stats::instructions - lastStartInstructions) <= instructionBudget;
}

ExecutionState &BatchingSearcher::selectState() {
  if (lastState && withinTimeBudget() && withinInstructionBudget()) {
    // return same state for as long as possible
    return *lastState;
  }

  // ensure time budget is larger than time between two calls (for same state)
  if (lastState && timeBudgetEnabled) {
    time::Span delta = time::getWallTime() - lastStartTime;
    auto t = timeBudget;
    t *= 1.1;
    if (delta > t) {
      klee_message("increased time budget from %f to %f\n",
                   timeBudget.toSeconds(), delta.toSeconds());
      timeBudget = delta;
    }
  }

  // pick a new state
  lastState = &baseSearcher->selectState();
  if (timeBudgetEnabled) {
    lastStartTime = time::getWallTime();
  }
  if (instructionBudgetEnabled) {
    lastStartInstructions = stats::instructions;
  }
  return *lastState;
}

void BatchingSearcher::update(ExecutionState *current,
                              const std::vector<ExecutionState *> &addedStates,
                              const std::vector<ExecutionState *> &removedStates) {
  // drop memoized state if it is marked for deletion
  if (std::find(removedStates.begin(), removedStates.end(), lastState) != removedStates.end())
    lastState = nullptr;
  // update underlying searcher
  baseSearcher->update(current, addedStates, removedStates);
}

bool BatchingSearcher::empty() {
  return baseSearcher->empty();
}

void BatchingSearcher::printName(llvm::raw_ostream &os) {
  os << "<BatchingSearcher> timeBudget: " << timeBudget
     << ", instructionBudget: " << instructionBudget
     << ", baseSearcher:\n";
  baseSearcher->printName(os);
  os << "</BatchingSearcher>\n";
}


///

IterativeDeepeningTimeSearcher::IterativeDeepeningTimeSearcher(Searcher *baseSearcher)
  : baseSearcher{baseSearcher} {};

ExecutionState &IterativeDeepeningTimeSearcher::selectState() {
  ExecutionState &res = baseSearcher->selectState();
  startTime = time::getWallTime();
  return res;
}

void IterativeDeepeningTimeSearcher::update(ExecutionState *current,
                                            const std::vector<ExecutionState *> &addedStates,
                                            const std::vector<ExecutionState *> &removedStates) {

  const auto elapsed = time::getWallTime() - startTime;

  // update underlying searcher (filter paused states unknown to underlying searcher)
  if (!removedStates.empty()) {
    std::vector<ExecutionState *> alt = removedStates;
    for (const auto state : removedStates) {
      auto it = pausedStates.find(state);
      if (it != pausedStates.end()) {
        pausedStates.erase(it);
        alt.erase(std::remove(alt.begin(), alt.end(), state), alt.end());
      }
    }    
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }

  // update current: pause if time exceeded
  if (current &&
      std::find(removedStates.begin(), removedStates.end(), current) == removedStates.end() &&
      elapsed > time) {
    pausedStates.insert(current);
    baseSearcher->update(nullptr, {}, {current});
  }

  // no states left in underlying searcher: fill with paused states
  if (baseSearcher->empty()) {
    time *= 2U;
    klee_message("increased time budget to %f\n", time.toSeconds());
    std::vector<ExecutionState *> ps(pausedStates.begin(), pausedStates.end());
    baseSearcher->update(nullptr, ps, std::vector<ExecutionState *>());
    pausedStates.clear();
  }
}

bool IterativeDeepeningTimeSearcher::empty() {
  return baseSearcher->empty() && pausedStates.empty();
}

void IterativeDeepeningTimeSearcher::printName(llvm::raw_ostream &os) {
  os << "IterativeDeepeningTimeSearcher\n";
}


///

InterleavedSearcher::InterleavedSearcher(const std::vector<Searcher*> &_searchers) {
  searchers.reserve(_searchers.size());
  for (auto searcher : _searchers)
    searchers.emplace_back(searcher);
}

ExecutionState &InterleavedSearcher::selectState() {
  Searcher *s = searchers[--index].get();
  if (index == 0) index = searchers.size();
  return s->selectState();
}

void InterleavedSearcher::update(ExecutionState *current,
                                 const std::vector<ExecutionState *> &addedStates,
                                 const std::vector<ExecutionState *> &removedStates) {

  // update underlying searchers
  for (auto &searcher : searchers)
    searcher->update(current, addedStates, removedStates);
}

bool InterleavedSearcher::empty() {
  return searchers[0]->empty();
}

void InterleavedSearcher::printName(llvm::raw_ostream &os) {
  os << "<InterleavedSearcher> containing " << searchers.size() << " searchers:\n";
  for (const auto &searcher : searchers)
    searcher->printName(os);
  os << "</InterleavedSearcher>\n";
}

// [SGS]: SubpathGuidedSearcher implementation
SubpathGuidedSearcher::SubpathGuidedSearcher(Executor &_executor, uint index,
                                             RNG &_rng)
    : executor(_executor), index(index), theRNG(_rng) {}

ExecutionState &SubpathGuidedSearcher::selectState() {
  unsigned long minCount = ULONG_MAX;
  std::vector<ExecutionState *> selectSet;
  for (auto &state : states) {
    subpath_ty subpath;
    executor.getSubpath(state, subpath, index);
    unsigned long curr = executor.getSubpathCount(subpath, index);
    if (curr < minCount) {
      selectSet.clear();
      minCount = curr;
    }

    if (curr == minCount) {
      selectSet.push_back(state);
    }
  }

  unsigned int random = theRNG.getInt32() % selectSet.size();
  ExecutionState *selection = selectSet[random];

  {
    subpath_ty subpath;
    executor.getSubpath(selection, subpath, index);
    executor.incSubpath(subpath, index);
  }

  return *selection;
}

void SubpathGuidedSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  states.insert(states.end(), addedStates.begin(), addedStates.end());
  std::set<ExecutionState *> removed;
  for (std::vector<ExecutionState *>::const_iterator it = removedStates.begin(),
                                                     ie = removedStates.end();
       it != ie; ++it) {
    ExecutionState *es = *it;
    if (removed.find(es) != removed.end())
      continue;
    __attribute__((unused)) bool ok = false;

    for (std::vector<ExecutionState *>::iterator it = states.begin(),
                                                 ie = states.end();
         it != ie; ++it) {
      if (es == *it) {
        states.erase(it);
        ok = true;
        break;
      }
    }

    assert(ok && "invalid state removed");
    removed.insert(es);
  }
}

void SubpathGuidedSearcher::printName(llvm::raw_ostream &os) {
  os << "Subpath Guided Searcher\n";
}

///

SwappingSearcher::SwappingSearcher(Searcher *s1, Searcher *s2,
                                   std::function<void()> cb)
    : swapCallback(std::move(cb)) {
  searchers[0].reset(s1);
  searchers[1].reset(s2);
}

ExecutionState &SwappingSearcher::selectState() {
  return searchers[currentSearcher]->selectState();
}

void SwappingSearcher::update(ExecutionState *current,
                              const std::vector<ExecutionState *> &addedStates,
                              const std::vector<ExecutionState *> &removedStates) {
  for (unsigned i = currentSearcher; i < 2; i++)
    searchers[i]->update(current, addedStates, removedStates);
}

bool SwappingSearcher::empty() {
  bool ret = searchers[currentSearcher]->empty();
  if (currentSearcher == 0 && ret) {
    currentSearcher++;
    swapCallback();
    return empty();
  }
  return ret;
}

void SwappingSearcher::printName(llvm::raw_ostream &os) {
  os << "<SwappingSearcher>\n";
  for (unsigned i = 0; i < 2; i++)
    searchers[i]->printName(os);
  os << "</SwappingSearcher>\n";
}

///

PendingSearcher::PendingSearcher(Searcher *baseNormal, Searcher *basePending,
                                 Executor &exec)
    : baseNormalSearcher(baseNormal), basePendingSearcher(basePending),
      executor(exec) {
  maxReviveTime = time::Span(MaxReviveTime);
}

ExecutionState &PendingSearcher::selectState() {
  return baseNormalSearcher->selectState();
}

bool PendingSearcher::empty() {
  if (!baseNormalSearcher->empty())
    return false;

  executor.solver->setTimeout(maxReviveTime);
  while (baseNormalSearcher->empty()) {
    if (basePendingSearcher->empty()) {
      executor.solver->setTimeout(time::Span());
      return true;
    }
    auto &es = basePendingSearcher->selectState();
    if (executor.attemptToRevive(es)) {
      // State revived — move from pending to normal
      baseNormalSearcher->update(nullptr, {&es}, {});
      basePendingSearcher->update(nullptr, {}, {&es});
    } else {
      // Infeasible — clear pending constraint, remove, and terminate
      es.pendingConstraint = nullptr;
      basePendingSearcher->update(nullptr, {}, {&es});
      executor.terminateState(es, StateTerminationType::SilentExit);
    }
  }
  executor.solver->setTimeout(time::Span());
  return baseNormalSearcher->empty() && basePendingSearcher->empty();
}

void PendingSearcher::update(ExecutionState *current,
                             const std::vector<ExecutionState *> &addedStates,
                             const std::vector<ExecutionState *> &removedStates) {
  auto is_pending = [](const auto &es) {
    return !es->pendingConstraint.isNull();
  };
  std::vector<ExecutionState *> addedN, addedP, removedN, removedP;

  for (const auto &es : addedStates) {
    if (is_pending(es))
      addedP.push_back(es);
    else
      addedN.push_back(es);
  }

  for (const auto &es : removedStates) {
    if (is_pending(es))
      removedP.push_back(es);
    else
      removedN.push_back(es);
  }

  // If current became pending during execution, move it
  if (current && is_pending(current)) {
    removedN.push_back(current);
    addedP.push_back(current);
  }

  baseNormalSearcher->update(current, addedN, removedN);
  basePendingSearcher->update(nullptr, addedP, removedP);
}

void PendingSearcher::printName(llvm::raw_ostream &os) {
  os << "<PendingSearcher>\n";
  baseNormalSearcher->printName(os);
  basePendingSearcher->printName(os);
  os << "</PendingSearcher>\n";
}

///

ZESTIPendingSearcher::ZESTIPendingSearcher(Executor &exec)
    : executor(exec), normalSearcher(std::make_unique<DFSSearcher>()) {}

void ZESTIPendingSearcher::computeDistances() {
  for (const auto &pEs : pendingStates) {
    int smallestDistance = 999999;
    for (const auto &sDepth : executor.senstiveDepths) {
      int diff = sDepth - (int)pEs->depth;
      smallestDistance =
          diff >= 0 && diff < smallestDistance ? diff : smallestDistance;
    }
    smallestSensitiveDistance[pEs] = smallestDistance;
  }
  std::sort(pendingStates.begin(), pendingStates.end(),
            [&](const ExecutionState *es1, const ExecutionState *es2) {
              if (smallestSensitiveDistance[es1] ==
                  smallestSensitiveDistance[es2])
                return es1->depth < es2->depth;
              return smallestSensitiveDistance[es1] >
                     smallestSensitiveDistance[es2];
            });
}

ExecutionState &ZESTIPendingSearcher::selectState() {
  if (!hasSelectedState)
    computeDistances();
  hasSelectedState = true;
  for (auto &es : toDelete)
    executor.terminateState(*es, StateTerminationType::SilentExit);
  toDelete.clear();
  return normalSearcher->selectState();
}

void ZESTIPendingSearcher::update(
    ExecutionState *current, const std::vector<ExecutionState *> &addedStates,
    const std::vector<ExecutionState *> &removedStates) {
  auto is_pending = [](const auto &es) {
    return !es->pendingConstraint.isNull();
  };
  std::vector<ExecutionState *> addedN, addedP, removedN, removedP;

  for (const auto &es : addedStates) {
    if (is_pending(es))
      addedP.push_back(es);
    else if (currentBaseDepth >= 0 &&
             (int)es->depth > currentBaseDepth + bound)
      toDelete.push_back(es);
    else
      addedN.push_back(es);
  }

  for (const auto &es : removedStates) {
    if (is_pending(es))
      removedP.push_back(es);
    else
      removedN.push_back(es);
  }

  if (current && is_pending(current)) {
    removedN.push_back(current);
    addedP.push_back(current);
  } else if (current && currentBaseDepth >= 0 &&
             (int)current->depth > currentBaseDepth + bound) {
    bool currentIsRemoved = false;
    for (const auto &es : removedStates)
      currentIsRemoved |= (es == current);
    if (!currentIsRemoved)
      toDelete.push_back(current);
    current = nullptr;
  }

  if (hasSelectedState)
    assert(addedP.empty() &&
           "ZESTI assumes pending mode is disabled when it is active");

  normalSearcher->update(current, addedN, removedN);
  normalStates.insert(normalStates.end(), addedN.begin(), addedN.end());
  pendingStates.insert(pendingStates.end(), addedP.begin(), addedP.end());

  for (const auto &es : removedN)
    normalStates.erase(
        std::remove(normalStates.begin(), normalStates.end(), es),
        normalStates.end());
  for (const auto &es : removedP)
    pendingStates.erase(
        std::remove(pendingStates.begin(), pendingStates.end(), es),
        pendingStates.end());
}

bool ZESTIPendingSearcher::empty() {
  if (!hasSelectedState)
    computeDistances();
  hasSelectedState = true;
  if (ZestiBound == 0)
    return true;
  while (normalStates.empty() && !pendingStates.empty()) {
    auto *es = pendingStates.back();
    pendingStates.pop_back();
    if (smallestSensitiveDistance[es] != 999999 &&
        executor.attemptToRevive(*es)) {
      currentBaseDepth = es->depth;
      bound = ZestiBound * smallestSensitiveDistance[es];
      bound = bound == 0 ? 1 : bound;
      update(nullptr, {es}, {});
    } else {
      es->pendingConstraint = nullptr;
      executor.terminateState(*es, StateTerminationType::SilentExit);
    }
  }
  return normalSearcher->empty();
}

void ZESTIPendingSearcher::printName(llvm::raw_ostream &os) {
  os << "<ZESTIPendingSearcher>\n";
}

///

#ifdef HAVE_PYTHON3

BranchingSearcher::BranchingSearcher(Searcher *_baseSearcher, Executor &_executor)
    : baseSearcher(_baseSearcher), executor(_executor) {}

ExecutionState &BranchingSearcher::selectState() {
  if (!lastState) {
    lastState = &baseSearcher->selectState();
    lastSelectStackSize = lastState->stack.size();
  }
  return *lastState;
}

void BranchingSearcher::update(ExecutionState *current,
                               const std::vector<ExecutionState *> &addedStates,
                               const std::vector<ExecutionState *> &removedStates) {
  if (std::find(removedStates.begin(), removedStates.end(), lastState) !=
      removedStates.end())
    lastState = nullptr;

  if (current && !addedStates.empty())
    lastState = nullptr;

  baseSearcher->update(current, addedStates, removedStates);
}

bool BranchingSearcher::empty() { return baseSearcher->empty(); }

void BranchingSearcher::printName(llvm::raw_ostream &os) {
  os << "<BranchingSearcher>, baseSearcher:\n";
  baseSearcher->printName(os);
  os << "</BranchingSearcher>\n";
}

void BranchingSearcher::addFeatures(ExecutionState &state) {
  baseSearcher->addFeatures(state);
}

///

GetFeaturesSearcher::GetFeaturesSearcher(Searcher *searcher, Executor &_executor)
    : baseSearcher(searcher), executor(_executor) {}

ExecutionState &GetFeaturesSearcher::selectState() {
  for (auto it = executor.featureStates.begin(); it != executor.featureStates.end(); ++it) {
    (*it)->predicted = false;
    executor.getStateFeatures(*it);
  }
  executor.featureStates.clear();
  ExecutionState &selected = baseSearcher->selectState();
  addFeatures(selected);

  subpath_ty subpath;
  executor.getSubpath(&selected, subpath, 0);
  executor.incSubpath(subpath, 0);
  executor.getSubpath(&selected, subpath, 1);
  executor.incSubpath(subpath, 1);
  executor.getSubpath(&selected, subpath, 2);
  executor.incSubpath(subpath, 2);
  executor.getSubpath(&selected, subpath, 3);
  executor.incSubpath(subpath, 3);

  llvm::BasicBlock *block = selected.pc->inst->getParent();
  if (ExecutionState::blockVisitTimes.find(block) == ExecutionState::blockVisitTimes.end())
    ExecutionState::blockVisitTimes[block] = 0;
  ExecutionState::blockVisitTimes[block]++;

  return selected;
}

void GetFeaturesSearcher::addFeatures(ExecutionState &es) {
  long index = featureIndex++;
  es.features.emplace_back(index, es.feature);
}

void GetFeaturesSearcher::update(ExecutionState *current,
                                 const std::vector<ExecutionState *> &addedStates,
                                 const std::vector<ExecutionState *> &removedStates) {
  baseSearcher->update(current, addedStates, removedStates);
}

bool GetFeaturesSearcher::empty() { return baseSearcher->empty(); }

void GetFeaturesSearcher::printName(llvm::raw_ostream &os) {
  os << "<GetFeaturesSearcher>, baseSearcher:\n";
  baseSearcher->printName(os);
  os << "</GetFeaturesSearcher>\n";
}

///

MLSearcher::MLSearcher(Executor &_executor, const std::string &modelPath)
    : executor(_executor) {
#ifdef KLEE_PYTHON_HOME
  // Set Python home so the embedded interpreter finds the correct site-packages
  // (critical for nix builds where packages are in a specific env prefix)
  {
    std::string home = KLEE_PYTHON_HOME;
    static std::wstring whome(home.begin(), home.end());
    Py_SetPythonHome(whome.c_str());
  }
#endif
  Py_Initialize();

  // Add learch parent directory to Python path
  PyObject *sys = PyImport_ImportModule("sys");
  if (!sys) {
    PyErr_Print();
    klee_error("Failed to import sys module");
  }
  PyObject *path = PyObject_GetAttrString(sys, "path");
  std::string modelDir = modelPath;
  auto pos = modelDir.rfind("/learch/");
  if (pos != std::string::npos)
    modelDir = modelDir.substr(0, pos);
  PyObject *modelDirStr = PyUnicode_FromString(modelDir.c_str());
  PyList_Insert(path, 0, modelDirStr);
  Py_DECREF(modelDirStr);
  Py_DECREF(path);
  Py_DECREF(sys);

  PyObject *pName = PyUnicode_FromString("learch.model");
  PyObject *pModule = PyImport_Import(pName);
  if (!pModule) {
    PyErr_Print();
    klee_error("Failed to import learch.model Python module");
  }

  PyObject *pInitFunc = PyObject_GetAttrString(pModule, "init_model");
  if (!pInitFunc) {
    PyErr_Print();
    klee_error("Failed to find init_model in learch.model");
  }
  PyObject *pArgs = PyTuple_New(2);
  PyTuple_SetItem(pArgs, 0, PyBytes_FromString("feedforward"));
  PyTuple_SetItem(pArgs, 1, PyBytes_FromString(modelPath.c_str()));
  PyObject *result = PyObject_CallObject(pInitFunc, pArgs);
  if (!result) {
    PyErr_Print();
    klee_error("learch.model.init_model() failed");
  }

  Py_DECREF(result);
  Py_DECREF(pModule);
  Py_DECREF(pName);
  Py_DECREF(pArgs);
  Py_DECREF(pInitFunc);
  // Release GIL so other threads can run; re-acquire in selectState()
  PyEval_SaveThread();
}

MLSearcher::~MLSearcher() {
  PyGILState_Ensure();
  Py_Finalize();
}

ExecutionState &MLSearcher::selectState() {
  PyGILState_STATE gstate = PyGILState_Ensure();
  int batch_size = 0;
  PyObject *features = PyList_New(0), *hiddens = PyList_New(0);
  for (auto state : states) {
    if (!state->predicted) {
      ++batch_size;
      PyObject *feature = PyList_New(0);
      // Skip first 2 features (query cost delta, cumulative cost)
      for (unsigned i = 2; i < state->feature.size(); i++) {
        PyObject *val = PyFloat_FromDouble(state->feature[i]);
        PyList_Append(feature, val);
        Py_DECREF(val);
      }
      PyList_Append(features, feature);
      Py_DECREF(feature);
    }
  }

  if (batch_size > 0) {
    PyObject *pArgs = PyTuple_New(2);
    PyTuple_SetItem(pArgs, 0, features);
    PyTuple_SetItem(pArgs, 1, hiddens);
    PyObject *pName = PyUnicode_FromString("learch.model");
    PyObject *pModule = PyImport_Import(pName);
    PyObject *pCallFunc = PyObject_GetAttrString(pModule, "predict");
    PyObject *res = PyObject_CallObject(pCallFunc, pArgs);
    if (!res) {
      PyErr_Print();
      klee_error("Learch predict() failed");
    }
    PyObject *rewards = PyTuple_GetItem(res, 0);

    int i = 0;
    for (auto state : states) {
      if (!state->predicted) {
        state->predicted = true;
        state->predicted_reward = PyFloat_AsDouble(PyList_GetItem(rewards, i));
        i++;
      }
    }

    Py_DECREF(res);
    Py_DECREF(pCallFunc);
    Py_DECREF(pModule);
    Py_DECREF(pName);
  } else {
    Py_DECREF(features);
    Py_DECREF(hiddens);
  }

  // Greedy selection: pick state with highest predicted reward
  ExecutionState *selection = nullptr;
  double current_max = -1e9;
  for (auto state : states) {
    if (!selection || current_max < state->predicted_reward) {
      selection = state;
      current_max = state->predicted_reward;
    }
  }
  assert(selection && "selectState called on empty MLSearcher");

  selection->predicted_reward = 0.0;
  selection->predicted = false;

  PyGILState_Release(gstate);
  return *selection;
}

void MLSearcher::update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) {
  states.insert(states.end(), addedStates.begin(), addedStates.end());
  std::set<ExecutionState *> removed;
  for (const auto *es : removedStates) {
    if (removed.count(const_cast<ExecutionState*>(es))) continue;
    auto it = std::find(states.begin(), states.end(), es);
    assert(it != states.end() && "invalid state removed");
    states.erase(it);
    removed.insert(const_cast<ExecutionState*>(es));
  }
}

bool MLSearcher::empty() { return states.empty(); }

void MLSearcher::printName(llvm::raw_ostream &os) {
  os << "MLSearcher (Learch feedforward)\n";
}

#endif // HAVE_PYTHON3

///

// ------------------------------------------------------------------------------------------------
// CGS: Concrete Constraint Guided Searcher (ICSE '24)

CGSSearcher::CGSSearcher(Executor &_executor):
  executor{_executor},
  TB{_executor.targetBranches},
  FCB{_executor.fullyCoveredBranches},
  PCB{_executor.partlyCoveredBranches},
  targetBranchNum{_executor.targetBranchNum},
  updateTargetBranch{_executor.updateTargetBranch},
  newFullyCoveredBranch{_executor.newFullyCoveredBranch},
  newPartlyCoveredBranch{_executor.newPartlyCoveredBranch},
  newBranchNumFromStore{_executor.newBranchNumFromStore},
  invalidStoreValues{_executor.invalidStoreValues},
  funcStores{_executor.funcStores} {}


ExecutionState &CGSSearcher::selectState() {
  ExecutionState *e;
  if (!branch_states.empty()) {
    e = branch_states.front();   // bfs
  }
  else {
    e = states.front();
  }

  return *e;
}


void CGSSearcher::update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) {

  if (current && (std::find(removedStates.begin(), removedStates.end(), current) == removedStates.end())) {

    // for bfs if no new state
    if (!addedStates.empty()) {

      // for bfs in states
      if (branch_states.empty()) {
        auto it = std::find(states.begin(), states.end(), current);
        states.erase(it);
        states.push_back(current);
      }

      // for bfs in branch states
      else {
        auto it = std::find(branch_states.begin(), branch_states.end(), current);
        branch_states.erase(it);
        branch_states.push_back(current);
      }
    }

    // reach target branch
    if (current->reachBranch) {

      // unsigned reachBID = executor.BI2ID[dyn_cast<BranchInst>(current->prevPC->inst)];
      // outs() << "state " << current->getID() << " reaches branch " << reachBID << "\n";

      // new fully covered branch
      if (newFullyCoveredBranch) {

        unsigned coveredBID = FCB.back();

        handleFullyCoveredBranch(current, coveredBID);

        // no target branches, move to states front for bfs
        if (current->branchInfos.empty()) {
          auto it = std::find(branch_states.begin(), branch_states.end(), current);
          if (it != branch_states.end()) {
            branch_states.erase(it);
            states.insert(states.begin(), current);
          }
        }

        newFullyCoveredBranch = false;
      }

      else {

        // [RARE] reach but not fully covered

        // remove the ID for this branch
        BranchInst *BI = dyn_cast<BranchInst>(current->prevPC->inst);
        unsigned pastBID = executor.BI2ID[BI];

        auto bInfos = &current->branchInfos;
        for (auto it = bInfos->begin(); it != bInfos->end(); it++) {
          ExecutionState::branchInfo *bInfo = *it;
          if (bInfo->targetBranchID == pastBID) {
            bInfos->erase(it);

            // outs() << "state " << current->getID() << " uses wrong value: "
            //         << current->storeValues[bInfo->reachStoreID]
            //         << " for brancn: " << bInfo->targetBranchID <<  "\n";
            break;
          }
        }

        // [TODO] there are corner cases such as:
        // 1) a[i] = k; if (a[j] == k){..} but we treat them as a def-use dependency
        // 2) a1->b = c; but a2->b != c

        // for debug
        // assert(0 && "this is not reasonable");

        // no target branches, move current to states
        if (bInfos->empty()) {
          auto it = std::find(branch_states.begin(), branch_states.end(), current);
          if (it != branch_states.end()) {
            branch_states.erase(it);
            states.push_back(current);
          }
        }
      }

      current->reachBranch = false;
    }

    // reach branch-dependent store instruction
    if (current->reachStore) {

      // for each <store, branch> pair, prioritize current state if it can fully cover
      // one of the branches using the store value
      auto bInfos = &current->branchInfos;
      auto istart = bInfos->begin() + (bInfos->size() - newBranchNumFromStore);
      for (auto it = istart; it != bInfos->end();) {
        ExecutionState::branchInfo *bInfo = *it;
        unsigned bid = bInfo->targetBranchID;
        unsigned sid = bInfo->reachStoreID;

        if (isNewStoreValue(current, bid, sid)) {
          // outs() << "state " << current->getID() << " has new target branch " << newBid << "\n";

          // move to branch states
          auto _it = std::find(states.begin(), states.end(), current);
          if (_it != states.end()) {
            states.erase(_it);
            branch_states.push_back(current);
          }

          it++;
        }

        else {

          // remove branch information for this branch
          bInfos->erase(it);
        }
      }

      newBranchNumFromStore = 0;
      current->reachStore = false;
    }

    // new partly covered branch
    if (!TB.empty() && newPartlyCoveredBranch) {
      unsigned targetBID = TB.back();

      handlePartlyCoveredBranch(current, targetBID);

      newPartlyCoveredBranch = false;
    }
  }   // end of if (current...)

  // update target branches when execute some instructions
  if (updateTargetBranch) {

    // clear all information for each state about target branch
    for (auto it = branch_states.begin(); it != branch_states.end();) {
      ExecutionState *state = *it;

      state->branchInfos.clear();

      if (!state->coveredNew) {
        branch_states.erase(it);
        states.push_back(state);
      }
      else {
        it++;
      }
    }

    // these old target branches can be added later if they are executed again
    TB.clear();

    // move new states to branch states based on new added target branches
    unsigned n = 0;
    while (n < targetBranchNum) {
      if (PCB.empty()) {
        break;
      }

      // move new target branches to TB in a dfs manner
      unsigned bid = PCB.back();
      PCB.pop_back();
      TB.push_back(bid);

      handlePartlyCoveredBranch(current, bid);
      n += 1;
    }

    updateTargetBranch = false;
  }

  // add new state to branch states if:
  // 1) it has target branch
  // 2) it will cover stores
  if (!current) {
    for (auto state: addedStates) {
      states.push_back(state);
    }
  }
  else {
    Function *F = current->pc->inst->getParent()->getParent();
    std::unordered_set<llvm::StoreInst *> func_stores = funcStores[F];

    for (auto state: addedStates) {
      if (!state->branchInfos.empty()) {

        // add state to branch_states if it has target branches
        branch_states.push_back(state);
      }
      else {

        // check if this branch contain a store instruction in funcStores
        bool find_bb = false;
        BasicBlock *bb = state->pc->inst->getParent();

        for (auto SI: func_stores) {
          unsigned sid = executor.SI2ID[SI];
          if (executor.storetTobranches[sid].empty()) {
            continue;
          }

          if (SI->getParent() == bb) {
            branch_states.push_back(state);
            find_bb = true;
            break;
          }
        }

        if (!find_bb) {
          states.push_back(state);
        }
      }
    }
  }

  // remove states
  for (auto state: removedStates) {
    auto it = std::find(branch_states.begin(), branch_states.end(), state);
    if (it != branch_states.end()) {
      branch_states.erase(it);
      continue;
    }

    auto _it = std::find(states.begin(), states.end(), state);
    if (_it != states.end()) {
      states.erase(_it);
    }
  }
}


bool CGSSearcher::empty() {
  return states.empty() && branch_states.empty();
}


void CGSSearcher::printName(llvm::raw_ostream &os) {
  klee_message("states: %ld. branch_states: %ld", states.size(), branch_states.size());
  //  os << "CGSSearcher\n";
}


void CGSSearcher::handlePartlyCoveredBranch(ExecutionState *current, unsigned targetBID) {

  // have covered stores
  // 1) from store instructions
  std::unordered_set<unsigned> newStoreIDs = executor._BDDep[targetBID]->stores;

  for (auto sid: newStoreIDs) {

    // add new branch var using current state that adds new branch
    if (current->storeValues.find(sid) != current->storeValues.end()) {
      isNewStoreValue(current, targetBID, sid);
    }

    // add branch info for current, continue execution
    ExecutionState::branchInfo *bInfo = new ExecutionState::branchInfo();
    bInfo->reachStoreID = sid;
    bInfo->targetBranchID = targetBID;
    current->branchInfos.push_back(bInfo);
  }

  // push current to branch states
  auto it = std::find(states.begin(), states.end(), current);
  if (it != states.end()) {
    states.erase(it);
    branch_states.push_back(current);
  }

  // find all states that have new store values
  for (auto sid: newStoreIDs) {
    for (auto it = states.begin(); it != states.end(); ) {
      ExecutionState *state = *it;
      if (state == current) {
        it++;
        continue;
      }

      // check if state can fully cover this store
      if ((state->storeValues.find(sid) != state->storeValues.end()) && \
          isNewStoreValue(state, targetBID, sid)) {

        state->reachBranch = false;
        state->reachStore = false;  // avoid to be considered as an old value..

        ExecutionState::branchInfo *bInfo = new ExecutionState::branchInfo();
        bInfo->reachStoreID = sid;
        bInfo->targetBranchID = targetBID;

        state->branchInfos.push_back(bInfo);

        // outs() << "state " << state->getID() << " has new target branch " << targetBID << "\n";

        states.erase(it);
        branch_states.push_back(state);
      }
      else {
        it++;
      }
    }
  }
}


void CGSSearcher::handleFullyCoveredBranch(ExecutionState *current, unsigned coveredBID) {

  // current cover targetBID (move out the branch states that have the same targetBID)
  std::unordered_set<unsigned> remove_sv;

  // remove coveredBID from storetTobranches, to determine whether to remove state storeValues
  Instruction *I = executor.ID2BI[coveredBID];
  Function *F = I->getParent()->getParent();
  std::unordered_set<llvm::StoreInst *> &func_stores = funcStores[F];

  for (auto sid: executor._BDDep[coveredBID]->stores) {
    executor.storetTobranches[sid].erase(coveredBID);

    // if the store related branches are all fully covered
    if (executor.storetTobranches[sid].empty()) {
      remove_sv.insert(sid);

      // erase these store blocks in function
      StoreInst *SI = executor.ID2SI[sid];
      auto it = func_stores.find(SI);
      if (it != func_stores.end()) {
        func_stores.erase(it);

        // outs() << "[searcher] function " << F->getName() << " removes store " << *SI << "\n";
      }
    }
  }

  // remove useless branch information
  for (auto it = branch_states.begin(); it != branch_states.end();) {
    ExecutionState *state = *it;

    // remove state store values, decrease memory budget
    for (auto sid: remove_sv) {
      state->storeValues.erase(sid);

      // outs() << "state " << state->getID() << " removes "
      //         << *executor.ID2SI[sid] << "\n";
    }

    auto branchInfos = &state->branchInfos;
    for (auto b_it = branchInfos->begin(); b_it != branchInfos->end(); b_it++) {
      ExecutionState::branchInfo *bInfo = *b_it;
      if (bInfo->targetBranchID == coveredBID) {
        branchInfos->erase(b_it);

        break;  // only one
      }
    }

    if ((state != current) && branchInfos->empty()) {
        branch_states.erase(it);
        states.push_back(state);
    }
    else {
       it++;
    }
  }
}

bool CGSSearcher::isNewStoreValue(ExecutionState *state, unsigned bid, unsigned sid) {

  bool result = false;
  std::string cause;

  signed value = state->storeValues[sid];

  // new branch (only for state that first cover this branch)
  if (invalidStoreValues.find(bid) == invalidStoreValues.end()) {

    // this is determined invalid value
    invalidStoreValues[bid].insert(value);

    result = true;
    cause = "new branch";
  }

  else {

    // determine whether this value can cover new branch
    bool find_new = false;

    if (invalidStoreValues[bid].find(value) != invalidStoreValues[bid].end()) {
      find_new = false;
    }

    else if (validStoreValues[bid].find(value) != validStoreValues[bid].end()) {
      find_new = true;
    }

    else {
      auto bdDep = executor._BDDep[bid];

      // switch
      if (bdDep->type) {
        std::unordered_set<signed> uCVs = bdDep->unCoveredValues;
        auto it = uCVs.find(value);
        if (it != uCVs.end()) {
          find_new = true;
        }
      }

      // br
      else {

        // 1. update value
        if (bdDep->arith_op != "") {
          if (bdDep->arith_op == "and") {
            value &= bdDep->arith_var;
          }
          if (bdDep->arith_op == "or") {
            value |= bdDep->arith_var;
          }
        }

         // 2. compare value with constant
        signed constant = bdDep->constant;
        unsigned pred = bdDep->unCoveredPred;
        switch(pred) {
          case llvm::CmpInst::ICMP_EQ: {
            find_new = (value == constant);
            break;
          }
          case llvm::CmpInst::ICMP_NE: {
            find_new = (value != constant);
            break;
          }
          case llvm::CmpInst::ICMP_SGT:
          case llvm::CmpInst::ICMP_UGT: {
            find_new = (value > constant);
            break;
          }
          case llvm::CmpInst::ICMP_SLT:
          case llvm::CmpInst::ICMP_ULT: {
            find_new = (value < constant);
            break;
          }
          case llvm::CmpInst::ICMP_SGE:
          case llvm::CmpInst::ICMP_UGE: {
            find_new = (value >= constant);
            break;
          }
          case llvm::CmpInst::ICMP_SLE:
          case llvm::CmpInst::ICMP_ULE: {
            find_new = (value <= constant);
            break;
          }

          default: {
            outs() << "invalid predicate: " << pred << "\n";
            break;
          }
        }
      }
    }

    if (find_new) {

      // update validStoreValues
      validStoreValues[bid].insert(value);

      result = true;
      cause = "new storeValue";
    }

    else {
      invalidStoreValues[bid].insert(value);
    }
  }

  if (result) {
    // outs() << "[searcher]" << cause << ": state " << state->getID() << " finds definition "
    //         << value << " in branch " << bid << "\n";
  }

  return result;
}
