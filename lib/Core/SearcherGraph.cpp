//===-- SearcherGraph.cpp ---------------------------------------*- C++ -*-===//
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

#include "SearcherGraph.h"
#include "SearcherDefs.h"
#include "SearcherLog.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/Threading.h"

#if defined(LLVM34)
#include "llvm/DebugInfo.h"
#else
#include "llvm/IR/DebugInfo.h"
#endif

#if defined(LLVM34) || defined(LLVM35) || defined(LLVM36)
#define LLVM_OLD_DEBUG_API
#endif

namespace klee {
llvm::cl::OptionCategory EmpcSearcherGraphCat(
    "Searcher graph options",
    "These options control the Empc searcher graphs configuration.");

llvm::cl::opt<unsigned> EmpcSearcherGraphMaxMatching(
    "empc-searcher-max-matching", llvm::cl::init(1),
    llvm::cl::desc(
        "The maximum count of maximum matchings (x10000) (default=1)"),
    llvm::cl::cat(EmpcSearcherGraphCat));

llvm::cl::opt<unsigned> EmpcSearcherGraphThreadCount(
    "empc-searcher-graph-thread-count", llvm::cl::init(0),
    llvm::cl::desc("The thread count for thread pool used in analyzing "
                   "searcher graphs (default=<cpu number>/2)"),
    llvm::cl::cat(EmpcSearcherGraphCat));

llvm::cl::opt<bool> EmpcSearcherGraphShowAnalyzingProgress(
    "empc-searcher-graph-show-progress", llvm::cl::init(true),
    llvm::cl::desc(
        "Show the progress of analyzing control-flow graphs (default=true)"),
    llvm::cl::cat(EmpcSearcherGraphCat));
} // namespace klee

