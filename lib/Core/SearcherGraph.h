//===-- SearcherGraph.h -----------------------------------------*- C++ -*-===//
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

#ifndef EMPC_SEARCHERGRAPH_H_
#define EMPC_SEARCHERGRAPH_H_

#include "SearcherDefs.h"
#include "SearcherGraphAlgorithm.hpp"

#include <cstdint>
#include <fstream>
#include <memory>

#include <deque>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#if defined(LLVM34)
#include "llvm/DebugInfo.h"
#else
#include "llvm/IR/DebugInfo.h"
#endif

#if defined(LLVM34) || defined(LLVM35) || defined(LLVM36)
#define LLVM_OLD_DEBUG_API
#endif

namespace klee {
namespace Empc {
class BasicBlockNode;
class BasicBlockNodePtr;
class IntraProcGraph;
class InterProcGraph;
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
namespace Utility {
template <typename _Tp>
bool find(const std::deque<_Tp> &_array, const _Tp &_value) {
  return std::find(_array.cbegin(), _array.cend(), _value) != _array.cend();
}

template <typename _Tp>
bool find(const std::list<_Tp> &_array, const _Tp &_value) {
  return std::find(_array.cbegin(), _array.cend(), _value) != _array.cend();
}

template <typename _Tp>
std::string getGraphInDotFormat(
    const std::unordered_map<_Tp, std::unordered_set<_Tp>> &graph,
    const std::string &name);

bool isBlacklistedFunction(const llvm::Function *func);

std::string getFunctionName(const llvm::Function *func);

std::size_t getBasicBlockInnerIndex(const llvm::BasicBlock *bblock);

std::string getBasicBlockName(const llvm::BasicBlock *bblock,
                              bool withFuncName = false);

void getInstructionLocation(const llvm::Instruction *instr,
                            std::string &fileName, unsigned &line,
                            unsigned &column);
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
typedef uint64_t NodeID;

template <typename _Tp> class Node {
public:
  NodeID id;
  _Tp realValue;

  bool isNull;

private:
  static NodeID countNodeID;

  static NodeID getNextNodeID();

public:
  Node() : id(getNextNodeID()), isNull(true) {}
  Node(const _Tp &_value)
      : id(getNextNodeID()), realValue(_value), isNull(false) {}
  ~Node() = default;
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
class BasicBlockNode : public Node<const llvm::BasicBlock *> {
  friend class BasicBlockNodePtr;

public:
  enum class Type {
    ORIGINAL,
    LOOP_HEADER_REPL,
    LOOP_LATCH_REPL,
    LOOP_EXIT_REPL,
  } type;

public:
  std::string name;
  std::size_t inComboID;
  std::size_t callComboID;
  const llvm::BasicBlock *adjunctBlock;
  std::deque<BasicBlockNodePtr> successors;
  std::unordered_set<const llvm::Function *> calleeSet;
  std::deque<const llvm::Function *> calleeSeq;

private:
  BasicBlockNode(const llvm::BasicBlock *_bb)
      : Node<const llvm::BasicBlock *>(_bb), type(Type::ORIGINAL), inComboID(0),
        callComboID(0), adjunctBlock(nullptr) {}
  BasicBlockNode(Type _type, const llvm::BasicBlock *_bb)
      : Node<const llvm::BasicBlock *>(), type(_type), inComboID(0),
        callComboID(0), adjunctBlock(_bb) {}

  BasicBlockNode() = delete;
  BasicBlockNode(const BasicBlockNode &) = delete;
  BasicBlockNode &operator=(const BasicBlockNode &) = delete;

  void setName();
};

class BasicBlockNodePtr {
  friend std::ostream &operator<<(std::ostream &outStream,
                                  const BasicBlockNodePtr &nodePtr);
  friend llvm::raw_fd_ostream &operator<<(llvm::raw_fd_ostream &outStream,
                                          const BasicBlockNodePtr &nodePtr);

private:
  std::shared_ptr<BasicBlockNode> pointer;

public:
  BasicBlockNodePtr() : pointer(nullptr) {}
  BasicBlockNodePtr(const llvm::BasicBlock *_bb)
      : pointer(std::shared_ptr<BasicBlockNode>(new BasicBlockNode(_bb))) {
    pointer->setName();
  }
  BasicBlockNodePtr(BasicBlockNode::Type _type, const llvm::BasicBlock *_bb)
      : pointer(
            std::shared_ptr<BasicBlockNode>(new BasicBlockNode(_type, _bb))) {
    pointer->setName();
  }
  BasicBlockNodePtr(const BasicBlockNodePtr &_other)
      : pointer(_other.pointer) {}
  BasicBlockNodePtr &operator=(const BasicBlockNodePtr &_other);
  ~BasicBlockNodePtr() = default;

  BasicBlockNode *operator->() const { return pointer.get(); }

  bool operator==(const BasicBlockNodePtr &other) const {
    return pointer == other.pointer;
  }
  bool operator!=(const BasicBlockNodePtr &other) const {
    return pointer != other.pointer;
  }

  // Explicit boolean conversion operator
  explicit operator bool() const { return pointer != nullptr; }

  inline std::size_t hash() const {
    return std::hash<NodeID>{}(pointer ? pointer->id : 0);
  };
};

std::ostream &operator<<(std::ostream &outStream,
                         const BasicBlockNodePtr &nodePtr);
llvm::raw_fd_ostream &operator<<(llvm::raw_fd_ostream &outStream,
                                 const BasicBlockNodePtr &nodePtr);
} // namespace Empc
} // namespace klee

namespace std {
template <> struct hash<klee::Empc::BasicBlockNodePtr> {
  size_t operator()(const klee::Empc::BasicBlockNodePtr &obj) const {
    return obj.hash();
  }
};
} // namespace std

/// @brief Declaration of `IntraProcGraph`
namespace klee {
namespace Empc {
class IntraProcGraph {
public:
  typedef std::size_t CID;

private:
  typedef std::unordered_map<BasicBlockNodePtr,
                             std::unordered_set<BasicBlockNodePtr>>
      RawGraph;

  struct ComboGraph {
    CID id;
    bool isRawLoop;

    RawGraph rawGraph;

    std::shared_ptr<DirectedAcyclicGraph<BasicBlockNodePtr>> dag;

    std::unordered_set<CID> callingCombos;

    BasicBlockNodePtr header;
    std::unordered_set<BasicBlockNodePtr> headerExitNodes;

    std::unordered_set<BasicBlockNodePtr> latches;
    std::unordered_set<BasicBlockNodePtr> exitNodes;
    std::unordered_map<BasicBlockNodePtr, std::unordered_set<BasicBlockNodePtr>>
        exitEdges;

    std::unordered_set<BasicBlockNodePtr> comboExitNodes;
    std::unordered_map<BasicBlockNodePtr, std::unordered_set<BasicBlockNodePtr>>
        comboExitEdges;

    ComboGraph() : id(0), isRawLoop(false) {}
    ComboGraph(std::size_t _id, bool _isLoop) : id(_id), isRawLoop(_isLoop) {}
  };

public:
  struct PathReservedInfo {
    std::stack<std::pair<CID, BasicBlockNodePtr>> graphCallStack;
    std::stack<std::pair<
        DirectedAcyclicGraph<BasicBlockNodePtr>::PathReservedInfo, std::size_t>>
        dagGraphStack;
    BasicBlockNodePtr currNode;
  };

private:
  enum class StackChangeType {
    INIT,
    NORMAL,
    ENTER_LOOP,
    LOOP_LOOP,
    EXIT_LOOP,
    EXIT,
    UNKNOWN,
  };

  struct TempReservedStackInfo {
    StackChangeType type;
    bool success;
    CID targetComboGraphID;
    CID prevComboGraphID;

    BasicBlockNodePtr compExitingNode;
    BasicBlockNodePtr compCallerNode;
    BasicBlockNodePtr compLatchNode;

    std::pair<CID, BasicBlockNodePtr> changedFrameGraph;
    std::pair<DirectedAcyclicGraph<BasicBlockNodePtr>::PathReservedInfo,
              std::size_t>
        changedFrameDAG;

    TempReservedStackInfo()
        : type(StackChangeType::UNKNOWN), success(true), targetComboGraphID(0),
          prevComboGraphID(0) {}
    TempReservedStackInfo(StackChangeType _type)
        : type(_type), success(true), targetComboGraphID(0),
          prevComboGraphID(0) {}
  };

  std::unordered_map<BasicBlockNodePtr, TempReservedStackInfo>
      tempReservedStackInfo;

private:
  std::size_t id;

  const llvm::Function *func;

  BasicBlockNodePtr entry;

  RawGraph rawGraph;

  std::deque<ComboGraph> comboGraphs;

  bool hasWeirdCycles;

  bool hasUnconnectedComponents;

  bool hasInvalidEntry;

  std::unordered_map<const llvm::BasicBlock *, BasicBlockNodePtr> bbNodeMap;

  BasicBlockNodePtr getOrCreateBBNodePtr(const llvm::BasicBlock *bblock);

  bool queryStepNode(BasicBlockNodePtr currNode, BasicBlockNodePtr nextNode,
                     const PathReservedInfo &reservedInfo);

  void ensureStepNode(BasicBlockNodePtr nextVertex,
                      PathReservedInfo &reservedInfo, bool forceNext = false);

public:
  IntraProcGraph() = delete;
  IntraProcGraph(std::size_t _id, llvm::Function *_func,
                 llvm::LoopInfo *_loopInfo);
  IntraProcGraph(const IntraProcGraph &) = delete;
  IntraProcGraph &operator=(const IntraProcGraph &) = delete;
  ~IntraProcGraph() = default;