namespace klee {
namespace Empc {
namespace Utility {
static const unsigned MaxMatchingNumScale = 10000;

static const std::vector<std::string> BlackListFunctions = {
    "asan.", "llvm.",  "sancov.", "__ubsan_handle_",
    "free",  "malloc", "calloc",  "realloc"};

bool isBlacklistedFunction(const llvm::Function *func) {
  for (auto const &blFunc : BlackListFunctions)
    if (func->getName().startswith(blFunc))
      return true;

  return false;
}

std::string getFunctionName(const llvm::Function *func) {
  return func ? func->getName().str() : std::string("null");
}

void getInstructionLocation(const llvm::Instruction *instr,
                            std::string &fileName, unsigned &line,
                            unsigned &column) {
  fileName = "";
  line = 0;
  column = 0;
#ifdef LLVM_OLD_DEBUG_API
  DebugLoc Loc = instr->getDebugLoc();
  if (!Loc.isUnknown()) {
    DILocation cDILoc(Loc.getAsMDNode(M.getContext()));
    DILocation oDILoc = cDILoc.getOrigLocation();

    line = oDILoc.getLineNumber();
    fileName = oDILoc.getFilename().str();

    if (fileName.empty()) {
      line = cDILoc.getLineNumber();
      fileName = cDILoc.getFilename().str();
    }
  }
#else
  if (llvm::DILocation *Loc = instr->getDebugLoc()) {
    line = Loc->getLine();
    column = Loc->getColumn();
    fileName = Loc->getFilename().str();

    if (fileName.empty()) {
      llvm::DILocation *oDILoc = Loc->getInlinedAt();
      if (oDILoc) {
        line = oDILoc->getLine();
        column = oDILoc->getColumn();
        fileName = oDILoc->getFilename().str();
      }
    }
  }
#endif
}

std::size_t getBasicBlockInnerIndex(const llvm::BasicBlock *bblock) {
  if (!bblock)
    return 0;
  const llvm::Function *func = bblock->getParent();
  std::size_t bbId = 0;
  for (const auto &BB : *func) {
    if (&BB == bblock)
      return bbId;
    ++bbId;
  }
  assert(false && "Unexpected Error: failed to find the parameter basic block "
                  "in its parent function");
}

std::string getBasicBlockName(const llvm::BasicBlock *bblock,
                              bool withFuncName) {
  if (!bblock)
    return "null";

  std::string fileName;
  unsigned line = 0, column = 0;
  std::size_t bbIndex = getBasicBlockInnerIndex(bblock);

  for (auto &instr : *bblock) {
    getInstructionLocation(&instr, fileName, line, column);
    if (fileName.empty() || line == 0)
      continue;
    else {
      std::string result = fileName + ":" + std::to_string(line) + ":" +
                           std::to_string(column) + ":" +
                           std::to_string(bbIndex);
      if (withFuncName)
        result = getFunctionName(bblock->getParent()) + "():" + result;
      return result;
    }
  }

  return getFunctionName(bblock->getParent()) +
         "():" + std::to_string(getBasicBlockInnerIndex(bblock));
}

// template <typename _Tp>
// bool find(const std::deque<_Tp> &_array, const _Tp &_value) {
//   return std::find(_array.cbegin(), _array.cend(), _value) != _array.cend();
// }

// template <typename _Tp>
// bool find(const std::list<_Tp, std::allocator<_Tp>> &_array,
//           const _Tp &_value) {
//   return std::find(_array.cbegin(), _array.cend(), _value) != _array.cend();
// }

// template <typename _Tp> bool erase(std::list<_Tp> &_array, const _Tp &_value)
// {
//   _array.remove(_value);
// }

template <typename _Tp>
std::string getGraphInDotFormat(
    const std::unordered_map<_Tp, std::unordered_set<_Tp>> &graph,
    const std::string &name) {
  auto formatHex = [](unsigned _num) -> std::string {
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(6) << std::hex << _num;
    return stream.str();
  };
  auto valueString = [](const _Tp &_value) -> std::string {
    std::stringstream stream;
    stream << _value;
    return stream.str();
  };

  std::string result = "";
  unsigned nodeIndex = 0x1a6d00;
  std::unordered_map<_Tp, std::string> nodeIndexMap;
  std::unordered_map<_Tp, std::string> nodeNameMap;
  for (const auto &graphPair : graph) {
    std::unordered_set<_Tp> tempNodeSet = graphPair.second;
    tempNodeSet.emplace(graphPair.first);
    for (const auto &tempNode : tempNodeSet) {
      nodeIndexMap[tempNode] = "Node0x" + formatHex(nodeIndex);
      nodeIndex += 0x10;
      nodeNameMap[tempNode] = valueString(tempNode);
    }
  }

  result += "digraph \"" + name + "\" {\n";
  result += "\tlabel=\"" + name + "\";\n\n";

  for (const auto &graphPair : graph) {
    result += '\t' + nodeIndexMap[graphPair.first] +
              " [shape=record,label=\"{" + nodeNameMap[graphPair.first] +
              "}\"];\n";
    for (const auto &succ : graphPair.second) {
      result += '\t' + nodeIndexMap[graphPair.first] + " -> " +
                nodeIndexMap[succ] + ";\n";
    }
  }

  result += "}\n";

  return result;
}
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
template <typename _Tp> NodeID Node<_Tp>::countNodeID = 0x1829acefdbfc0411;

template <typename _Tp> NodeID Node<_Tp>::getNextNodeID() {
  return countNodeID++;
}

// template class Node<const llvm::BasicBlock *>;

BasicBlockNodePtr &
BasicBlockNodePtr::operator=(const BasicBlockNodePtr &_other) {
  if (this != &_other)
    pointer = _other.pointer;
  return *this;
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
static std::unique_ptr<llvm::ThreadPool> StaticThreadPool;

void BasicBlockNode::setName() {
  switch (type) {
  case Type::ORIGINAL:
    name = Utility::getBasicBlockName(realValue);
    break;

  case Type::LOOP_HEADER_REPL:
    name = std::string("(Loop Header) ") +
           Utility::getBasicBlockName(adjunctBlock);
    break;

  case Type::LOOP_LATCH_REPL:
    name =
        std::string("(Loop Latch) ") + Utility::getBasicBlockName(adjunctBlock);
    break;

  case Type::LOOP_EXIT_REPL:
    name =
        std::string("(Loop Exit) ") + Utility::getBasicBlockName(adjunctBlock);
    break;

  default:
    name = "UNKNOWN";
    break;
  }
}

std::ostream &operator<<(std::ostream &outStream,
                         const BasicBlockNodePtr &nodePtr) {
  if (nodePtr)
    outStream << nodePtr->name;
  else
    outStream << "null";
  return outStream;
}

llvm::raw_fd_ostream &operator<<(llvm::raw_fd_ostream &outStream,
                                 const BasicBlockNodePtr &nodePtr) {
  if (nodePtr)
    outStream << nodePtr->name;
  else
    outStream << "null";
  return outStream;
}

BasicBlockNodePtr
IntraProcGraph::getOrCreateBBNodePtr(const llvm::BasicBlock *bblock) {
  if (bbNodeMap.find(bblock) == bbNodeMap.end())
    bbNodeMap[bblock] = BasicBlockNodePtr(bblock);

  return bbNodeMap.at(bblock);
}

BasicBlockNodePtr
IntraProcGraph::getBBNodePtr(const llvm::BasicBlock *bblock) const {
  assert(bbNodeMap.find(bblock) != bbNodeMap.end());

  return bbNodeMap.at(bblock);
}

IntraProcGraph::IntraProcGraph(std::size_t _id, llvm::Function *_func,
                               llvm::LoopInfo *_loopInfo)
    : id(_id), func(_func), hasWeirdCycles(false),
      hasUnconnectedComponents(false), hasInvalidEntry(false) {
  assert(StaticThreadPool);

  if (_func->empty() || _func->isDeclaration() || _func->isIntrinsic())
    return;

  entry = getOrCreateBBNodePtr(&_func->getEntryBlock());

  // Construct the main graph
  for (llvm::Function::const_iterator BB = _func->begin(); BB != _func->end();
       ++BB) {
    auto bblock = &(*BB);

    // Create basic block pointer
    auto bbNodePtr = getOrCreateBBNodePtr(bblock);
    rawGraph[bbNodePtr];

    // Basic block's successors
    for (llvm::const_succ_iterator SI = llvm::succ_begin(bblock),
                                   SE = llvm::succ_end(bblock);
         SI != SE; ++SI) {
      auto successor = *SI;
      auto succNodePtr = getOrCreateBBNodePtr(successor);

      bbNodePtr->successors.emplace_back(succNodePtr);
      rawGraph[bbNodePtr].emplace(succNodePtr);
      rawGraph[succNodePtr];
    }

    // Basic block's callsites
    for (auto &I : *BB) {
      // Get the function called by this BB
      if (const llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        if (const llvm::Function *calledFunction = CI->getCalledFunction()) {
          if (!Utility::isBlacklistedFunction(calledFunction)) {
            bbNodePtr->calleeSet.emplace(calledFunction);
            bbNodePtr->calleeSeq.push_back(calledFunction);
          }
        }
      }
    }
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::GRAPH)) {
    std::string title =
        "Raw Graph of Function '" + Utility::getFunctionName(func) + "'";
    Logging::start(Logging::Type::GRAPH, title);
    Logging::log() << Utility::getGraphInDotFormat(rawGraph, title) << '\n';
    Logging::stop();
  }

  //
  // Split loops into single loops without subloops

  std::queue<llvm::Loop *> subLoopQueue;
  subLoopQueue.push(nullptr);
  std::unordered_set<BasicBlockNodePtr> tempCallerNodes;
  std::vector<std::unordered_set<BasicBlockNodePtr>> tempComboCallGraph;

  while (!subLoopQueue.empty()) {
    llvm::Loop *currLoop = subLoopQueue.front();
    subLoopQueue.pop();

    comboGraphs.emplace_back(comboGraphs.size(), true);
    ComboGraph &comboGraph = comboGraphs.back();

    tempComboCallGraph.emplace_back();

    std::vector<llvm::Loop *> subLoops;

    // Initialize bbCombo and get a subgraph
    if (!currLoop) {
      // Function basic graph (id = 0, isRawLoop = false)

      comboGraph.isRawLoop = false;

      // Construct a subgraph for this combo
      comboGraph.rawGraph = rawGraph;

      // Get subloops
      for (llvm::Loop *loop : *_loopInfo)
        subLoops.push_back(loop);

      comboGraph.header = entry;
    } else {
      // Raw loop

      comboGraph.isRawLoop = true;

      // Construct a subgraph for this combo
      {
        const auto &blockSet = currLoop->getBlocksSet();
        for (const auto &block : blockSet) {
          BasicBlockNodePtr nodePtr = getBBNodePtr(block);
          std::unordered_set<BasicBlockNodePtr> successors;
          for (const auto &succ : rawGraph.at(nodePtr)) {
            if (blockSet.find(succ->realValue) != blockSet.end())
              successors.emplace(succ);
          }
          comboGraph.rawGraph[nodePtr] = successors;
        }
      }

      // Get subloops
      subLoops = currLoop->getSubLoops();

      // Initialize some loop properties of bbCombo
      {
        comboGraph.header = getBBNodePtr(currLoop->getHeader());

        //
        // graph[opBBCombo.header].callComboId = opBBCombo.id;

        llvm::SmallVector<llvm::BasicBlock *, 9> latches;
        currLoop->getLoopLatches(latches);
        for (const auto &latch : latches) {
          auto latchNodePtr = getBBNodePtr(latch);
          comboGraph.latches.emplace(latchNodePtr);
          comboGraph.rawGraph[latchNodePtr].erase(comboGraph.header);
        }

        llvm::SmallVector<llvm::BasicBlock *, 9> exitBlocks;
        currLoop->getExitBlocks(exitBlocks);
        for (const auto &exitBlock : exitBlocks) {
          comboGraph.exitNodes.emplace(getBBNodePtr(exitBlock));
        }

        llvm::SmallVector<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, 9>
            exitEdges;
        currLoop->getExitEdges(exitEdges);
        for (const auto &edge : exitEdges) {
          auto edgeFirstNodePtr = getBBNodePtr(edge.first);
          auto edgeSecondNodePtr = getBBNodePtr(edge.second);

          if (edgeFirstNodePtr == comboGraph.header)
            comboGraph.headerExitNodes.emplace(edgeSecondNodePtr);
          else
            comboGraph.exitEdges[edgeFirstNodePtr].emplace(edgeSecondNodePtr);

          comboGraph.rawGraph[edgeFirstNodePtr].erase(edgeSecondNodePtr);
        }
      }
    }

    std::unordered_set<BasicBlockNodePtr> subLoopReplaceNodes;

    // Erase nodes of subloops
    for (llvm::Loop *loop : subLoops) {
      llvm::SmallVector<llvm::BasicBlock *, 9> exitBlocks;
      loop->getExitBlocks(exitBlocks);
      const auto &blockSet = loop->getBlocksSet();
      auto loopHeader = loop->getHeader();

      // Use a replacement node to replace the loop body
      BasicBlockNodePtr loopReplaceNode(BasicBlockNode::Type::LOOP_HEADER_REPL,
                                        loopHeader);
      tempCallerNodes.emplace(loopReplaceNode);
      BasicBlockNodePtr loopHeaderNode = getBBNodePtr(loopHeader);
      subLoopReplaceNodes.emplace(loopHeaderNode);

      for (const auto &block : blockSet)
        comboGraph.rawGraph.erase(getBBNodePtr(block));

      for (auto &graphPair : comboGraph.rawGraph) {
        std::unordered_set<BasicBlockNodePtr> updatedSuccNodes;
        for (const auto &succ : graphPair.second) {
          if (succ->isNull || blockSet.find(succ->realValue) == blockSet.end())
            updatedSuccNodes.emplace(succ);
          else if (succ->realValue == loopHeader)
            updatedSuccNodes.emplace(loopReplaceNode);
        }
        graphPair.second = updatedSuccNodes;
      }
      comboGraph.rawGraph[loopReplaceNode];

      // Link the replacement node with these exit nodes in the transformed
      // graph
      for (const auto &exitBlock : exitBlocks) {
        auto exitNodePtr = getBBNodePtr(exitBlock);

        if (comboGraph.rawGraph.find(exitNodePtr) !=
            comboGraph.rawGraph.end()) {
          comboGraph.rawGraph[loopReplaceNode].emplace(exitNodePtr);
        } else if (comboGraph.isRawLoop) {
          // Add combo exit edges and exit nodes
          comboGraph.comboExitEdges[loopReplaceNode].emplace(exitNodePtr);
          comboGraph.comboExitNodes.emplace(exitNodePtr);
        }
      }

      subLoopQueue.push(loop);
      tempComboCallGraph.back().emplace(loopHeaderNode);
    }

    // Update exit edges
    if (comboGraph.isRawLoop) {
      std::unordered_set<BasicBlockNodePtr> tempErasedExitingNodes;
      std::unordered_set<BasicBlockNodePtr> tempErasedExitNodes;
      for (const auto &exitEdge : comboGraph.exitEdges) {
        if (subLoopReplaceNodes.find(exitEdge.first) !=
            subLoopReplaceNodes.end()) {
          tempErasedExitingNodes.emplace(exitEdge.first);
          Utility::merge(tempErasedExitNodes, exitEdge.second);
        } else if (comboGraph.rawGraph.find(exitEdge.first) ==
                   comboGraph.rawGraph.end()) {
          tempErasedExitingNodes.emplace(exitEdge.first);
          Utility::merge(tempErasedExitNodes, exitEdge.second);
        } else {
          for (const auto &exitEdgeSecond : exitEdge.second) {
            tempErasedExitNodes.erase(exitEdgeSecond);
          }
        }
      }
      for (const auto &tempExitingNode : tempErasedExitingNodes) {
        comboGraph.exitEdges.erase(tempExitingNode);
      }
      for (const auto &tempExitNode : tempErasedExitNodes) {
        comboGraph.exitNodes.erase(tempExitNode);
      }

      // Add header exit node
      Utility::merge(comboGraph.exitNodes, comboGraph.headerExitNodes);

      // Add an exit edge for each exiting node except the header node
      for (const auto &exitEdge : comboGraph.exitEdges) {
        for (const auto &exitEdgeSecond : exitEdge.second) {
          BasicBlockNodePtr replaceExitNode(
              BasicBlockNode::Type::LOOP_EXIT_REPL, exitEdgeSecond->realValue);

          comboGraph.rawGraph[exitEdge.first].emplace(replaceExitNode);
          comboGraph.rawGraph[replaceExitNode] = {};
        }
      }
      for (const auto &exitNode : comboGraph.headerExitNodes) {
        BasicBlockNodePtr replaceExitNode(BasicBlockNode::Type::LOOP_EXIT_REPL,
                                          exitNode->realValue);

        comboGraph.rawGraph[comboGraph.header].emplace(replaceExitNode);
        comboGraph.rawGraph[replaceExitNode] = {};
      }

      // Add an latch edge for each latch node
      for (const auto &latch : comboGraph.latches) {
        BasicBlockNodePtr replaceHeaderNode(
            BasicBlockNode::Type::LOOP_LATCH_REPL,
            comboGraph.header->realValue);

        comboGraph.rawGraph[latch].emplace(replaceHeaderNode);
        comboGraph.rawGraph[replaceHeaderNode] = {};
      }

      // Update `inComboId`
      for (const auto &nodePair : comboGraph.rawGraph)
        nodePair.first->inComboID = comboGraph.id;
    }
  }

  // Complete calling index
  for (auto callerNode : tempCallerNodes)
    callerNode->callComboID = getBBNodePtr(callerNode->adjunctBlock)->inComboID;

  //
  // Complete directed acyclic graphs

  for (auto &comboGraph : comboGraphs) {
    for (const auto &loopHeaderNode : tempComboCallGraph[comboGraph.id])
      comboGraph.callingCombos.emplace(loopHeaderNode->inComboID);

    comboGraph.dag = std::make_shared<DirectedAcyclicGraph<BasicBlockNodePtr>>(
        comboGraph.rawGraph.size(), comboGraph.header);

    // Construct DAG
    for (const auto &graphPair : comboGraph.rawGraph) {
      comboGraph.dag->addVertex(graphPair.first);
      for (const auto &vNode : graphPair.second)
        comboGraph.dag->addEdge(graphPair.first, vNode);
    }

    bool tempGraphHasValidEntry = true, tempGraphIsConnected = true,
         tempGraphIsAcyclic = true;
    if (!comboGraph.dag->checkValidEntry()) {
      hasInvalidEntry = true;
      tempGraphHasValidEntry = false;
    }
    if (!comboGraph.dag->checkConnected()) {
      hasUnconnectedComponents = true;
      tempGraphIsConnected = false;
    }
    if (!comboGraph.dag->checkAcyclic()) {
      hasWeirdCycles = true;
      tempGraphIsAcyclic = false;
    }

    // [DEBUG]
    if (Logging::check(Logging::Type::GRAPH)) {
      std::string title = std::string("Combo ") +
                          std::to_string(comboGraph.id) + " in Function " +
                          Utility::getFunctionName(func);

      Logging::start(Logging::Type::GRAPH, title);

      Logging::log() << "Valid Entry: " << tempGraphHasValidEntry
                     << " | Connected: " << tempGraphIsConnected
                     << " | Acyclic: " << tempGraphIsAcyclic << "\n";

      std::string result;
      result += "[LOOP] Header: " + comboGraph.header->name + "\n";

      result += "\n";
      for (const auto &latch : comboGraph.latches)
        result += "Latch: " + latch->name + "\n";

      result += "\n";
      for (const auto &exitBlock : comboGraph.exitNodes)
        result += "Exit Node: " + exitBlock->name + "\n";

      result += "\n";
      for (const auto &headerExitBlock : comboGraph.headerExitNodes)
        result += "Header Exit Node: " + headerExitBlock->name + "\n";

      result += "\n";
      for (const auto &exitEdge : comboGraph.exitEdges)
        for (const auto &exitEdgeSecond : exitEdge.second)
          result += "Exit Edge: " + exitEdge.first->name + " -> " +
                    exitEdgeSecond->name + "\n";

      result += "\n";
      for (const auto &comboExitBlock : comboGraph.comboExitNodes)
        result += "Combo Exit Node: " + comboExitBlock->name + "\n";

      result += "\n";
      for (const auto &comboExitEdge : comboGraph.comboExitEdges)
        for (const auto &comboExitEdgeSecond : comboExitEdge.second)
          result += "Combo Exit Edge: " + comboExitEdge.first->name + " -> " +
                    comboExitEdgeSecond->name + "\n";

      Logging::log(result);

      Logging::log() << "\n"
                     << Utility::getGraphInDotFormat(comboGraph.rawGraph, title)
                     << "\n";

      Logging::stop();
    }
  }
  if (!empty()) {
    for (auto &comboGraph : comboGraphs) {
      StaticThreadPool->async([&]() {
        comboGraph.dag->genMinimumPathCovers(EmpcSearcherGraphMaxMatching *
                                             Utility::MaxMatchingNumScale);

        // Hint
        if (EmpcSearcherGraphShowAnalyzingProgress) {
          std::string outputStr =
              "[Program Graph Analysis] Func ID: " + std::to_string(id) +
              "; Func Name: " + Utility::getFunctionName(this->func) +
              "; Combo ID: " + std::to_string(comboGraph.id) + ";\n";
          llvm::outs() << outputStr;
        }
      });
    }
  }
}

bool IntraProcGraph::queryStepNode(BasicBlockNodePtr currNode,
                                   BasicBlockNodePtr nextNode,
                                   const PathReservedInfo &reservedInfo) {
  TempReservedStackInfo singleReservedInfo;

  if (nextNode == entry) {
    singleReservedInfo.type = StackChangeType::INIT;
    singleReservedInfo.targetComboGraphID = 0;

    std::size_t nextNodeInCombo = nextNode->inComboID;
    auto &nextComboGraph = comboGraphs[nextNodeInCombo];

    DirectedAcyclicGraph<Empc::BasicBlockNodePtr>::PathReservedInfo
        dagReservedInfo;
    if (!nextComboGraph.dag->queryStepNode(nextNode, nextNode, dagReservedInfo))
      singleReservedInfo.success = false;

    singleReservedInfo.changedFrameDAG = {dagReservedInfo, 1};

    goto RESULT_CHECK;
  } else {
    assert(currNode && nextNode);
    assert(Utility::find(currNode->successors, nextNode));
    assert(!reservedInfo.graphCallStack.empty());

    std::size_t currNodeInCombo = currNode->inComboID;
    std::size_t nextNodeInCombo = nextNode->inComboID;
    auto &currComboGraph = comboGraphs[currNodeInCombo];
    auto &nextComboGraph = comboGraphs[nextNodeInCombo];

    if (currNodeInCombo == nextNodeInCombo) {
      singleReservedInfo.targetComboGraphID = currNodeInCombo;

      // Latch
      if (nextNode == currComboGraph.header) {
        assert(currComboGraph.latches.find(currNode) !=
               currComboGraph.latches.end());

        singleReservedInfo.type = StackChangeType::LOOP_LOOP;

        // Find the complementary latch node
        BasicBlockNodePtr replLatchNode;
        for (auto successor : currComboGraph.rawGraph.at(currNode)) {
          if (successor->type == BasicBlockNode::Type::LOOP_LATCH_REPL &&
              successor->adjunctBlock == currComboGraph.header->realValue) {
            replLatchNode = successor;
            break;
          }
        }
        assert(replLatchNode);
        singleReservedInfo.compLatchNode = replLatchNode;

        if (!currComboGraph.dag->queryStepNode(
                currNode, replLatchNode,
                reservedInfo.dagGraphStack.top().first))
          singleReservedInfo.success = false;

        singleReservedInfo.changedFrameDAG.second =
            reservedInfo.dagGraphStack.top().second + 1;
        // if (!currComboGraph.dag->queryStepNode(nextNode, nextNode,
        // singleReservedInfo.changedFrameDAG.first))
        //     singleReservedInfo.success = false;

        goto RESULT_CHECK;
      } else {
        singleReservedInfo.type = StackChangeType::NORMAL;

        if (!currComboGraph.dag->queryStepNode(
                currNode, nextNode, reservedInfo.dagGraphStack.top().first))
          singleReservedInfo.success = false;

        goto RESULT_CHECK;
      }
    } else {
      singleReservedInfo.success = true;
      singleReservedInfo.targetComboGraphID = nextNodeInCombo;
      singleReservedInfo.prevComboGraphID = currNodeInCombo;

      BasicBlockNodePtr currGraphSuccNode;
      for (auto successor : currComboGraph.rawGraph.at(currNode)) {
        if (successor->type == BasicBlockNode::Type::LOOP_HEADER_REPL &&
            successor->callComboID == nextNodeInCombo) {
          currGraphSuccNode = successor;
          break;
        }
      }

      if (!currGraphSuccNode) {
        // Get out of loop: the header or the exiting
        if (currNode == currComboGraph.header)
          assert(currComboGraph.headerExitNodes.find(nextNode) !=
                 currComboGraph.headerExitNodes.end());
        else
          assert(currComboGraph.exitEdges.find(currNode) !=
                     currComboGraph.exitEdges.end() &&
                 currComboGraph.exitEdges.at(currNode).find(nextNode) !=
                     currComboGraph.exitEdges.at(currNode).end());

        singleReservedInfo.type = StackChangeType::EXIT_LOOP;

        auto graphCallStack = reservedInfo.graphCallStack;
        auto dagCallStack = reservedInfo.dagGraphStack;

        // Find the complementary exiting node
        BasicBlockNodePtr replExitingNode;
        for (auto successor : currComboGraph.rawGraph.at(currNode)) {
          if (successor->type == BasicBlockNode::Type::LOOP_EXIT_REPL &&
              successor->adjunctBlock == nextNode->realValue) {
            replExitingNode = successor;
            break;
          }
        }
        assert(replExitingNode);
        singleReservedInfo.compExitingNode = replExitingNode;

        if (!currComboGraph.dag->queryStepNode(currNode, replExitingNode,
                                               dagCallStack.top().first))
          singleReservedInfo.success = false;

        // Find the previous caller node
        BasicBlockNodePtr callerNode;
        while (!graphCallStack.empty()) {
          if (graphCallStack.top().first == nextNodeInCombo)
            break;

          callerNode = graphCallStack.top().second;
          graphCallStack.pop();
          dagCallStack.pop();
        }

        // The caller node must have been found
        assert(callerNode && callerNode->inComboID == nextNodeInCombo);

        if (!nextComboGraph.dag->queryStepNode(callerNode, nextNode,
                                               dagCallStack.top().first))
          singleReservedInfo.success = false;

        singleReservedInfo.changedFrameGraph = graphCallStack.top();

        goto RESULT_CHECK;
      } else {
        // Enter loop
        assert(nextNode == nextComboGraph.header);

        singleReservedInfo.type = StackChangeType::ENTER_LOOP;
        singleReservedInfo.compCallerNode = currGraphSuccNode;

        // Query the node stepping in current DAG
        if (!currComboGraph.dag->queryStepNode(
                currNode, currGraphSuccNode,
                reservedInfo.dagGraphStack.top().first))
          singleReservedInfo.success = false;

        // Query the node stepping (entering) in the next DAG
        singleReservedInfo.changedFrameDAG.second = 1;
        if (!nextComboGraph.dag->queryStepNode(
                nextNode, nextNode, singleReservedInfo.changedFrameDAG.first))
          singleReservedInfo.success = false;

        singleReservedInfo.changedFrameGraph = {nextNodeInCombo,
                                                currGraphSuccNode};

        goto RESULT_CHECK;
      }
    }
  }

RESULT_CHECK:
  tempReservedStackInfo[nextNode] = singleReservedInfo;
  return singleReservedInfo.success;
}

void IntraProcGraph::ensureStepNode(BasicBlockNodePtr nextNode,
                                    PathReservedInfo &reservedInfo,
                                    bool forceNext) {
  assert(tempReservedStackInfo.find(nextNode) != tempReservedStackInfo.end() &&
         "Wrong status when ensuring to step node");

  auto &singleReservedInfo = tempReservedStackInfo.at(nextNode);

  assert(forceNext || singleReservedInfo.success);

  auto &targetComboGraph = comboGraphs[singleReservedInfo.targetComboGraphID];

  switch (singleReservedInfo.type) {
  case StackChangeType::INIT: {
    Utility::clear(reservedInfo.dagGraphStack);
    Utility::clear(reservedInfo.graphCallStack);

    targetComboGraph.dag->ensureStepNode(
        nextNode, singleReservedInfo.changedFrameDAG.first, false, forceNext);

    reservedInfo.currNode = entry;
    reservedInfo.dagGraphStack.push(singleReservedInfo.changedFrameDAG);
    reservedInfo.graphCallStack.emplace(0, BasicBlockNodePtr());
  } break;

  case StackChangeType::NORMAL: {
    targetComboGraph.dag->ensureStepNode(
        nextNode, reservedInfo.dagGraphStack.top().first, false, forceNext);

    reservedInfo.currNode = nextNode;
  } break;

  case StackChangeType::LOOP_LOOP: {
    // Ensure going through the complementary latch node
    targetComboGraph.dag->ensureStepNode(singleReservedInfo.compLatchNode,
                                         reservedInfo.dagGraphStack.top().first,
                                         false, forceNext);

    // Exit current DAG graph
    targetComboGraph.dag->ensureStepNode(singleReservedInfo.compLatchNode,
                                         reservedInfo.dagGraphStack.top().first,
                                         true, forceNext);

    // Replace DAG reserved info with a new one
    reservedInfo.dagGraphStack.top() = singleReservedInfo.changedFrameDAG;

    // Query and ensure going through the next node
    targetComboGraph.dag->queryStepNode(nextNode, nextNode,
                                        reservedInfo.dagGraphStack.top().first);
    targetComboGraph.dag->ensureStepNode(
        nextNode, reservedInfo.dagGraphStack.top().first, false, forceNext);

    reservedInfo.currNode = nextNode;
  } break;

  case StackChangeType::EXIT_LOOP: {
    auto &prevComboGraph = comboGraphs[singleReservedInfo.prevComboGraphID];

    // Ensure going through the complementary exiting node
    prevComboGraph.dag->ensureStepNode(singleReservedInfo.compExitingNode,
                                       reservedInfo.dagGraphStack.top().first,
                                       false, forceNext);

    // Exit current DAG graph
    prevComboGraph.dag->ensureStepNode(singleReservedInfo.compExitingNode,
                                       reservedInfo.dagGraphStack.top().first,
                                       true, forceNext);

    // Pop frames
    while (reservedInfo.graphCallStack.top().first !=
           singleReservedInfo.changedFrameGraph.first) {
      reservedInfo.graphCallStack.pop();
      reservedInfo.dagGraphStack.pop();
    }

    // Ensure exit node in the caller combo graph
    targetComboGraph.dag->ensureStepNode(
        nextNode, reservedInfo.dagGraphStack.top().first, false, forceNext);

    reservedInfo.currNode = nextNode;
  } break;

  case StackChangeType::ENTER_LOOP: {
    auto &prevComboGraph = comboGraphs[singleReservedInfo.prevComboGraphID];

    // Ensure current combo graph stepping
    prevComboGraph.dag->ensureStepNode(singleReservedInfo.compCallerNode,
                                       reservedInfo.dagGraphStack.top().first,
                                       false, forceNext);

    // Ensure the header in the next combo graph
    targetComboGraph.dag->ensureStepNode(
        nextNode, singleReservedInfo.changedFrameDAG.first, false, forceNext);

    reservedInfo.currNode = nextNode;
    reservedInfo.graphCallStack.push(singleReservedInfo.changedFrameGraph);
    reservedInfo.dagGraphStack.push(singleReservedInfo.changedFrameDAG);
  } break;

  case StackChangeType::EXIT: {
    reservedInfo.currNode = nextNode;

    targetComboGraph.dag->ensureStepNode(
        nextNode, reservedInfo.dagGraphStack.top().first, true, forceNext);

    Utility::clear(reservedInfo.graphCallStack);
    Utility::clear(reservedInfo.dagGraphStack);
  } break;

  default:
    assert(false);
    break;
  }

  tempReservedStackInfo.clear();
}

bool IntraProcGraph::queryStepBasicBlock(const llvm::BasicBlock *currBB,
                                         const llvm::BasicBlock *nextBB,
                                         const PathReservedInfo &reservedInfo) {
  if (nextBB == entry->realValue)
    return queryStepNode(getBBNodePtr(nextBB), getBBNodePtr(nextBB),
                         reservedInfo);
  else
    return queryStepNode(getBBNodePtr(currBB), getBBNodePtr(nextBB),
                         reservedInfo);
}

void IntraProcGraph::ensureStepBasicBlock(const llvm::BasicBlock *nextBB,
                                          PathReservedInfo &reservedInfo,
                                          bool isExit, bool forceNext) {
  BasicBlockNodePtr nextNode = getBBNodePtr(nextBB);
  if (isExit) {
    assert(nextNode->successors.empty());

    TempReservedStackInfo singleReservedInfo(StackChangeType::EXIT);
    singleReservedInfo.targetComboGraphID = nextNode->inComboID;
    tempReservedStackInfo[nextNode] = singleReservedInfo;

    ensureStepNode(nextNode, reservedInfo, true);
  } else {
    ensureStepNode(nextNode, reservedInfo, forceNext);
  }
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
static std::unordered_map<const llvm::Function *, llvm::LoopInfo *>
    FuncLoopInfo;

class DetectLoopFunctionPass
    : public llvm::PassInfoMixin<DetectLoopFunctionPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &funcAnalyManager) {
    llvm::LoopInfo &li = funcAnalyManager.getResult<llvm::LoopAnalysis>(func);
    FuncLoopInfo[&func] = &li;
    return llvm::PreservedAnalyses::all();
  }
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
InterProcGraph::InterProcGraph(
    llvm::Module *_module, const llvm::Function *_entry,
    const std::unordered_map<std::string, bool> &definedFunctions)
    : entry(_entry) {
  assert(_module && _entry);

  // Ensure maximum number for maximum matchings
  llvm::outs()
      << "[Program Graph Analysis] Maximum count for maximum matchings: "
      << EmpcSearcherGraphMaxMatching * Utility::MaxMatchingNumScale << "\n";

  // Ensure thread count
  unsigned threadCount = EmpcSearcherGraphThreadCount;
  if (!threadCount) {
    unsigned threadCount = llvm::get_cpus();
    threadCount = threadCount > 1 ? (threadCount / 2) : threadCount;
  }

  llvm::outs() << "[Program Graph Analysis] Thread count: " << threadCount
               << "\n";
  StaticThreadPool = std::make_unique<llvm::ThreadPool>(
      llvm::hardware_concurrency(threadCount));

  llvm::PassBuilder passBuilder;
  llvm::FunctionPassManager funcPassManager;
  llvm::FunctionAnalysisManager funcAnalysisManager;
  passBuilder.registerFunctionAnalyses(funcAnalysisManager);
  funcPassManager.addPass(DetectLoopFunctionPass());
  FuncLoopInfo.clear();

  // const auto &funcList = _module->getFunctionList();
  // std::size_t funcNum = funcList.size();
  std::size_t handledFuncID = 0;

  for (llvm::Module::iterator F = _module->begin(); F != _module->end(); ++F) {
    ++handledFuncID;

    auto func = &(*F);

    auto foundIter = definedFunctions.find(func->getName().str());
    if (func != _entry &&
        (foundIter == definedFunctions.end() || !foundIter->second ||
         Utility::isBlacklistedFunction(func)))
      continue;

    if (Utility::isBlacklistedFunction(func))
      continue;
    if (func->isDeclaration() || func->isIntrinsic())
      continue;

    // Detect loops in this function
    funcPassManager.run(*func, funcAnalysisManager);

    // Initialize intra-procedural CFG
    graph[func] = std::make_shared<IntraProcGraph>(handledFuncID, func,
                                                   FuncLoopInfo.at(func));
  }

  FuncLoopInfo.clear();

  auto iter = graph.find(_entry);
  assert(iter != graph.end() && "Failed to find entry function in the module");
  assert(!iter->second->empty() && "The entry function can NOT be empty");

  StaticThreadPool->wait();
  std::cout << "[DEBUG] End of analysis" << std::endl;
}

bool InterProcGraph::queryStepBasicBlock(const llvm::BasicBlock *currBasicBlock,
                                         const llvm::BasicBlock *nextBasicBlock,
                                         const PathReservedInfo &reservedInfo,
                                         StateStepType stepType) {
  const llvm::Function *nextFunc =
      nextBasicBlock ? nextBasicBlock->getParent() : nullptr;
  const llvm::Function *currFunc =
      currBasicBlock ? currBasicBlock->getParent() : nullptr;
  TempReservedStackInfo singleReservedInfo;

  // [DEBUG]
  if (Logging::check(Logging::Type::DEBUG)) {
    Logging::start(Logging::Type::DEBUG, "Before ICFG Querying");
    Logging::log() << "Stepping Type: " << (unsigned)stepType << "\n";
    Logging::stop();
  }

  switch (stepType) {
  case StateStepType::PUSH: {
    assert(nextBasicBlock && nextFunc);

    if (reservedInfo.graphCallStack.empty() && nextFunc != entry) {
      singleReservedInfo.type = StackChangeType::NOTHING;
    } else {
      singleReservedInfo.type = StackChangeType::PUSH_CALL;

      if (!empty(nextFunc)) {
        auto nextFuncIPG = graph.at(nextFunc);
        if (nextFuncIPG->queryStepBasicBlock(
                nextBasicBlock, nextBasicBlock,
                singleReservedInfo.changedFrameIPG))
          singleReservedInfo.success = true;
        else
          singleReservedInfo.success = false;
      }

      singleReservedInfo.changedFrameGraph = {nextFunc, currBasicBlock};
    }
  } break;

  case StateStepType::POP: {
    assert(currBasicBlock && currFunc);

    if (reservedInfo.graphCallStack.empty()) {
      singleReservedInfo.type = StackChangeType::NOTHING;
    } else {
      singleReservedInfo.success = true;
      singleReservedInfo.exitingFunction = currFunc;
      singleReservedInfo.exitingBasicBlock = currBasicBlock;

      if (reservedInfo.graphCallStack.size() == 1) {
        assert(currFunc == entry);

        singleReservedInfo.type = StackChangeType::EXIT;
        if (!nextBasicBlock)
          nextBasicBlock = currBasicBlock;
      } else {
        assert(nextFunc && nextBasicBlock);
        singleReservedInfo.type = StackChangeType::POP_CALL;

        //
        // Check the caller and the return BB

        const llvm::BasicBlock *callerBB =
            reservedInfo.graphCallStack.top().second;

        assert(callerBB && callerBB->getParent() == nextFunc);

        // For some C++ programs which ignore the return basic blocks
        if (callerBB != nextBasicBlock) {
          singleReservedInfo.callerNotMatch = true;

          if (!empty(nextFunc)) {
            auto ifgCallGraph = reservedInfo.ipgCallStack;
            ifgCallGraph.pop();

            auto nextFuncIPG = graph.at(nextFunc);
            if (!nextFuncIPG->queryStepBasicBlock(callerBB, nextBasicBlock,
                                                  ifgCallGraph.top()))
              singleReservedInfo.success = false;
          }
        }
      }
    }
  } break;

  case StateStepType::COMMON: {
    assert(currBasicBlock && currFunc && nextBasicBlock && nextFunc &&
           currFunc == nextFunc);

    if (reservedInfo.graphCallStack.empty()) {
      singleReservedInfo.type = StackChangeType::NOTHING;
    } else {
      assert(reservedInfo.currFunc == currFunc);

      singleReservedInfo.type = StackChangeType::COMMON;

      if (!empty(currFunc)) {
        auto currFuncIPG = graph.at(currFunc);
        if (currFuncIPG->queryStepBasicBlock(currBasicBlock, nextBasicBlock,
                                             reservedInfo.ipgCallStack.top()))
          singleReservedInfo.success = true;
        else
          singleReservedInfo.success = false;
      }
    }
  } break;

  default:
    assert(false && "Unknown state stepping type");
    break;
  }

  tempReservedStackInfo[nextBasicBlock] = singleReservedInfo;
  return singleReservedInfo.success;
}

void InterProcGraph::ensureStepBasicBlock(
    const llvm::BasicBlock *nextBasicBlock, PathReservedInfo &reservedInfo,
    bool forceNext) {
  assert(nextBasicBlock);
  assert(tempReservedStackInfo.find(nextBasicBlock) !=
             tempReservedStackInfo.end() &&
         "Wrong status when ensuring to step node");

  auto &singleReservedInfo = tempReservedStackInfo.at(nextBasicBlock);

  switch (singleReservedInfo.type) {
  case StackChangeType::PUSH_CALL: {
    const llvm::Function *nextFunc = nextBasicBlock->getParent();

    if (!empty(nextFunc)) {
      auto nextFuncIPG = graph.at(nextFunc);
      nextFuncIPG->ensureStepBasicBlock(
          nextBasicBlock, singleReservedInfo.changedFrameIPG, false, forceNext);
    }

    reservedInfo.ipgCallStack.push(singleReservedInfo.changedFrameIPG);
    reservedInfo.graphCallStack.push(singleReservedInfo.changedFrameGraph);
    reservedInfo.currFunc = nextFunc;
    reservedInfo.currBasicBlock = nextBasicBlock;
  } break;

  case StackChangeType::POP_CALL: {
    const llvm::BasicBlock *currBasicBlock =
        singleReservedInfo.exitingBasicBlock;
    const llvm::Function *currFunc = singleReservedInfo.exitingFunction;

    if (!empty(currFunc)) {
      auto currFuncIPG = graph.at(currFunc);
      currFuncIPG->ensureStepBasicBlock(
          currBasicBlock, reservedInfo.ipgCallStack.top(), true, forceNext);
    }

    reservedInfo.ipgCallStack.pop();
    reservedInfo.graphCallStack.pop();

    currBasicBlock = nextBasicBlock;
    currFunc = currBasicBlock->getParent();

    // Caller BB not matched for some C++ programs
    if (!empty(currFunc) && singleReservedInfo.callerNotMatch) {
      auto currFuncIPG = graph.at(currFunc);
      currFuncIPG->ensureStepBasicBlock(
          nextBasicBlock, reservedInfo.ipgCallStack.top(), false, forceNext);
    }

    reservedInfo.currBasicBlock = currBasicBlock;
    reservedInfo.currFunc = currFunc;
  } break;

  case StackChangeType::COMMON: {
    const llvm::Function *currFunc = nextBasicBlock->getParent();

    if (!empty(currFunc)) {
      auto currFuncIPG = graph.at(currFunc);
      currFuncIPG->ensureStepBasicBlock(
          nextBasicBlock, reservedInfo.ipgCallStack.top(), false, forceNext);
    }

    reservedInfo.currBasicBlock = nextBasicBlock;
    reservedInfo.currFunc = currFunc;
  } break;

  case StackChangeType::EXIT: {
    const llvm::BasicBlock *currBasicBlock =
        singleReservedInfo.exitingBasicBlock;
    const llvm::Function *currFunc = singleReservedInfo.exitingFunction;
    auto currFuncIPG = graph.at(currFunc);

    currFuncIPG->ensureStepBasicBlock(
        currBasicBlock, reservedInfo.ipgCallStack.top(), true, forceNext);

    reservedInfo.ipgCallStack.pop();
    reservedInfo.graphCallStack.pop();
    reservedInfo.currBasicBlock = nullptr;
    reservedInfo.currFunc = nullptr;
  } break;

  case StackChangeType::NOTHING:
    break;

  default:
    assert(false && "Unknown stack change type");
    break;
  }

  tempReservedStackInfo.clear();
}
} // namespace Empc
} // namespace klee