  bool empty() const {
    return !entry || bbNodeMap.empty() || comboGraphs.empty() ||
           hasInvalidEntry || hasUnconnectedComponents || hasWeirdCycles;
  }

  bool realEmpty() const {
    return !entry || bbNodeMap.empty() || comboGraphs.empty();
  }

  BasicBlockNodePtr getBBNodePtr(const llvm::BasicBlock *bblock) const;

  bool queryStepBasicBlock(const llvm::BasicBlock *currBB,
                           const llvm::BasicBlock *nextBB,
                           const PathReservedInfo &reservedInfo);

  void ensureStepBasicBlock(const llvm::BasicBlock *nextBB,
                            PathReservedInfo &reservedInfo, bool isExit = false,
                            bool forceNext = false);
};

} // namespace Empc
} // namespace klee

/// @brief Declaration of `InterProcGraph`
namespace klee {
namespace Empc {
class InterProcGraph {
public:
  struct PathReservedInfo {
    std::stack<std::pair<const llvm::Function *, const llvm::BasicBlock *>>
        graphCallStack;
    std::stack<IntraProcGraph::PathReservedInfo> ipgCallStack;
    const llvm::Function *currFunc;
    const llvm::BasicBlock *currBasicBlock;

    PathReservedInfo() : currFunc(nullptr), currBasicBlock(nullptr) {}
  };

private:
  enum class StackChangeType {
    NOTHING,
    COMMON,
    PUSH_CALL,
    POP_CALL,
    EXIT,
    UNKNOWN,
  };

  struct TempReservedStackInfo {
    StackChangeType type;
    bool success;
    bool callerNotMatch;

    const llvm::Function *exitingFunction;
    const llvm::BasicBlock *exitingBasicBlock;

    std::pair<const llvm::Function *, const llvm::BasicBlock *>
        changedFrameGraph;
    IntraProcGraph::PathReservedInfo changedFrameIPG;

    TempReservedStackInfo()
        : type(StackChangeType::UNKNOWN), success(true), callerNotMatch(false),
          exitingFunction(nullptr), exitingBasicBlock(nullptr) {}
    TempReservedStackInfo(StackChangeType _type)
        : type(_type), success(true), callerNotMatch(false),
          exitingFunction(nullptr), exitingBasicBlock(nullptr) {}
  };

  std::unordered_map<const llvm::BasicBlock *, TempReservedStackInfo>
      tempReservedStackInfo;

private:
  std::unordered_map<const llvm::Function *, std::shared_ptr<IntraProcGraph>>
      graph;

  const llvm::Function *entry;

public:
  InterProcGraph() = delete;
  InterProcGraph(const InterProcGraph &) = delete;
  InterProcGraph &operator=(const InterProcGraph &) = delete;
  InterProcGraph(llvm::Module *_module, const llvm::Function *_entry,
                 const std::unordered_map<std::string, bool> &definedFunctions);
  ~InterProcGraph() = default;

  bool empty(const llvm::Function *_func) const {
    return graph.find(_func) == graph.cend() ? true : graph.at(_func)->empty();
  }

  bool realEmpty(const llvm::Function *_func) const {
    return graph.find(_func) == graph.cend() ? true
                                             : graph.at(_func)->realEmpty();
  }

  bool queryStepBasicBlock(const llvm::BasicBlock *currBasicBlock,
                           const llvm::BasicBlock *nextBasicBlock,
                           const PathReservedInfo &reservedInfo,
                           StateStepType stepType);

  void ensureStepBasicBlock(const llvm::BasicBlock *nextBasicBlock,
                            PathReservedInfo &reservedInfo,
                            bool forceNext = false);
};
} // namespace Empc
} // namespace klee

// namespace klee
// {
//     namespace Empc
//     {
//         // DELETE
//         struct TempState
//         {
//             uint64_t id;
//         };

//         class ExecutionState
//         {
//             friend class ExecutionStatePtr;

//         public:
//             typedef TempState RawExecutionState;

//         private:
//             RawExecutionState *state;

//             uint64_t id;

//         private:
//             ExecutionState(RawExecutionState *_state) : state(_state),
//             id(_state->id + 1) {}

//             ExecutionState(const ExecutionState &) = delete;
//             ExecutionState &operator=(const ExecutionState &) = delete;
//         };

//         class ExecutionStatePtr
//         {
//         private:
//             std::shared_ptr<ExecutionState> pointer;

//         public:
//             ExecutionStatePtr() : pointer(nullptr) {}
//             ~ExecutionStatePtr() = default;

//             ExecutionState *operator->() const { return pointer.get(); }

//             bool operator==(const ExecutionStatePtr &other) const { return
//             pointer == other.pointer; } bool operator!=(const
//             ExecutionStatePtr &other) const { return pointer !=
//             other.pointer; }

//             inline std::size_t hash() const { return
//             std::hash<NodeID>{}(pointer ? pointer->id : 0); };
//         };
//     }
// }

#endif