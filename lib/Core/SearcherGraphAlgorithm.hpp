//===-- SearcherGraphAlgorithm.hpp ------------------------------*- C++ -*-===//
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

#ifndef EMPC_SEARCHERGRAPHALGORITHM_HPP_
#define EMPC_SEARCHERGRAPHALGORITHM_HPP_

#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include <algorithm>
#include <deque>
#include <functional>
#include <list>
#include <queue>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fstream>
#include <iostream>

namespace klee {
namespace Empc {
namespace Utility {
template <typename _Tp>
void merge(std::unordered_set<_Tp> &set1, const std::unordered_set<_Tp> &set2) {
  for (const auto &elem : set2)
    set1.emplace(elem);
}

template <typename _Tp>
void merge(std::list<_Tp> &list1, std::list<_Tp> &list2) {
  list1.splice(list1.end(), list2);
}

template <typename _Tp>
void intersection(std::unordered_set<_Tp> &interset,
                  const std::unordered_set<_Tp> &set1,
                  const std::unordered_set<_Tp> &set2) {
  for (const auto &elem : set1)
    if (set2.find(elem) != set2.cend())
      interset.emplace(elem);
}

template <typename _Tp>
void difference(std::unordered_set<_Tp> &diffset,
                const std::unordered_set<_Tp> &set1,
                const std::unordered_set<_Tp> &set2) {
  for (const auto &elem : set1)
    if (set2.find(elem) == set2.cend())
      diffset.emplace(elem);
}

template <typename _Tp>
void difference(std::unordered_set<_Tp> &set1,
                const std::unordered_set<_Tp> &set2) {
  std::unordered_set<_Tp> result;
  difference(result, set1, set2);
  set1 = result;
}

template <typename _Tp>
bool equal(const std::unordered_set<_Tp> &set1,
           const std::unordered_set<_Tp> &set2) {
  std::unordered_set<_Tp> tempSet;
  difference(tempSet, set1, set2);
  return tempSet.empty();
}

template <typename _Tp>
bool equal(const std::unordered_map<_Tp, std::unordered_set<_Tp>> &set1,
           const std::unordered_map<_Tp, std::unordered_set<_Tp>> &set2) {
  if (set1.size() != set2.size())
    return false;
  for (const auto &mapPair : set1)
    if (set2.find(mapPair.first) == set2.end() ||
        !equal(mapPair.second, set2.at(mapPair.first)))
      return false;
  return true;
}

template <typename _Tp> void clear(std::stack<_Tp> &_stack) {
  while (!_stack.empty())
    _stack.pop();
}
} // namespace Utility
} // namespace Empc
} // namespace klee

/// @brief Definitions of bipartite graph `BiGraph`
namespace klee {
namespace Empc {
template <typename _Tp> class BipartiteGraph {
private:
  std::size_t leftSize;
  std::size_t rightSize;

  std::size_t leftIndex;
  std::size_t rightIndex;

  std::vector<std::unordered_set<std::size_t>> leftEdges;
  std::vector<std::unordered_set<std::size_t>> rightEdges;

  std::vector<std::unordered_set<std::size_t>> undiLeftEdges;
  std::vector<std::unordered_set<std::size_t>> undiRightEdges;

  std::vector<_Tp> leftNodes;
  std::vector<_Tp> rightNodes;

  std::unordered_map<_Tp, std::size_t> leftNodeMap;
  std::unordered_map<_Tp, std::size_t> rightNodeMap;

  std::list<std::vector<std::pair<std::size_t, std::size_t>>> allMatchings;
  std::size_t maxMatchingNumber;

  void getMaximumMatching(
      std::vector<std::pair<std::size_t, std::size_t>> &matching);

  void enumMaximumMatchingIter(
      const std::vector<std::unordered_set<std::size_t>> &graphLeftEdges,
      const std::vector<std::unordered_set<std::size_t>> &graphRightEdges,
      const std::vector<std::pair<std::size_t, std::size_t>> &matching,
      const std::list<std::pair<std::size_t, std::size_t>> &addedEdges,
      bool hasCycles);

public:
  BipartiteGraph(std::size_t _leftCount, std::size_t _rightCount);

  ~BipartiteGraph() = default;

  void addEdge(const _Tp &leftElement, const _Tp &rightElement,
               bool lToR = true);

  void addEdge(const std::pair<_Tp, _Tp> &elementPair, bool lToR = true);

  /// @brief Get the maximum matching using Hopcroft Karp algorithm
  /// @param matching The result maximum matching with edge direction
  ///
  /// @refitem John E. Hopcroft and Richard M. Karp. "An n^{5 / 2} Algorithm for
  /// Maximum Matchings in Bipartite Graphs" In: **SIAM Journal of
  /// Computing** 2.4 (1973), pp. 225--231. <https://doi.org/10.1137/0202019>.
  /// @refitem Hopcroft Karp algorithm implemented by NetworkX bipartite graph
  /// in Python
  /// https://networkx.org/documentation/stable/reference/algorithms/generated/networkx.algorithms.bipartite.matching.hopcroft_karp_matching.html
  void getMaximumMatching(std::vector<std::tuple<_Tp, _Tp, bool>> &matching);

  /// @brief Get the maximum matching using Hopcroft Karp algorithm
  /// @param matching The result maximum matching
  ///
  /// @refitem John E. Hopcroft and Richard M. Karp. "An n^{5 / 2} Algorithm for
  /// Maximum Matchings in Bipartite Graphs" In: **SIAM Journal of
  /// Computing** 2.4 (1973), pp. 225--231. <https://doi.org/10.1137/0202019>.
  /// @refitem Hopcroft Karp algorithm implemented by NetworkX bipartite graph
  /// in Python
  /// https://networkx.org/documentation/stable/reference/algorithms/generated/networkx.algorithms.bipartite.matching.hopcroft_karp_matching.html
  void
  getMaximumMatching(std::unordered_map<_Tp, std::pair<_Tp, bool>> &matching);

  bool
  checkMatchingValid(const std::vector<std::tuple<_Tp, _Tp, bool>> &matching);

  bool checkMatchingValid(
      const std::unordered_map<_Tp, std::pair<_Tp, bool>> &matching);

  /// @brief Enumerate all possible maximum matchings
  /// @param matchings
  void enumMaximumMatchings(
      std::list<std::unordered_map<_Tp, std::pair<_Tp, bool>>> &matchings,
      std::size_t maxNum = 0);
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

// @brief A union-find pair set for MPC construction
template <typename _Tp> class UnionFindPairSet {
private:
  std::list<std::list<_Tp>> ufSet;
  std::unordered_set<_Tp> vertexSet;
  std::unordered_map<_Tp, typename std::list<std::list<_Tp>>::iterator>
      uVertexMap;
  std::unordered_map<_Tp, typename std::list<std::list<_Tp>>::iterator>
      vVertexMap;

public:
  UnionFindPairSet() = default;
  ~UnionFindPairSet() = default;

  void addEdge(const _Tp &uVertex, const _Tp &vVertex);

  void getResultUFSet(std::list<std::list<_Tp>> &result,
                      std::unordered_set<_Tp> &vertices);
};

template <typename _Tp> class DirectedAcyclicGraph {
public:
  typedef std::size_t VID;
  typedef std::size_t GID;
  typedef std::size_t CID;
  typedef std::size_t PID;

  const VID VID_MAX = SIZE_MAX;
  const PID PID_MAX = SIZE_MAX;

private:
  struct MinimalPathCover {
    std::deque<std::pair<std::list<VID>, bool>> paths;

    bool selected;

    MinimalPathCover() : selected(true) {}
    MinimalPathCover(const std::deque<std::list<VID>> &_paths)
        : selected(true) {
      for (const auto &_path : _paths)
        paths.emplace_back(_path, true);
    }
    MinimalPathCover(std::vector<std::list<VID>>::iterator vecStart,
                     std::vector<std::list<VID>>::iterator vecLast)
        : selected(true) {
      for (auto iter = vecStart; iter != vecLast; ++iter)
        paths.emplace_back(*iter, true);
    }
  };

  struct MinimalSTGrpah {
    std::size_t size;

    VID sVertex;
    VID tVertex;

    std::unordered_map<VID, std::unordered_set<VID>> graph;
    std::unordered_map<VID, std::unordered_set<VID>> reverseGraph;

    std::unordered_map<VID, std::unordered_set<VID>> reachGraph;
    std::unordered_map<VID, std::unordered_set<VID>> reverseReachGraph;

    std::deque<MinimalPathCover> pathCovers;
    std::unordered_map<std::size_t,
                       std::pair<std::list<VID>, std::unordered_set<CID>>>
        combinedPaths;
    std::unordered_set<std::size_t> coveredPaths;

    std::unordered_set<VID> unvisitedVertices;
    bool allCalleeVisited;

    std::unordered_set<std::size_t> temporaryCoveredPaths;

    MinimalSTGrpah()
        : size(0), sVertex(0), tVertex(0), allCalleeVisited(false) {}
    MinimalSTGrpah(VID _sVertex, VID _tVertex)
        : size(0), sVertex(_sVertex), tVertex(_tVertex),
          allCalleeVisited(false) {}
    MinimalSTGrpah(
        std::size_t _size, VID _sVertex, VID _tVertex,
        const std::unordered_map<VID, std::unordered_set<VID>> &_graph,
        const std::unordered_map<VID, std::unordered_set<VID>> &_rGrpah,
        const std::unordered_map<VID, std::unordered_set<VID>> &_reachGrpah,
        const std::unordered_map<VID, std::unordered_set<VID>> &_rReachGrpah)
        : size(_size), sVertex(_sVertex), tVertex(_tVertex), graph(_graph),
          reverseGraph(_rGrpah), reachGraph(_reachGrpah),
          reverseReachGraph(_rReachGrpah), allCalleeVisited(false) {}
  };

  struct VertexProperty {
    GID inFoldedGraphID;
    GID callFoldedGraphID;
    bool isOriginal;

    VertexProperty()
        : inFoldedGraphID(0), callFoldedGraphID(0), isOriginal(true) {}
    VertexProperty(GID _callFoledGraphID)
        : inFoldedGraphID(0), callFoldedGraphID(_callFoledGraphID),
          isOriginal(false) {}
  };

public:
  struct PathReservedInfo {
    std::stack<std::pair<GID, VID>> graphCallStack;
    std::stack<std::unordered_set<std::size_t>> pathsStack;
    VID currVertex;

    PathReservedInfo() : currVertex(0) {}
    void clear() {
      clear(graphCallStack);
      clear(pathsStack);
      currVertex = 0;
    }
  };

private:
  /// @brief Original entry vertex
  _Tp entryVertex;

  /// @brief The size of vertex set
  std::size_t size;

  /// @brief Entry vertex ID
  const VID entry = 0;

  /// @brief Original graph represented by vertex ID
  std::vector<std::unordered_set<VID>> graph;

  /// @brief Vertex set
  std::vector<VertexProperty> vertices;

  /// @brief All the subgraphs
  std::deque<MinimalSTGrpah> stGraphs;

  /// @brief The call graph of these s-t graphs
  std::unordered_map<GID, std::unordered_set<GID>> stGraphCalls;

  std::vector<_Tp> vertexMap;
  std::unordered_map<_Tp, VID> rVertexMap;

  VID nextVertexID;
  GID nextGraphID;

private:
  enum class StackChangeType {
    INIT,
    SAME_GRAPH,
    PUSH_GRAPH,
    POP_GRAPH,
    POP_PUSH_GRAPH,
    EXIT,
    UNKNOWN,
  };

  struct TempReservedStackInfo {
    StackChangeType type;
    bool success;

    std::list<std::pair<GID, VID>> changedFrameGraph;
    std::list<std::unordered_set<std::size_t>> changedFramePaths;

    std::list<std::pair<GID, VID>> addiChangedFrameGraph;
    std::list<std::unordered_set<std::size_t>> addiChangedFramePaths;

    TempReservedStackInfo() : type(StackChangeType::UNKNOWN), success(false) {}
    TempReservedStackInfo(StackChangeType _type)
        : type(_type), success(false) {}
  };

  std::unordered_map<VID, TempReservedStackInfo> tempReservedStackInfo;

private:
  void splitGraph();

  bool queryStepVertex(VID currVertex, VID nextVertex,
                       const PathReservedInfo &reservedInfo);

  void ensureStepVertex(VID nextVertex, PathReservedInfo &reservedInfo,
                        bool forceNext = false);

public:
  bool verifySTGraphs(std::string &error);

  void dumpGraphToDot();

public:
  DirectedAcyclicGraph(std::size_t _size, const _Tp &_entry);
  ~DirectedAcyclicGraph() = default;

  DirectedAcyclicGraph() = delete;
  DirectedAcyclicGraph(const DirectedAcyclicGraph &) = delete;
  DirectedAcyclicGraph &operator=(const DirectedAcyclicGraph &) = delete;

  void addVertex(const _Tp &verNode);

  void addEdge(const _Tp &parent, const _Tp &child);

  /// @brief Check whether this graph is an acyclic graph
  /// @return Return true if the graph is acyclic
  bool checkAcyclic();

  /// @brief Check whether this graph is a (weakly) connected graph
  /// @return Return true if the graph is connected
  bool checkConnected();

  /// @brief Check whether this graph has a valid entry
  /// @return Return true if the graph has a valid entry
  bool checkValidEntry();

  /// @brief Generate a group of MPCs
  /// @param maxMatchingNum
  void genMinimumPathCovers(std::size_t maxMatchingNum = 0UL);

  /// @brief Query the next node on the iCFG
  /// @param currNode
  /// @param nextNode
  /// @param reservedInfo
  /// @return
  bool queryStepNode(const _Tp &currNode, const _Tp &nextNode,
                     const PathReservedInfo &reservedInfo);

  /// @brief Ensure the selection of the next node provided by MPC on iCFG
  /// @param nextNode
  /// @param reservedInfo
  /// @param isExit
  /// @param forceNext
  void ensureStepNode(const _Tp &nextNode, PathReservedInfo &reservedInfo,
                      bool isExit = false, bool forceNext = false);
};
} // namespace Empc
} // namespace klee

// ================================================================
// definitions
// ================================================================

namespace klee {
namespace Empc {
// Custom hash function for pairs
struct VertexPairHash {
  std::size_t operator()(const std::pair<std::size_t, bool> &p) const {
    auto h1 = std::hash<std::size_t>{}(p.first);
    auto h2 = std::hash<bool>{}(p.second);
    return h1 ^ (h2 << 1); // Combine hashes
  }
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
template <typename _Tp>
BipartiteGraph<_Tp>::BipartiteGraph(std::size_t _leftCount,
                                    std::size_t _rightCount)
    : leftSize(_leftCount), rightSize(_rightCount), leftIndex(0), rightIndex(0),
      maxMatchingNumber(50000ULL) {
  leftEdges.resize(_leftCount);
  rightEdges.resize(_rightCount);
  undiLeftEdges.resize(_leftCount);
  undiRightEdges.resize(_rightCount);
  leftNodes.resize(_leftCount);
  rightNodes.resize(_rightCount);
}

template <typename _Tp>
void BipartiteGraph<_Tp>::addEdge(const _Tp &leftElement,
                                  const _Tp &rightElement, bool lToR) {
  std::size_t curLeftId, curRightId;
  auto leftIter = leftNodeMap.find(leftElement);
  auto rightIter = rightNodeMap.find(rightElement);
  if (leftIter == leftNodeMap.end()) {
    assert(leftIndex < leftSize &&
           "The left node count exceed to the set value");
    curLeftId = leftIndex++;
    leftNodeMap.emplace(leftElement, curLeftId);
    leftNodes[curLeftId] = leftElement;
  } else {
    curLeftId = leftIter->second;
  }
  if (rightIter == rightNodeMap.end()) {
    assert(rightIndex < rightSize &&
           "The right node count exceed to the set value");
    curRightId = rightIndex++;
    rightNodeMap.emplace(rightElement, curRightId);
    rightNodes[curRightId] = rightElement;
  } else {
    curRightId = rightIter->second;
  }

  if (lToR)
    leftEdges[curLeftId].emplace(curRightId);
  else
    rightEdges[curRightId].emplace(curLeftId);
  undiLeftEdges[curLeftId].emplace(curRightId);
  undiRightEdges[curRightId].emplace(curLeftId);
}

template <typename _Tp>
void BipartiteGraph<_Tp>::addEdge(const std::pair<_Tp, _Tp> &elementPair,
                                  bool lToR) {
  addEdge(elementPair.first, elementPair.second, lToR);
}

template <typename _Tp>
void BipartiteGraph<_Tp>::getMaximumMatching(
    std::vector<std::pair<std::size_t, std::size_t>> &matching) {
  static const std::size_t NONE_NODE = std::numeric_limits<std::size_t>::max();
  static const uint64_t INFIN_DIST = std::numeric_limits<uint64_t>::max();

  auto isNoneNode = [](std::size_t node) -> bool { return node == NONE_NODE; };

  auto isInfDist = [](uint64_t dist) -> bool { return dist == INFIN_DIST; };

  auto equalDistance = [&](uint64_t dist1, uint64_t dist2,
                           uint64_t delta = 0) -> bool {
    if (isInfDist(dist1) && isInfDist(dist2))
      return true;
    else if (isInfDist(dist2) || isInfDist(dist1))
      return false;
    else
      return dist1 == (dist2 + delta);
  };

  std::vector<std::size_t> leftMatches(leftSize, NONE_NODE),
      rightMatches(rightSize, NONE_NODE);
  std::unordered_map<std::size_t, uint64_t> leftDistances;
  std::list<std::size_t> leftQueue;

  auto breadthFirstSearch = [&]() -> bool {
    for (std::size_t i = 0; i < this->leftSize; ++i) {
      if (isNoneNode(leftMatches[i])) {
        leftDistances[i] = 0;
        leftQueue.push_back(i);
      } else {
        leftDistances[i] = INFIN_DIST;
      }
    }
    leftDistances[NONE_NODE] = INFIN_DIST;

    while (!leftQueue.empty()) {
      auto leftNode = leftQueue.front();
      leftQueue.pop_front();
      if (leftDistances[leftNode] < leftDistances[NONE_NODE]) ////
      {
        for (auto rightNode : this->undiLeftEdges[leftNode]) {
          if (isInfDist(leftDistances[rightMatches[rightNode]])) {
            leftDistances[rightMatches[rightNode]] =
                leftDistances[leftNode] + 1;
            leftQueue.push_back(rightMatches[rightNode]);
          }
        }
      }
    }

    return !isInfDist(leftDistances[NONE_NODE]);
  };

  std::function<bool(std::size_t)> depthFirstSearch =
      [&](std::size_t leftNode) -> bool {
    if (!isNoneNode(leftNode)) {
      for (auto rightNode : this->undiLeftEdges[leftNode]) {
        if (equalDistance(leftDistances[rightMatches[rightNode]],
                          leftDistances[leftNode], 1)) {
          if (depthFirstSearch(rightMatches[rightNode])) {
            rightMatches[rightNode] = leftNode;
            leftMatches[leftNode] = rightNode;
            return true;
          }
        }
      }
      leftDistances[leftNode] = INFIN_DIST;
      return false;
    }
    return true;
  };

  std::size_t matchingSize = 0;
  while (breadthFirstSearch()) {
    for (std::size_t leftNode = 0; leftNode < leftSize; ++leftNode) {
      if (isNoneNode(leftMatches[leftNode])) {
        if (depthFirstSearch(leftNode)) {
          ++matchingSize;
        }
      }
    }
  }

  matching.clear();
  for (std::size_t leftNode = 0; leftNode < leftSize; ++leftNode) {
    if (!isNoneNode(leftMatches[leftNode])) {
      matching.emplace_back(leftNode, leftMatches[leftNode]);
    }
  }
}

template <typename _Tp>
void BipartiteGraph<_Tp>::getMaximumMatching(
    std::vector<std::tuple<_Tp, _Tp, bool>> &matching) {
  std::vector<std::pair<std::size_t, std::size_t>> innerMatching;
  getMaximumMatching(innerMatching);

  matching.clear();
  for (auto &edge : innerMatching) {
    auto leftNode = edge.first;
    auto rightNode = edge.second;
    if (leftEdges[leftNode].find(rightNode) == leftEdges[leftNode].end()) {
      matching.push_back(
          std::make_tuple(leftNodes[leftNode], rightNodes[rightNode], false));
    } else {
      matching.push_back(
          std::make_tuple(leftNodes[leftNode], rightNodes[rightNode], true));
    }
  }
}

template <typename _Tp>
void BipartiteGraph<_Tp>::getMaximumMatching(
    std::unordered_map<_Tp, std::pair<_Tp, bool>> &matching) {
  std::vector<std::pair<std::size_t, std::size_t>> innerMatching;
  getMaximumMatching(innerMatching);

  matching.clear();
  for (auto &edge : innerMatching) {
    auto leftNode = edge.first;
    auto rightNode = edge.second;

    assert(matching.find(leftNodes[leftNode]) == matching.end() &&
           "Invalid matching");

    if (leftEdges[leftNode].find(rightNode) == leftEdges[leftNode].end())
      matching[leftNodes[leftNode]] = {rightNodes[rightNode], false};
    else
      matching[leftNodes[leftNode]] = {rightNodes[rightNode], true};
  }
}

template <typename _Tp>
bool BipartiteGraph<_Tp>::checkMatchingValid(
    const std::vector<std::tuple<_Tp, _Tp, bool>> &matching) {
  std::unordered_set<_Tp> tempLeftSet, tempRightSet;

  for (const auto &edge : matching) {
    if (tempLeftSet.find(std::get<0>(edge)) != tempLeftSet.end() ||
        tempRightSet.find(std::get<1>(edge)) != tempRightSet.end())
      return false;
    tempLeftSet.emplace(std::get<0>(edge));
    tempRightSet.emplace(std::get<1>(edge));
  }

  return true;
}

template <typename _Tp>
bool BipartiteGraph<_Tp>::checkMatchingValid(
    const std::unordered_map<_Tp, std::pair<_Tp, bool>> &matching) {
  std::unordered_set<_Tp> tempLeftSet, tempRightSet;

  for (const auto &edge : matching) {
    if (tempLeftSet.find(edge.first) != tempLeftSet.end() ||
        tempRightSet.find(edge.second.first) != tempRightSet.end())
      return false;
    tempLeftSet.emplace(edge.first);
    tempRightSet.emplace(edge.second.first);
  }

  return true;
}

template <typename _Tp>
void BipartiteGraph<_Tp>::enumMaximumMatchings(
    std::list<std::unordered_map<_Tp, std::pair<_Tp, bool>>> &matchings,
    std::size_t maxNum) {
  if (maxNum)
    maxMatchingNumber = maxNum;

  allMatchings.clear();
  matchings.clear();

  std::vector<std::pair<std::size_t, std::size_t>> initMatching;
  getMaximumMatching(initMatching);
  allMatchings.push_back(initMatching);

  std::list<std::pair<std::size_t, std::size_t>> addedEdges;

  enumMaximumMatchingIter(undiLeftEdges, undiRightEdges, initMatching,
                          addedEdges, true);

  for (const auto &innerMatching : allMatchings) {
    matchings.emplace_back();
    for (auto &edge : innerMatching) {
      auto leftNode = edge.first;
      auto rightNode = edge.second;

      assert(matchings.back().find(leftNodes[leftNode]) ==
                 matchings.back().end() &&
             "Invalid matching");

      if (leftEdges[leftNode].find(rightNode) == leftEdges[leftNode].end())
        matchings.back()[leftNodes[leftNode]] = {rightNodes[rightNode], false};
      else
        matchings.back()[leftNodes[leftNode]] = {rightNodes[rightNode], true};
    }
  }
}

template <typename _Tp>
void BipartiteGraph<_Tp>::enumMaximumMatchingIter(
    const std::vector<std::unordered_set<std::size_t>> &graphLeftEdges,
    const std::vector<std::unordered_set<std::size_t>> &graphRightEdges,
    const std::vector<std::pair<std::size_t, std::size_t>> &matching,
    const std::list<std::pair<std::size_t, std::size_t>> &addedEdges,
    bool hasCycles) {
  // [DEBUG]
  //   std::cout << "Matching Number: " << allMatchings.size() << std::endl;

  if (allMatchings.size() >= maxMatchingNumber)
    return;

  bool isEmptyGraph = true;
  for (const auto &successors : graphLeftEdges) {
    if (!successors.empty()) {
      isEmptyGraph = false;
      break;
    }
  }
  if (isEmptyGraph) {
    for (const auto &successors : graphRightEdges) {
      if (!successors.empty()) {
        isEmptyGraph = false;
        break;
      }
    }
  }
  if (isEmptyGraph)
    return;
  if (matching.empty())
    return;

  //
  // Get a directed graph D(G, M)

  std::vector<std::unordered_set<std::size_t>> diGraphLeftEdges(
      graphLeftEdges.size());
  std::vector<std::unordered_set<std::size_t>> diGraphRightEdges =
      graphRightEdges;
  for (const auto &matchEdge : matching) {
    diGraphLeftEdges[matchEdge.first].emplace(matchEdge.second);
    diGraphRightEdges[matchEdge.second].erase(matchEdge.first);
  }

  //
  // Find cycles via DFS

  std::list<std::list<std::pair<std::size_t, bool>>> foundCycles;
  if (hasCycles) {
    std::unordered_set<std::pair<std::size_t, bool>, VertexPairHash>
        dfsVisitedVertices;
    std::unordered_set<std::pair<std::size_t, bool>, VertexPairHash>
        dfsVisitingVertices;
    std::list<std::pair<std::size_t, bool>> dfsTraverseChain;
    std::function<void(std::pair<std::size_t, bool>)> findCyclesByDFS =
        [&](std::pair<std::size_t, bool> currVertex) {
          // There is a cycle
          if (dfsVisitingVertices.find(currVertex) !=
              dfsVisitingVertices.end()) {
            auto iter = dfsTraverseChain.cend();
            do {
              --iter;
            } while (*iter != currVertex);

            std::list<std::pair<std::size_t, bool>> newCycle;
            for (; iter != dfsTraverseChain.cend(); ++iter) {
              newCycle.push_back(*iter);
            }
            newCycle.push_back(currVertex);

            foundCycles.push_back(newCycle);
            return;
          }

          // The current vertex has been visited
          if (dfsVisitedVertices.find(currVertex) != dfsVisitedVertices.end())
            return;

          dfsVisitingVertices.emplace(currVertex);
          dfsTraverseChain.push_back(currVertex);

          // Traverse successors
          if (currVertex.second) {
            for (auto successor : diGraphLeftEdges[currVertex.first]) {
              findCyclesByDFS(std::make_pair(successor, false));
              if (!foundCycles.empty())
                goto END_DFS;
            }
          } else {
            for (auto successor : diGraphRightEdges[currVertex.first]) {
              findCyclesByDFS(std::make_pair(successor, true));
              if (!foundCycles.empty())
                goto END_DFS;
            }
          }

        END_DFS:
          dfsTraverseChain.pop_back();
          dfsVisitingVertices.erase(currVertex);
          dfsVisitedVertices.emplace(currVertex);
        };

    findCyclesByDFS(std::make_pair(matching.front().first, true));
  }

  // If there is a cycle
  if (!foundCycles.empty()) {
    std::list<std::pair<std::size_t, bool>> foundCycle =
        std::move(foundCycles.front());

    std::unordered_map<std::size_t, std::unordered_set<std::size_t>>
        cycleLeftEdges, cycleRightEdges;
    auto iter = foundCycle.cbegin();
    auto nextIter = ++foundCycle.cbegin();
    for (; nextIter != foundCycle.cend(); ++iter, ++nextIter) {
      if (iter->second) {
        cycleLeftEdges[iter->first].emplace(nextIter->first);
      } else {
        cycleRightEdges[iter->first].emplace(nextIter->first);
      }
    }

    //
    // Get a new matching through replacing some edges in current matching

    std::vector<std::pair<std::size_t, std::size_t>> newMatching;
    for (std::size_t leftVertex = 0; leftVertex < diGraphLeftEdges.size();
         ++leftVertex) {
      // If left vertex is not on the cycle
      if (cycleLeftEdges.find(leftVertex) == cycleLeftEdges.end() ||
          cycleLeftEdges.at(leftVertex).empty()) {
        for (auto rightVertex : diGraphLeftEdges[leftVertex]) {
          newMatching.emplace_back(leftVertex, rightVertex);
        }
      } else {
        for (auto rightVertex : diGraphLeftEdges[leftVertex]) {
          // If the edge (leftVertex, rightVertex) is not on the cycle
          if (cycleLeftEdges.at(leftVertex).find(rightVertex) ==
              cycleLeftEdges.at(leftVertex).end()) {
            newMatching.emplace_back(leftVertex, rightVertex);
          }
        }
      }
    }
    for (std::size_t rightVertex = 0; rightVertex < diGraphRightEdges.size();
         ++rightVertex) {
      // If the right vertex is on the cycle
      if (cycleRightEdges.find(rightVertex) != cycleRightEdges.end()) {
        for (auto leftVertex : diGraphRightEdges[rightVertex]) {
          // If the edge (rightVertex, leftVertex) is on the cycle
          if (cycleRightEdges.at(rightVertex).find(leftVertex) !=
              cycleRightEdges.at(rightVertex).end()) {
            newMatching.emplace_back(leftVertex, rightVertex);
          }
        }
      }
    }

    auto matchingMinus = newMatching;
    auto matchingPlus = matching;

    // Complete this new matching by adding some previously-deleted edges
    for (const auto &addedEdge : addedEdges) {
      newMatching.push_back(addedEdge);
    }

    // Add this new matching into match pool
    allMatchings.push_back(newMatching);

    //
    // Find an appropriate edge e

    std::pair<std::size_t, std::size_t> edgeE;
    for (const auto &matchEdge : matching) {
      if (cycleLeftEdges.find(matchEdge.first) != cycleLeftEdges.end() &&
          cycleLeftEdges.at(matchEdge.first).find(matchEdge.second) !=
              cycleLeftEdges.at(matchEdge.first).end()) {
        edgeE = matchEdge;
        break;
      }
    }

    //
    // Construct G+ and G-

    auto graphPlusLeftEdges = graphLeftEdges;
    auto graphPlusRightEdges = graphRightEdges;
    auto graphMinusLeftEdges = graphLeftEdges;
    auto graphMinusRightEdges = graphRightEdges;

    for (auto rightVertex : graphPlusLeftEdges[edgeE.first])
      graphPlusRightEdges[rightVertex].erase(edgeE.first);
    graphPlusLeftEdges[edgeE.first].clear();

    for (auto leftVertex : graphPlusRightEdges[edgeE.second])
      graphPlusLeftEdges[leftVertex].erase(edgeE.second);
    graphPlusRightEdges[edgeE.second].clear();

    graphMinusLeftEdges[edgeE.first].erase(edgeE.second);
    graphMinusRightEdges[edgeE.second].erase(edgeE.first);

    // Remove edge e from M+
    auto edgeEIter =
        std::find(matchingPlus.cbegin(), matchingPlus.cend(), edgeE);
    assert(edgeEIter != matchingPlus.cend());
    matchingPlus.erase(edgeEIter);

    auto addedEdgesPlus = addedEdges;
    auto addedEdgesMinus = addedEdges;
    addedEdgesPlus.push_back(edgeE);

    // Iteratively find matchings
    enumMaximumMatchingIter(graphPlusLeftEdges, graphPlusRightEdges,
                            matchingPlus, addedEdgesPlus, true);
    enumMaximumMatchingIter(graphMinusLeftEdges, graphMinusRightEdges,
                            matchingMinus, addedEdgesMinus, true);
  } else {
    // Find a length-2 feasible path

    std::unordered_map<std::size_t, std::unordered_set<std::size_t>>
        matchingLeftVertices, matchingRightVertices;
    for (const auto &matchEdge : matching) {
      matchingLeftVertices[matchEdge.first].emplace(matchEdge.second);
      matchingRightVertices[matchEdge.second].emplace(matchEdge.first);
    }

    std::vector<std::pair<std::size_t, bool>> length2Path;

    // Since all the left vertices with edges are covered, only right vertices
    // are traversed
    for (std::size_t rightVertex = 0; rightVertex < diGraphRightEdges.size();
         ++rightVertex) {
      if (!length2Path.empty())
        break;

      // The starting vertex is uncovered
      if (matchingRightVertices.find(rightVertex) ==
          matchingRightVertices.end()) {
        for (std::size_t leftVertex : diGraphRightEdges[rightVertex]) {
          if (!diGraphLeftEdges[leftVertex].empty()) {
            length2Path = {{rightVertex, false},
                           {leftVertex, true},
                           {*diGraphLeftEdges[leftVertex].cbegin(), false}};
            break;
          }
        }
      }
    }

    if (length2Path.empty())
      return;

    //
    // Construct a new matching through exchanging edges

    std::vector<std::pair<std::size_t, std::size_t>> newMatching;
    for (const auto &matchEdge : matching) {
      if (matchEdge.first == length2Path[1].first &&
          matchEdge.second == length2Path[2].first)
        continue;
      else
        newMatching.push_back(matchEdge);
    }
    newMatching.emplace_back(length2Path[1].first, length2Path[0].first);

    auto matchingMinus = matching;
    auto matchingPlus = newMatching;
    matchingPlus.pop_back();

    // Complete this new matching by adding some previously-deleted edges
    for (const auto &addedEdge : addedEdges) {
      newMatching.push_back(addedEdge);
    }

    // Add this new matching into match pool
    allMatchings.push_back(newMatching);

    //
    // Find an appropriate edge e

    std::pair<std::size_t, std::size_t> edgeE = {length2Path[1].first,
                                                 length2Path[0].first};

    //
    // Construct G+ and G-

    auto graphPlusLeftEdges = graphLeftEdges;
    auto graphPlusRightEdges = graphRightEdges;
    auto graphMinusLeftEdges = graphLeftEdges;
    auto graphMinusRightEdges = graphRightEdges;

    for (auto rightVertex : graphPlusLeftEdges[edgeE.first])
      graphPlusRightEdges[rightVertex].erase(edgeE.first);
    graphPlusLeftEdges[edgeE.first].clear();

    for (auto leftVertex : graphPlusRightEdges[edgeE.second])
      graphPlusLeftEdges[leftVertex].erase(edgeE.second);
    graphPlusRightEdges[edgeE.second].clear();

    graphMinusLeftEdges[edgeE.first].erase(edgeE.second);
    graphMinusRightEdges[edgeE.second].erase(edgeE.first);

    auto addedEdgesPlus = addedEdges;
    auto addedEdgesMinus = addedEdges;
    addedEdgesPlus.push_back(edgeE);

    enumMaximumMatchingIter(graphPlusLeftEdges, graphPlusRightEdges,
                            matchingPlus, addedEdgesPlus, false);
    enumMaximumMatchingIter(graphMinusLeftEdges, graphMinusRightEdges,
                            matchingMinus, addedEdgesMinus, false);
  }
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
template <class _Tp>
void UnionFindPairSet<_Tp>::addEdge(const _Tp &uVertex, const _Tp &vVertex) {
  vertexSet.emplace(uVertex);
  vertexSet.emplace(vVertex);

  auto foundUVertexIter = vVertexMap.find(uVertex);
  auto foundVVertexIter = uVertexMap.find(vVertex);

  if (foundUVertexIter != vVertexMap.end() &&
      foundVVertexIter != uVertexMap.end()) {
    auto pathIter = foundUVertexIter->second;
    auto removedPathIter = foundVVertexIter->second;
    // pathIter->splice(pathIter->end(), *removedPathIter);
    Utility::merge(*pathIter, *removedPathIter);

    vVertexMap.erase(foundUVertexIter);
    uVertexMap.erase(foundVVertexIter);
    vVertexMap[pathIter->back()] = pathIter;

    ufSet.erase(removedPathIter);
  } else if (foundUVertexIter != vVertexMap.end()) {
    auto pathIter = foundUVertexIter->second;
    pathIter->push_back(vVertex);

    vVertexMap.erase(foundUVertexIter);
    vVertexMap[vVertex] = pathIter;
  } else if (foundVVertexIter != uVertexMap.end()) {
    auto pathIter = foundVVertexIter->second;
    pathIter->push_front(uVertex);

    uVertexMap.erase(foundVVertexIter);
    uVertexMap[uVertex] = pathIter;
  } else {
    ufSet.push_back({uVertex, vVertex});
    auto pathIter = --ufSet.end();

    uVertexMap[uVertex] = pathIter;
    vVertexMap[vVertex] = pathIter;
  }
}

template <class _Tp>
void UnionFindPairSet<_Tp>::getResultUFSet(std::list<std::list<_Tp>> &result,
                                           std::unordered_set<_Tp> &vertices) {
  result = ufSet;
  vertices = vertexSet;
}

template <class _Tp>
DirectedAcyclicGraph<_Tp>::DirectedAcyclicGraph(std::size_t _size,
                                                const _Tp &_entry)
    : entryVertex(_entry), size(_size), nextVertexID(1), nextGraphID(0) {
  assert(_size);

  graph.resize(_size);
  vertexMap.resize(_size);
  vertices.resize(_size);

  vertexMap[0] = _entry;
  rVertexMap[_entry] = 0;
}

template <class _Tp>
void DirectedAcyclicGraph<_Tp>::addVertex(const _Tp &verNode) {
  if (rVertexMap.find(verNode) == rVertexMap.end())
    rVertexMap[verNode] = nextVertexID++;
}

template <class _Tp>
void DirectedAcyclicGraph<_Tp>::addEdge(const _Tp &parent, const _Tp &child) {
  // assert(child != entryVertex);

  if (rVertexMap.find(parent) == rVertexMap.end())
    rVertexMap[parent] = nextVertexID++;
  if (rVertexMap.find(child) == rVertexMap.end())
    rVertexMap[child] = nextVertexID++;

  VID parentVID = rVertexMap[parent], childVID = rVertexMap[child];

  assert(parentVID < size && childVID < size);

  vertexMap[parentVID] = parent;
  vertexMap[childVID] = child;

  graph[parentVID].emplace(childVID);
  graph[childVID];
}

template <class _Tp> bool DirectedAcyclicGraph<_Tp>::checkAcyclic() {
  assert(nextVertexID == size);
  assert(graph.size() == size);

  bool foundCycles = false;
  std::unordered_set<VID> dfsVisitingVertices, dfsVisitedVertices;
  std::function<void(VID)> findCyclesByDFS = [&](VID currVertex) {
    if (foundCycles)
      return;

    if (dfsVisitingVertices.find(currVertex) != dfsVisitingVertices.end()) {
      foundCycles = true;
      return;
    }

    if (dfsVisitedVertices.find(currVertex) != dfsVisitedVertices.end())
      return;

    dfsVisitingVertices.emplace(currVertex);
    for (const auto &successor : graph[currVertex]) {
      findCyclesByDFS(successor);
      if (foundCycles)
        goto END_DFS;
    }

  END_DFS:
    dfsVisitingVertices.erase(currVertex);
    dfsVisitedVertices.emplace(currVertex);
  };

  findCyclesByDFS(entry);

  return !foundCycles;
}

template <class _Tp> bool DirectedAcyclicGraph<_Tp>::checkConnected() {
  assert(nextVertexID == size);
  assert(graph.size() == size);

  unsigned inDegreeZeroCount = 0;
  std::vector<bool> inDegreeMap(size, false);
  for (VID uVertex = 0; uVertex < size; ++uVertex)
    for (VID vVertex : graph[uVertex])
      inDegreeMap[vVertex] = true;
  for (VID uVertex = 0; uVertex < size; ++uVertex)
    if (!inDegreeMap[uVertex])
      ++inDegreeZeroCount;

  if (inDegreeZeroCount > 1)
    return false;
  else
    return true;
}

template <class _Tp> bool DirectedAcyclicGraph<_Tp>::checkValidEntry() {
  assert(nextVertexID == size);
  assert(graph.size() == size);

  for (VID uVertex = 0; uVertex < size; ++uVertex)
    for (VID vVertex : graph[uVertex])
      if (vVertex == entry)
        return false;

  return true;
}

template <class _Tp> void DirectedAcyclicGraph<_Tp>::dumpGraphToDot() {
  auto &outStream = std::cout;

  std::string graphName = "Inner Graph";

  outStream << "digraph \"" << graphName << "\" {\n\tlabel=\"" << graphName
            << "\";\n"
            << std::endl;
  for (VID uVertex = 0; uVertex < size; ++uVertex) {
    outStream << "\t" << uVertex << " [shape=record,label=\"{"
              << vertexMap[uVertex] << " (" << uVertex << ")}\"];" << std::endl;
    for (VID vVertex : graph.at(uVertex))
      outStream << "\t" << uVertex << " -> " << vVertex << ";" << std::endl;
  }
  outStream << "}" << std::endl;
}

template <class _Tp> void DirectedAcyclicGraph<_Tp>::splitGraph() {
  assert(checkAcyclic() && "Not an acyclic graph");

  //
  // Get a new graph and reverse graph and reachable vertices
  {
    VID exitVertex = nextVertexID++;
    vertices.emplace_back(0);

    std::unordered_map<VID, std::unordered_set<VID>> oneGraph, reverseGraph;

    // Get the new complete graph and the reverse graph
    {
      std::unordered_set<VID> likelyExitVertices;
      for (VID uVertex = 0; uVertex < size; ++uVertex) {
        oneGraph[uVertex] = graph[uVertex];
        for (VID vVertex : graph[uVertex])
          reverseGraph[vVertex].emplace(uVertex);
        if (graph[uVertex].empty())
          likelyExitVertices.emplace(uVertex);
      }

      assert(!likelyExitVertices.empty());

      if (likelyExitVertices.size() == 1) {
        nextVertexID--;
        vertices.pop_back();
        exitVertex = *likelyExitVertices.cbegin();
      } else {
        for (VID originalExitVertex : likelyExitVertices)
          oneGraph[originalExitVertex].emplace(exitVertex);

        reverseGraph[exitVertex] = likelyExitVertices;
      }
      oneGraph[exitVertex] = {};
      reverseGraph[entry] = {};

      assert(oneGraph.size() == vertices.size());
      assert(reverseGraph.size() == vertices.size());
    }

    std::unordered_map<VID, std::unordered_set<VID>> reachGraph, rReachGraph;
    // Get the reachability graph and its reverse graph
    {
      std::unordered_set<VID> dfsVisitedVertices;
      std::function<void(VID)> getReachableVerticesByDFS = [&](VID currVertex) {
        if (dfsVisitedVertices.find(currVertex) != dfsVisitedVertices.end())
          return;

        std::unordered_set<VID> currVertexReachableVertices;
        for (const auto &successor : oneGraph.at(currVertex)) {
          currVertexReachableVertices.emplace(successor);

          // Get the reachable vertices of this successor
          getReachableVerticesByDFS(successor);

          const auto &successorReachableVertices = reachGraph[successor];
          // currVertexReachableVertices.insert(successorReachableVertices.cbegin(),
          // successorReachableVertices.cend());
          Utility::merge(currVertexReachableVertices,
                         successorReachableVertices);
        }
        reachGraph[currVertex] = currVertexReachableVertices;

        dfsVisitedVertices.emplace(currVertex);
      };

      getReachableVerticesByDFS(entry);

      for (const auto &graphPair : reachGraph) {
        VID uVertex = graphPair.first;
        for (VID vVertex : graphPair.second)
          rReachGraph[vVertex].emplace(uVertex);
      }
      rReachGraph[entry] = {};

      assert(reachGraph.size() == vertices.size());
      assert(rReachGraph.size() == vertices.size());
    }

    nextGraphID++;
    stGraphs.emplace_back(vertices.size(), entry, exitVertex, oneGraph,
                          reverseGraph, reachGraph, rReachGraph);
  }

  // Split graph recursively
  {
    GID currHandleGID = 0;

    // Handle s-t graph one by one until the minimal s-t graph
    while (currHandleGID < stGraphs.size()) {
      auto &oneSize = stGraphs[currHandleGID].size;
      VID sVertex = stGraphs[currHandleGID].sVertex;
      VID tVertex = stGraphs[currHandleGID].tVertex;
      auto &oneGraph = stGraphs[currHandleGID].graph;
      auto &reverseGraph = stGraphs[currHandleGID].reverseGraph;
      auto &reachGraph = stGraphs[currHandleGID].reachGraph;
      auto &reverseReachGraph = stGraphs[currHandleGID].reverseReachGraph;

      std::queue<VID> bfsQueue;
      bfsQueue.push(sVertex);
      std::unordered_set<VID> bfsVisitedSVertices;
      while (!bfsQueue.empty()) {
        VID likelySVertex = bfsQueue.front();
        bfsQueue.pop();

        if (bfsVisitedSVertices.find(likelySVertex) !=
            bfsVisitedSVertices.end())
          continue;
        if (oneGraph.find(likelySVertex) == oneGraph.end())
          continue;

        // Record this s-vertex and push its successors into queue
        bfsVisitedSVertices.emplace(likelySVertex);
        for (VID successor : oneGraph.at(likelySVertex))
          bfsQueue.push(successor);

        std::unordered_set<VID> likelyTVertices = reachGraph.at(likelySVertex);
        std::unordered_set<VID> usedTVertices;

        // Find a single list starting from s-vertex and remove from the handled
        // list
        for (VID successor : oneGraph.at(likelySVertex)) {
          VID startVertex = successor;
          // VID lastVertex = startVertex;
          std::list<VID> singleList;
          while (startVertex != tVertex) {
            assert(!oneGraph.at(startVertex).empty());

            // lastVertex = startVertex;
            if (oneGraph.at(startVertex).size() > 1)
              break;
            else if (reverseGraph.at(startVertex).size() > 1)
              break;
            else {
              singleList.push_back(startVertex);
              startVertex = *oneGraph.at(startVertex).cbegin();
            }
          }

          // There is a single list
          if (!singleList.empty()) {
            for (VID singleListVertex : singleList) {
              bfsVisitedSVertices.emplace(singleListVertex);
              usedTVertices.emplace(singleListVertex);

              // Add successors to BFS queue
              for (VID successor : oneGraph.at(singleListVertex))
                bfsQueue.push(successor);
            }
          }
        }

        // Find a feasible t-vertex to construct a s-t subgraph
        while (!likelyTVertices.empty()) {
          if (oneGraph.find(likelySVertex) == oneGraph.end())
            break;

          for (VID tempTVertex : likelyTVertices)
            if (reachGraph.at(likelySVertex).find(tempTVertex) ==
                reachGraph.at(likelySVertex).end())
              usedTVertices.emplace(tempTVertex);

          Utility::difference(likelyTVertices, usedTVertices);

          // If there is not feasible t-vertex to select
          if (likelyTVertices.empty())
            break;

          VID likelyTVertex = *likelyTVertices.cbegin();
          usedTVertices.emplace(likelyTVertex);

          // Skip if the s-vertex is entry and t-vertex is exit
          if (likelySVertex == sVertex && likelyTVertex == tVertex)
            continue;

          std::unordered_set<VID> interset;
          Utility::intersection(interset, reachGraph[likelySVertex],
                                reverseReachGraph[likelyTVertex]);

          // If the s-vertex and t-vertex have no intermediate vertices
          if (interset.empty())
            continue;

          bool isValidInterset = true;
          for (VID innerVertex : interset) {
            if (!isValidInterset)
              break;
            for (VID successor : oneGraph.at(innerVertex)) {
              if (!isValidInterset)
                break;
              else if (successor == likelyTVertex)
                continue;
              else if (interset.find(successor) == interset.end())
                isValidInterset = false;
            }
            for (VID predecessor : reverseGraph.at(innerVertex)) {
              if (!isValidInterset)
                break;
              else if (predecessor == likelySVertex)
                continue;
              else if (interset.find(predecessor) == interset.end())
                isValidInterset = false;
            }
          }
          if (!isValidInterset)
            continue;

          {
            //
            // Check whether the s-vertex has other successors not in the
            // interset and the t-vertex has other predecessors not in the
            // interset

            bool hasOtherSuccessors = false, hasOtherPredecessors = false;
            for (VID successor : oneGraph.at(likelySVertex)) {
              if (successor == likelyTVertex)
                continue;
              else if (interset.find(successor) == interset.end()) {
                hasOtherSuccessors = true;
                break;
              }
            }
            for (VID predecessor : reverseGraph.at(likelyTVertex)) {
              if (predecessor == likelySVertex)
                continue;
              else if (interset.find(predecessor) == interset.end()) {
                hasOtherPredecessors = true;
                break;
              }
            }
            if (likelySVertex == sVertex)
              hasOtherSuccessors = true;
            if (likelyTVertex == tVertex)
              hasOtherPredecessors = true;

            // Check whether the interset only has one vertex
            if (interset.size() == 1 && hasOtherSuccessors &&
                hasOtherPredecessors)
              continue;

            GID subGraphID = nextGraphID++;
            VID subGraphReplaceVertex = nextVertexID++;
            vertices.emplace_back(subGraphID);

            //
            // Construct the subgraph

            std::size_t newSize = interset.size() + 2;
            VID newSVertex = likelySVertex;
            VID newTVertex = likelyTVertex;
            std::unordered_map<VID, std::unordered_set<VID>> newOneGraph,
                newReverseGraph, newReachGraph, newReReachGraph;

            // Add some virtual vertices
            if (hasOtherSuccessors) {
              newSVertex = nextVertexID++;
              vertices.emplace_back(0);
            }
            if (hasOtherPredecessors) {
              newTVertex = nextVertexID++;
              vertices.emplace_back(0);
            }

            // Construct the graph and reverse graph
            for (VID successor : oneGraph.at(likelySVertex)) {
              if (successor == likelyTVertex)
                newOneGraph[newSVertex].emplace(newTVertex);
              else if (interset.find(successor) != interset.end())
                newOneGraph[newSVertex].emplace(successor);
            }
            for (VID predecessor : reverseGraph.at(likelyTVertex)) {
              if (predecessor == likelySVertex)
                newReverseGraph[newTVertex].emplace(newSVertex);
              else if (interset.find(predecessor) != interset.end())
                newReverseGraph[newTVertex].emplace(predecessor);
            }
            for (VID innerVertex : interset) {
              newOneGraph[innerVertex] = oneGraph.at(innerVertex);
              newReverseGraph[innerVertex] = reverseGraph.at(innerVertex);

              if (newOneGraph[innerVertex].erase(likelyTVertex))
                newOneGraph[innerVertex].emplace(newTVertex);
              if (newReverseGraph[innerVertex].erase(likelySVertex))
                newReverseGraph[innerVertex].emplace(newSVertex);
            }
            newOneGraph[newTVertex] = {};
            newReverseGraph[newSVertex] = {};
            assert(newOneGraph.size() == newSize);
            assert(newReverseGraph.size() == newSize);

            // Construct the reachability graph and its reverse graph
            {
              newReachGraph[newSVertex] = {newTVertex};
              Utility::merge(newReachGraph[newSVertex], interset);

              newReReachGraph[newTVertex] = {newSVertex};
              Utility::merge(newReReachGraph[newTVertex], interset);
            }
            for (VID innerVertex : interset) {
              Utility::difference(newReachGraph[innerVertex],
                                  reachGraph.at(innerVertex),
                                  reachGraph.at(likelyTVertex));
              Utility::difference(newReReachGraph[innerVertex],
                                  reverseReachGraph.at(innerVertex),
                                  reverseReachGraph.at(likelySVertex));

              if (newReachGraph[innerVertex].erase(likelyTVertex))
                newReachGraph[innerVertex].emplace(newTVertex);
              if (newReReachGraph[innerVertex].erase(likelySVertex))
                newReReachGraph[innerVertex].emplace(newSVertex);
            }
            newReachGraph[newTVertex] = {};
            newReReachGraph[newSVertex] = {};
            assert(newReachGraph.size() == newSize);
            assert(newReReachGraph.size() == newSize);

            // Add to graph list
            stGraphs.emplace_back(newSize, newSVertex, newTVertex, newOneGraph,
                                  newReverseGraph, newReachGraph,
                                  newReReachGraph);

            //
            // Change the current graphs and reachability graphs

            // Remove some vertices in reachability graphs and add the
            // replacement
            for (VID grandPredecessor : reverseReachGraph.at(likelySVertex)) {
              Utility::difference(reachGraph[grandPredecessor], interset);
              reachGraph[grandPredecessor].emplace(subGraphReplaceVertex);
              if (!hasOtherSuccessors)
                reachGraph[grandPredecessor].erase(likelySVertex);
              if (!hasOtherPredecessors)
                reachGraph[grandPredecessor].erase(likelyTVertex);
            }
            for (VID grandSuccessor : reachGraph.at(likelyTVertex)) {
              Utility::difference(reverseReachGraph[grandSuccessor], interset);
              reverseReachGraph[grandSuccessor].emplace(subGraphReplaceVertex);
              if (!hasOtherPredecessors)
                reverseReachGraph[grandSuccessor].erase(likelyTVertex);
              if (!hasOtherSuccessors)
                reverseReachGraph[grandSuccessor].erase(likelySVertex);
            }
            for (VID innerVertex : interset) {
              reachGraph.erase(innerVertex);
              reverseReachGraph.erase(innerVertex);
            }

            // Handle s-vertex and t-vertex
            reachGraph[subGraphReplaceVertex] = reachGraph.at(likelyTVertex);
            reverseReachGraph[subGraphReplaceVertex] =
                reverseReachGraph.at(likelySVertex);
            if (hasOtherPredecessors && hasOtherSuccessors) {
              Utility::difference(reachGraph[likelySVertex], interset);
              Utility::difference(reverseReachGraph[likelyTVertex], interset);

              reachGraph[likelySVertex].emplace(subGraphReplaceVertex);
              reverseReachGraph[likelyTVertex].emplace(subGraphReplaceVertex);

              reachGraph[subGraphReplaceVertex].emplace(likelyTVertex);
              reverseReachGraph[subGraphReplaceVertex].emplace(likelySVertex);
            } else if (hasOtherSuccessors) {
              Utility::difference(reachGraph[likelySVertex], interset);
              reachGraph[likelySVertex].erase(likelyTVertex);
              reachGraph[likelySVertex].emplace(subGraphReplaceVertex);

              reverseReachGraph[subGraphReplaceVertex].emplace(likelySVertex);

              reachGraph.erase(likelyTVertex);
              reverseReachGraph.erase(likelyTVertex);
            } else if (hasOtherPredecessors) {
              Utility::difference(reverseReachGraph[likelyTVertex], interset);
              reverseReachGraph[likelyTVertex].erase(likelySVertex);
              reverseReachGraph[likelyTVertex].emplace(subGraphReplaceVertex);

              reachGraph[subGraphReplaceVertex].emplace(likelyTVertex);

              reachGraph.erase(likelySVertex);
              reverseReachGraph.erase(likelySVertex);
            } else {
              reachGraph.erase(likelySVertex);
              reverseReachGraph.erase(likelySVertex);
              reachGraph.erase(likelyTVertex);
              reverseReachGraph.erase(likelyTVertex);
            }

            // Rmove some vertices in graphs
            oneSize = oneSize - interset.size() + 1;
            std::unordered_set<VID> sPredecessors =
                reverseGraph.at(likelySVertex);
            std::unordered_set<VID> tSuccessors = oneGraph.at(likelyTVertex);
            for (VID innerVertex : interset) {
              oneGraph.erase(innerVertex);
              reverseGraph.erase(innerVertex);
            }
            if (hasOtherSuccessors && hasOtherPredecessors) {
              Utility::difference(oneGraph[likelySVertex], interset);
              Utility::difference(reverseGraph[likelyTVertex], interset);

              oneGraph[likelySVertex].emplace(subGraphReplaceVertex);
              reverseGraph[likelyTVertex].emplace(subGraphReplaceVertex);

              oneGraph[subGraphReplaceVertex].emplace(likelyTVertex);
              reverseGraph[subGraphReplaceVertex].emplace(likelySVertex);
            } else if (hasOtherSuccessors) {
              Utility::difference(oneGraph[likelySVertex], interset);
              oneGraph[likelySVertex].erase(likelyTVertex);
              oneGraph[likelySVertex].emplace(subGraphReplaceVertex);
              reverseGraph[subGraphReplaceVertex].emplace(likelySVertex);

              for (VID successor : tSuccessors) {
                oneGraph[subGraphReplaceVertex].emplace(successor);
                reverseGraph[successor].erase(likelyTVertex);
                reverseGraph[successor].emplace(subGraphReplaceVertex);
              }

              oneGraph.erase(likelyTVertex);
              reverseGraph.erase(likelyTVertex);

              oneSize--;
            } else if (hasOtherPredecessors) {
              Utility::difference(reverseGraph[likelyTVertex], interset);
              reverseGraph[likelyTVertex].erase(likelySVertex);
              reverseGraph[likelyTVertex].emplace(subGraphReplaceVertex);
              oneGraph[subGraphReplaceVertex].emplace(likelyTVertex);

              for (VID predecessor : sPredecessors) {
                reverseGraph[subGraphReplaceVertex].emplace(predecessor);
                oneGraph[predecessor].erase(likelySVertex);
                oneGraph[predecessor].emplace(subGraphReplaceVertex);
              }

              oneGraph.erase(likelySVertex);
              reverseGraph.erase(likelySVertex);

              oneSize--;
            } else {
              for (VID predecessor : sPredecessors) {
                reverseGraph[subGraphReplaceVertex].emplace(predecessor);
                oneGraph[predecessor].erase(likelySVertex);
                oneGraph[predecessor].emplace(subGraphReplaceVertex);
              }
              for (VID successor : tSuccessors) {
                oneGraph[subGraphReplaceVertex].emplace(successor);
                reverseGraph[successor].erase(likelyTVertex);
                reverseGraph[successor].emplace(subGraphReplaceVertex);
              }

              oneGraph.erase(likelySVertex);
              oneGraph.erase(likelyTVertex);
              reverseGraph.erase(likelySVertex);
              reverseGraph.erase(likelyTVertex);

              oneSize -= 2;
            }

            assert(oneGraph.size() == oneSize);
            assert(reverseGraph.size() == oneSize);
            assert(reachGraph.size() == oneSize);
            assert(reverseReachGraph.size() == oneSize);

            // Add the new replacement vertex
            bfsQueue.push(subGraphReplaceVertex);
          }
        }

        if (oneGraph.find(likelySVertex) != oneGraph.end())
          for (VID successor : oneGraph.at(likelySVertex))
            bfsQueue.push(successor);
      }

      // Check and change properties of the vertices of this s-t graph
      for (const auto &graphPair : oneGraph) {
        vertices[graphPair.first].inFoldedGraphID = currHandleGID;
        GID callFoldedGraphID = vertices[graphPair.first].callFoldedGraphID;
        if (callFoldedGraphID) {
          stGraphCalls[currHandleGID].emplace(
              vertices[graphPair.first].callFoldedGraphID);
          stGraphCalls[callFoldedGraphID];
        }

        // Record unvisited vertex
        if (vertices[graphPair.first].isOriginal)
          stGraphs[currHandleGID].unvisitedVertices.emplace(graphPair.first);
      }

      // Complete the s-t graph call graph
      stGraphCalls[currHandleGID];

      ++currHandleGID;
    }
  }
}

template <class _Tp>
bool DirectedAcyclicGraph<_Tp>::verifySTGraphs(std::string &error) {
  if (entry != stGraphs[0].sVertex) {
    error = "Wrong entry";
    return false;
  }

  std::unordered_set<VID> tempVertices;
  for (GID handleGID = 0; handleGID < stGraphs.size(); ++handleGID) {
    std::size_t oneSize = stGraphs[handleGID].size;
    VID sVertex = stGraphs[handleGID].sVertex;
    VID tVertex = stGraphs[handleGID].tVertex;
    const auto &oneGraph = stGraphs[handleGID].graph;
    const auto &reverseGraph = stGraphs[handleGID].reverseGraph;
    const auto &reachGraph = stGraphs[handleGID].reachGraph;
    const auto &reverseReachGraph = stGraphs[handleGID].reverseReachGraph;

    if (!reverseGraph.at(sVertex).empty()) {
      error = "Wrong s-vertex";
      return false;
    }
    if (!oneGraph.at(tVertex).empty()) {
      error = "Wrong t-vertex";
      return false;
    }
    if (oneSize != oneGraph.size() || oneSize != reverseGraph.size() ||
        oneSize != reachGraph.size() || oneSize != reverseReachGraph.size()) {
      error = "Wrong size";
      return false;
    }

    for (const auto &graphPair : oneGraph) {
      if (vertices[graphPair.first].isOriginal)
        tempVertices.emplace(graphPair.first);
      for (const auto &vVertex : graphPair.second)
        if (vertices[vVertex].isOriginal)
          tempVertices.emplace(vVertex);
    }

    std::unordered_map<VID, std::unordered_set<VID>> tempReverseGraph,
        tempReachGraph, tempReReachGraph;
    for (const auto &graphPair : oneGraph)
      for (const auto &vVertex : graphPair.second)
        tempReverseGraph[vVertex].emplace(graphPair.first);
    tempReverseGraph[sVertex] = {};
    for (const auto &graphPair : reachGraph)
      for (const auto &vVertex : graphPair.second)
        tempReReachGraph[vVertex].emplace(graphPair.first);
    tempReReachGraph[sVertex] = {};

    {
      std::unordered_set<VID> dfsVisitedVertices;
      std::function<void(VID)> getReachableVerticesByDFS = [&](VID currVertex) {
        if (dfsVisitedVertices.find(currVertex) != dfsVisitedVertices.end())
          return;

        std::unordered_set<VID> currVertexReachableVertices;
        for (const auto &successor : oneGraph.at(currVertex)) {
          currVertexReachableVertices.emplace(successor);

          // Get the reachable vertices of this successor
          getReachableVerticesByDFS(successor);

          const auto &successorReachableVertices = tempReachGraph[successor];
          // currVertexReachableVertices.insert(successorReachableVertices.cbegin(),
          // successorReachableVertices.cend());
          Utility::merge(currVertexReachableVertices,
                         successorReachableVertices);
        }
        tempReachGraph[currVertex] = currVertexReachableVertices;

        dfsVisitedVertices.emplace(currVertex);
      };

      getReachableVerticesByDFS(sVertex);
    }

    if (!Utility::equal(reverseGraph, tempReverseGraph)) {
      error = "Unequal reverse graph";
      return false;
    }
    if (!Utility::equal(reverseReachGraph, tempReReachGraph)) {
      error = "Unequal reverse reach graph";
      return false;
    }
    if (!Utility::equal(reachGraph, tempReachGraph)) {
      error = "Unequal reach graph";
      return false;
    }
  }

  if (tempVertices.size() != size) {
    error = "Wrong combined graph";
    return false;
  }

  return true;
}

template <class _Tp>
void DirectedAcyclicGraph<_Tp>::genMinimumPathCovers(
    std::size_t maxMatchingNum) {
  // // [DEBUG]
  // dumpGraphToDot();

  // Split the graph into s-t graphs firstly
  splitGraph();

  for (GID currGraphID = 0; currGraphID < stGraphs.size(); ++currGraphID) {
    std::size_t oneSize = stGraphs[currGraphID].size;
    VID sVertex = stGraphs[currGraphID].sVertex;
    VID tVertex = stGraphs[currGraphID].tVertex;
    const auto &oneGraph = stGraphs[currGraphID].graph;
    const auto &reverseGraph = stGraphs[currGraphID].reverseGraph;
    const auto &reachGraph = stGraphs[currGraphID].reachGraph;
    auto &pathCovers = stGraphs[currGraphID].pathCovers;
    auto &combinedPaths = stGraphs[currGraphID].combinedPaths;

    // Clear path covers
    pathCovers.clear();
    combinedPaths.clear();

    //
    // Get all possible matchings

    BipartiteGraph<VID> biGraph(oneSize, oneSize);
    for (const auto &graphPair : reachGraph)
      for (VID vVertex : graphPair.second)
        biGraph.addEdge(graphPair.first, vVertex);

    std::list<std::unordered_map<VID, std::pair<VID, bool>>> allMatchings;
    biGraph.enumMaximumMatchings(allMatchings, maxMatchingNum);

    //
    // Iterate each matching

    std::unordered_set<std::size_t> MPCHashValues;
    for (const auto &matching : allMatchings) {
      UnionFindPairSet<VID> ufSet;
      for (const auto &matchEdge : matching)
        ufSet.addEdge(matchEdge.first, matchEdge.second.first);

      std::list<std::list<VID>> resultMPC;
      std::unordered_set<VID> matchingVertexSet;
      ufSet.getResultUFSet(resultMPC, matchingVertexSet);

      // Add single vertex to result Empc
      for (const auto &graphPair : oneGraph)
        if (matchingVertexSet.find(graphPair.first) == matchingVertexSet.end())
          resultMPC.push_back({graphPair.first});

      //
      // Remove some redundant vertices in this result Empc

      std::vector<std::list<VID>> minMPC;
      for (const auto &path : resultMPC) {
        /// @brief Check whether the two vertices have a direct link path
        auto checkDirectLink =
            [](const std::unordered_map<VID, std::unordered_set<VID>> &_graph,
               VID _uVertex, VID _vVertex) -> bool {
          VID startVertex = _uVertex;
          while (startVertex != _vVertex) {
            std::size_t successorNum = _graph.at(startVertex).size();
            if (successorNum == 1) {
              startVertex = *_graph.at(startVertex).cbegin();
              continue;
            } else {
              return false;
            }
          }
          return true;
        };

        if (path.empty())
          continue;

        if (path.size() == 1) {
          if (path.front() != sVertex && path.front() != tVertex)
            minMPC.push_back(path);
          continue;
        }

        auto lastVertexIter = path.cbegin();
        auto currVertexIter = ++path.cbegin();
        std::list<VID> newLeftPath;
        while (currVertexIter != path.cend()) {
          VID lastVertex = *lastVertexIter;
          VID currVertex = *currVertexIter;

          if (!checkDirectLink(reverseGraph, currVertex, lastVertex))
            newLeftPath.push_back(lastVertex);

          lastVertexIter = currVertexIter;
          ++currVertexIter;
        }
        newLeftPath.push_back(path.back());

        auto nextVertexIter = --newLeftPath.cend();
        currVertexIter = nextVertexIter;
        std::list<VID> newRightPath;
        if (currVertexIter != newLeftPath.cbegin()) {
          --currVertexIter;
          while (true) {
            if (!checkDirectLink(oneGraph, *currVertexIter, *nextVertexIter))
              newRightPath.push_front(*nextVertexIter);

            nextVertexIter = currVertexIter;
            if (currVertexIter == newLeftPath.cbegin())
              break;
            else
              --currVertexIter;
          }
        }
        newRightPath.push_front(newLeftPath.front());

        minMPC.push_back(newRightPath);
      }

      //
      // Calculate the hash value of this Empc

      auto pathListComparator = [](const std::list<VID> &path1,
                                   const std::list<VID> &path2) -> bool {
        if (path1.size() < path2.size())
          return true;
        else if (path1.size() > path2.size())
          return false;
        else {
          for (std::list<VID>::const_iterator iter1 = path1.cbegin(),
                                              iter2 = path2.cbegin();
               iter1 != path1.cend() && iter2 != path2.cend();
               ++iter1, ++iter2) {
            if (*iter1 < *iter2)
              return true;
            else if (*iter1 > *iter2)
              return false;
          }
          return false;
        }
      };
      auto hashOnePath = [](const std::list<VID> &path) -> std::size_t {
        std::size_t result = 0;
        for (VID vertex : path)
          result ^= std::hash<VID>{}(vertex) + 0x9e3779b9 + (result << 6) +
                    (result >> 2);
        return result;
      };
      auto hashOneMPC =
          [&pathListComparator, &hashOnePath](
              const std::vector<std::list<VID>> &oneMPC) -> std::size_t {
        auto tempMPC = oneMPC;
        std::sort(tempMPC.begin(), tempMPC.end(), pathListComparator);

        std::size_t result = 0;
        for (const auto &path : tempMPC)
          result ^=
              hashOnePath(path) + 0x9e3779b9 + (result << 6) + (result >> 2);

        return result;
      };

      //
      // Add a non-duplicate Empc to path cover set

      std::size_t hashValue = hashOneMPC(minMPC);
      if (MPCHashValues.find(hashValue) == MPCHashValues.end()) {
        CID currCoverID = pathCovers.size();
        for (const auto &path : minMPC) {
          std::size_t pathHash = hashOnePath(path);
          if (combinedPaths.find(pathHash) == combinedPaths.end())
            combinedPaths[pathHash] =
                std::make_pair(path, std::unordered_set<CID>{currCoverID});
          else
            combinedPaths[pathHash].second.emplace(currCoverID);
        }

        MPCHashValues.emplace(hashValue);
        pathCovers.emplace_back(minMPC.begin(), minMPC.end());
      }
    }
  }
}

template <class _Tp>
bool DirectedAcyclicGraph<_Tp>::queryStepVertex(
    VID currVertex, VID nextVertex, const PathReservedInfo &reservedInfo) {
  // assert(tempReservedStackInfo.empty() && "Wrong status when querying to step
  // vertex");

  if (nextVertex == entry) {
    TempReservedStackInfo singleReservedInfo(StackChangeType::INIT);

    singleReservedInfo.changedFrameGraph.emplace_back(0, 0);
    singleReservedInfo.changedFramePaths.emplace_back();

    for (const auto &pathHashPair : stGraphs[0].combinedPaths)
      singleReservedInfo.changedFramePaths.back().emplace(pathHashPair.first);

    singleReservedInfo.success = true;
    tempReservedStackInfo[0] = singleReservedInfo;
    return true;
  } else {
    assert(graph.at(currVertex).find(nextVertex) != graph.at(currVertex).end());
    assert(reservedInfo.currVertex == currVertex);
    assert(!reservedInfo.graphCallStack.empty());
    assert(!reservedInfo.pathsStack.empty());

    GID currGraphID = reservedInfo.graphCallStack.top().first;
    auto currLikelyPaths = reservedInfo.pathsStack.top();

    GID currVertexInGraph = vertices[currVertex].inFoldedGraphID;
    GID nextVertexInGraph = vertices[nextVertex].inFoldedGraphID;
    assert(currGraphID == currVertexInGraph);

    bool nextVertexIsVisited =
        stGraphs[nextVertexInGraph].unvisitedVertices.find(nextVertex) ==
        stGraphs[nextVertexInGraph].unvisitedVertices.end();

    //
    // Find a call chain between the two s-t graphs

    bool isInSameGraph = false, isCallGraph = false, isCalleeGraph = false;
    std::list<VID> curr_nextCallChain, next_currCallChain;
    GID intermeGraphID;
    if (currVertexInGraph == nextVertexInGraph)
      isInSameGraph = true;
    else {
      std::list<VID> neighborCallChain;
      GID endDFSGraphID;
      std::function<void(GID)> findCallChainByDFS = [&](GID tempVertexInGraph) {
        if (tempVertexInGraph == endDFSGraphID) {
          neighborCallChain.push_front(tempVertexInGraph);
          return;
        }

        for (GID succVertexInGraph : this->stGraphCalls.at(tempVertexInGraph)) {
          findCallChainByDFS(succVertexInGraph);
          if (!neighborCallChain.empty()) {
            neighborCallChain.push_front(tempVertexInGraph);
            return;
          }
        }
      };

      neighborCallChain.clear();
      endDFSGraphID = nextVertexInGraph;
      findCallChainByDFS(currVertexInGraph);
      if (!neighborCallChain.empty()) {
        curr_nextCallChain = neighborCallChain;
        isCallGraph = true;
      } else {
        neighborCallChain.clear();
        endDFSGraphID = currVertexInGraph;
        findCallChainByDFS(nextVertexInGraph);

        if (!neighborCallChain.empty()) {
          next_currCallChain = neighborCallChain;
          isCalleeGraph = true;
        } else {
          // pop and then push
          auto tempGraphStack = reservedInfo.graphCallStack;
          tempGraphStack.pop();
          while (!tempGraphStack.empty()) {
            GID tempGraphID = tempGraphStack.top().first;
            tempGraphStack.pop();

            neighborCallChain.clear();
            endDFSGraphID = currVertexInGraph;
            findCallChainByDFS(tempGraphID);
            if (neighborCallChain.empty())
              continue;
            else
              curr_nextCallChain = neighborCallChain;

            neighborCallChain.clear();
            endDFSGraphID = nextVertexInGraph;
            findCallChainByDFS(tempGraphID);
            if (neighborCallChain.empty())
              continue;
            else
              next_currCallChain = neighborCallChain;

            intermeGraphID = tempGraphID;
            break;
          }

          assert(!curr_nextCallChain.empty() && !next_currCallChain.empty());
        }
      }
    }

    auto hasSelectedCovers =
        [&](GID graphID, const std::unordered_set<CID> &coversSet) -> bool {
      for (CID coverID : coversSet)
        if (this->stGraphs[graphID].pathCovers[coverID].selected)
          return true;
      return false;
    };
    auto isUncoveredPath = [&](GID graphID, std::size_t pathHash) -> bool {
      const auto &tempSTGraph = this->stGraphs[graphID];
      // if (!tempSTGraph.allCalleeVisited)
      //   return true;
      // else
      if (tempSTGraph.coveredPaths.find(pathHash) ==
          tempSTGraph.coveredPaths.end())
        return true;
      else
        return false;
    };
    auto findVertexCallingGraph = [&](GID caller, GID callee) -> VID {
      for (const auto &graphPair : this->stGraphs[caller].graph)
        if (this->vertices[graphPair.first].callFoldedGraphID == callee)
          return graphPair.first;
      return VID_MAX;
    };
    auto isVertexOnPath = [&](GID graphID, const std::list<VID> &path,
                              VID addiVertex) -> bool {
      const auto &oneGraph = this->stGraphs[graphID].graph;
      const auto &reachGraph = this->stGraphs[graphID].reachGraph;

      if (oneGraph.find(addiVertex) == oneGraph.cend())
        return false;

      auto iter = path.cbegin();
      for (; iter != path.cend(); ++iter) {
        if (*iter == addiVertex)
          return true;
        else if (reachGraph.at(addiVertex).find(*iter) !=
                 reachGraph.at(addiVertex).cend())
          return true;
      }

      return reachGraph.at(path.back()).find(addiVertex) !=
             reachGraph.at(path.back()).cend();
    };
    // auto isVerticesOnPath = [&](GID graphID, const std::list<VID> &path, VID
    // uVertex, VID vVertex) -> bool
    // {
    //     const auto &oneGraph = this->stGraphs[graphID].graph;
    //     const auto &reachGraph = this->stGraphs[graphID].reachGraph;

    //     if (uVertex == vVertex)
    //         return false;
    //     if (oneGraph.find(uVertex) == oneGraph.cend() ||
    //     oneGraph.find(vVertex) == oneGraph.cend())
    //         return false;
    //     if (reachGraph.at(uVertex).find(vVertex) ==
    //     oneGraph.at(uVertex).cend())
    //         return false;

    //     return isVertexOnPath(uVertex) && isVertexOnPath(vVertex);
    // };

    TempReservedStackInfo singleReservedInfo;

    if (isInSameGraph) {
      singleReservedInfo.type = StackChangeType::SAME_GRAPH;
      singleReservedInfo.success = true;

      const auto &frameCombinedPaths = stGraphs[currGraphID].combinedPaths;
      bool allVerticesVisited = stGraphs[currGraphID].unvisitedVertices.empty();
      std::unordered_set<std::size_t> removedPaths;
      for (std::size_t pathHash : currLikelyPaths) {
        // Check whether the path cover of this path is selected
        if (!hasSelectedCovers(currGraphID,
                               frameCombinedPaths.at(pathHash).second)) {
          removedPaths.emplace(pathHash);
          continue;
        }

        const auto &path = frameCombinedPaths.at(pathHash).first;

        // Check whether the vertex is on this path
        if (!isVertexOnPath(currGraphID, path, nextVertex)) {
          removedPaths.emplace(pathHash);
          continue;
        }

        // Check whether this path is uncovered
        if (!isUncoveredPath(currGraphID, pathHash)) {
          removedPaths.emplace(pathHash);
          stGraphs[currGraphID].temporaryCoveredPaths.emplace(pathHash);
          continue;
        }
      }
      Utility::difference(currLikelyPaths, removedPaths);

      // If there is no feasible path to select
      if (currLikelyPaths.empty() && !allVerticesVisited)
        singleReservedInfo.success = false;

      singleReservedInfo.changedFramePaths.push_back(currLikelyPaths);

      goto RESULT_CHECK;
    } else if (isCallGraph) {
      singleReservedInfo.type = StackChangeType::PUSH_GRAPH;
      singleReservedInfo.success = true;

      auto currIter = curr_nextCallChain.cbegin();
      auto nextIter = ++curr_nextCallChain.cbegin();
      VID nextGraphCallerVertex = 0;
      while (currIter != curr_nextCallChain.cend()) {
        GID frameCurrGraphID = *currIter;
        const auto &frameCombinedPaths =
            stGraphs[frameCurrGraphID].combinedPaths;
        bool allVerticesVisited =
            stGraphs[frameCurrGraphID].unvisitedVertices.empty();

        VID testVertex = 0;
        if (nextIter == curr_nextCallChain.cend()) {
          testVertex = nextVertex;
        } else {
          GID frameNextGraphID = *nextIter;

          VID currGraphCallerVertex =
              findVertexCallingGraph(frameCurrGraphID, frameNextGraphID);
          assert(currGraphCallerVertex != VID_MAX);

          testVertex = currGraphCallerVertex;
        }

        std::unordered_set<std::size_t> frameLikelyPaths;
        if (currIter == curr_nextCallChain.cbegin())
          frameLikelyPaths = currLikelyPaths;
        else {
          for (const auto &mapPair : frameCombinedPaths)
            frameLikelyPaths.emplace(mapPair.first);
        }

        std::unordered_set<std::size_t> removedPaths;
        for (std::size_t pathHash : frameLikelyPaths) {
          if (!hasSelectedCovers(frameCurrGraphID,
                                 frameCombinedPaths.at(pathHash).second)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          const auto &path = frameCombinedPaths.at(pathHash).first;

          if (!isVertexOnPath(frameCurrGraphID, path, testVertex)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          // Check whether this path is uncovered
          if (!isUncoveredPath(frameCurrGraphID, pathHash)) {
            removedPaths.emplace(pathHash);
            stGraphs[frameCurrGraphID].temporaryCoveredPaths.emplace(pathHash);
            continue;
          }
        }
        Utility::difference(frameLikelyPaths, removedPaths);

        if (frameLikelyPaths.empty() && !allVerticesVisited &&
            !nextVertexIsVisited)
          singleReservedInfo.success = false;

        singleReservedInfo.changedFramePaths.push_back(frameLikelyPaths);

        if (currIter != curr_nextCallChain.cbegin())
          singleReservedInfo.changedFrameGraph.emplace_back(
              frameCurrGraphID, nextGraphCallerVertex);
        else
          singleReservedInfo.changedFrameGraph.emplace_back(frameCurrGraphID,
                                                            0);

        // Record the vertex calling the next graph
        nextGraphCallerVertex = testVertex;

        ++currIter;
        if (nextIter != curr_nextCallChain.cend())
          ++nextIter;
      }

      goto RESULT_CHECK;
    } else if (isCalleeGraph) {
      singleReservedInfo.type = StackChangeType::POP_GRAPH;
      singleReservedInfo.success = true;

      auto currIter = --next_currCallChain.cend();
      auto prevIter = currIter;
      --prevIter;

      auto tempGraphCallStack = reservedInfo.graphCallStack;
      auto tempPathsStack = reservedInfo.pathsStack;
      while (currIter != next_currCallChain.cbegin()) {
        GID frameCurrGraphID = *currIter;
        GID framePrevGraphID = *prevIter;

        // const auto &frameCombinedPaths =
        //     stGraphs[framePrevGraphID].combinedPaths;

        VID prevGraphCallVertex =
            findVertexCallingGraph(framePrevGraphID, frameCurrGraphID);
        assert(prevGraphCallVertex != VID_MAX);

        // The graph is not the recorded top graph or the vertex is not the
        // recorded calling vertex
        assert(tempGraphCallStack.top().first == frameCurrGraphID &&
               tempGraphCallStack.top().second == prevGraphCallVertex);

        singleReservedInfo.changedFrameGraph.emplace_back(frameCurrGraphID,
                                                          prevGraphCallVertex);
        singleReservedInfo.changedFramePaths.push_back(tempPathsStack.top());
        tempGraphCallStack.pop();
        tempPathsStack.pop();

        --currIter;
        if (prevIter != next_currCallChain.cbegin())
          --prevIter;
      }
      // Check next vertex
      {
        GID frameCurrGraphID = nextVertexInGraph;
        const auto &frameCombinedPaths =
            stGraphs[frameCurrGraphID].combinedPaths;
        bool allVerticesVisited =
            stGraphs[frameCurrGraphID].unvisitedVertices.empty();

        auto frameLikelyPaths = tempPathsStack.top();
        std::unordered_set<std::size_t> removedPaths;
        for (std::size_t pathHash : frameLikelyPaths) {
          if (!hasSelectedCovers(frameCurrGraphID,
                                 frameCombinedPaths.at(pathHash).second)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          const auto &path = frameCombinedPaths.at(pathHash).first;

          if (!isVertexOnPath(frameCurrGraphID, path, nextVertex)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          // Check whether this path is uncovered
          if (!isUncoveredPath(frameCurrGraphID, pathHash)) {
            removedPaths.emplace(pathHash);
            stGraphs[frameCurrGraphID].temporaryCoveredPaths.emplace(pathHash);
            continue;
          }
        }
        Utility::difference(frameLikelyPaths, removedPaths);

        if (frameLikelyPaths.empty() && !allVerticesVisited &&
            !nextVertexIsVisited)
          singleReservedInfo.success = false;

        singleReservedInfo.changedFramePaths.push_back(frameLikelyPaths);
        singleReservedInfo.changedFrameGraph.emplace_back(frameCurrGraphID, 0);
      }

      goto RESULT_CHECK;
    } else {
      singleReservedInfo.type = StackChangeType::POP_PUSH_GRAPH;
      singleReservedInfo.success = true;

      //
      // Pop frames first

      auto currIter = --curr_nextCallChain.cend();
      auto prevIter = currIter;
      --prevIter;

      auto tempGraphCallStack = reservedInfo.graphCallStack;
      auto tempPathsStack = reservedInfo.pathsStack;
      while (currIter != curr_nextCallChain.cbegin()) {
        GID frameCurrGraphID = *currIter;
        GID framePrevGraphID = *prevIter;

        // const auto &frameCombinedPaths =
        //     stGraphs[framePrevGraphID].combinedPaths;

        VID prevGraphCallVertex =
            findVertexCallingGraph(framePrevGraphID, frameCurrGraphID);
        assert(prevGraphCallVertex != VID_MAX);

        // The graph is not the recorded top graph or the vertex is not the
        // recorded calling vertex
        assert(tempGraphCallStack.top().first == frameCurrGraphID &&
               tempGraphCallStack.top().second == prevGraphCallVertex);

        singleReservedInfo.changedFrameGraph.emplace_back(frameCurrGraphID,
                                                          prevGraphCallVertex);
        singleReservedInfo.changedFramePaths.push_back(tempPathsStack.top());
        tempGraphCallStack.pop();
        tempPathsStack.pop();

        --currIter;
        if (prevIter != next_currCallChain.cbegin())
          --prevIter;
      }

      //
      // Handle the intermediate s-t graph

      assert(intermeGraphID == tempGraphCallStack.top().first);
      std::unordered_set<std::size_t> intermeLikelyPaths;
      {
        GID frameCurrGraphID = intermeGraphID;
        const auto &frameCombinedPaths =
            stGraphs[frameCurrGraphID].combinedPaths;
        bool allVerticesVisited =
            stGraphs[frameCurrGraphID].unvisitedVertices.empty();

        intermeLikelyPaths = tempPathsStack.top();
        std::unordered_set<std::size_t> removedPaths;
        for (std::size_t pathHash : intermeLikelyPaths) {
          if (!hasSelectedCovers(frameCurrGraphID,
                                 frameCombinedPaths.at(pathHash).second)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          // Check whether this path is uncovered
          if (!isUncoveredPath(frameCurrGraphID, pathHash)) {
            removedPaths.emplace(pathHash);
            stGraphs[frameCurrGraphID].temporaryCoveredPaths.emplace(pathHash);
            continue;
          }
        }
        Utility::difference(intermeLikelyPaths, removedPaths);

        if (intermeLikelyPaths.empty() && !allVerticesVisited &&
            !nextVertexIsVisited)
          singleReservedInfo.success = false;

        singleReservedInfo.changedFramePaths.push_back(intermeLikelyPaths);
        singleReservedInfo.changedFrameGraph.emplace_back(frameCurrGraphID, 0);
      }

      //
      // Push then

      currIter = next_currCallChain.cbegin();
      auto nextIter = ++next_currCallChain.cbegin();
      VID nextGraphCallerVertex = 0;
      while (currIter != next_currCallChain.cend()) {
        GID frameCurrGraphID = *currIter;
        const auto &frameCombinedPaths =
            stGraphs[frameCurrGraphID].combinedPaths;
        bool allVerticesVisited =
            stGraphs[frameCurrGraphID].unvisitedVertices.empty();

        VID testVertex = 0;
        if (nextIter == next_currCallChain.cend()) {
          testVertex = nextVertex;
        } else {
          GID frameNextGraphID = *nextIter;

          VID currGraphCallerVertex =
              findVertexCallingGraph(frameCurrGraphID, frameNextGraphID);
          assert(currGraphCallerVertex != VID_MAX);

          testVertex = currGraphCallerVertex;
        }

        std::unordered_set<std::size_t> frameLikelyPaths;
        if (currIter == next_currCallChain.cbegin())
          frameLikelyPaths = intermeLikelyPaths;
        else {
          for (const auto &mapPair : frameCombinedPaths)
            frameLikelyPaths.emplace(mapPair.first);
        }

        std::unordered_set<std::size_t> removedPaths;
        for (std::size_t pathHash : frameLikelyPaths) {
          if (!hasSelectedCovers(frameCurrGraphID,
                                 frameCombinedPaths.at(pathHash).second)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          const auto &path = frameCombinedPaths.at(pathHash).first;

          if (!isVertexOnPath(frameCurrGraphID, path, testVertex)) {
            removedPaths.emplace(pathHash);
            continue;
          }

          // Check whether this path is uncovered
          if (!isUncoveredPath(frameCurrGraphID, pathHash)) {
            removedPaths.emplace(pathHash);
            stGraphs[frameCurrGraphID].temporaryCoveredPaths.emplace(pathHash);
            continue;
          }
        }
        Utility::difference(frameLikelyPaths, removedPaths);

        if (frameLikelyPaths.empty() && !allVerticesVisited &&
            !nextVertexIsVisited)
          singleReservedInfo.success = false;

        singleReservedInfo.addiChangedFramePaths.push_back(frameLikelyPaths);

        if (currIter != next_currCallChain.cbegin())
          singleReservedInfo.addiChangedFrameGraph.emplace_back(
              frameCurrGraphID, nextGraphCallerVertex);
        else
          singleReservedInfo.addiChangedFrameGraph.emplace_back(
              frameCurrGraphID, 0);

        // Record the vertex calling the next graph
        nextGraphCallerVertex = testVertex;

        ++currIter;
        if (nextIter != next_currCallChain.cend())
          ++nextIter;
      }

      goto RESULT_CHECK;
    }

  RESULT_CHECK:
    tempReservedStackInfo[nextVertex] = singleReservedInfo;
    return singleReservedInfo.success;
  }
}

template <class _Tp>
void DirectedAcyclicGraph<_Tp>::ensureStepVertex(VID nextVertex,
                                                 PathReservedInfo &reservedInfo,
                                                 bool forceNext) {
  assert(tempReservedStackInfo.find(nextVertex) !=
             tempReservedStackInfo.end() &&
         "Wrong status when ensuring to step vertex");

  auto &singleReservedInfo = tempReservedStackInfo.at(nextVertex);

  assert(forceNext || singleReservedInfo.success);

  switch (singleReservedInfo.type) {
  case StackChangeType::INIT: {
    Utility::clear(reservedInfo.graphCallStack);
    Utility::clear(reservedInfo.pathsStack);

    reservedInfo.currVertex = entry;
    reservedInfo.graphCallStack.emplace(0, entry);
    reservedInfo.pathsStack.push(singleReservedInfo.changedFramePaths.front());
  } break;

  case StackChangeType::SAME_GRAPH: {
    reservedInfo.currVertex = nextVertex;
    reservedInfo.pathsStack.top() =
        singleReservedInfo.changedFramePaths.front();
  } break;

  case StackChangeType::PUSH_GRAPH: {
    reservedInfo.currVertex = nextVertex;
    reservedInfo.pathsStack.top() =
        singleReservedInfo.changedFramePaths.front();

    auto pathsIter = ++singleReservedInfo.changedFramePaths.cbegin();
    auto graphIter = ++singleReservedInfo.changedFrameGraph.cbegin();
    for (; pathsIter != singleReservedInfo.changedFramePaths.cend() &&
           graphIter != singleReservedInfo.changedFrameGraph.cend();
         ++pathsIter, ++graphIter) {
      reservedInfo.pathsStack.push(*pathsIter);
      reservedInfo.graphCallStack.push(*graphIter);
    }
  } break;

  case StackChangeType::POP_GRAPH: {
    reservedInfo.currVertex = nextVertex;

    while (!singleReservedInfo.changedFrameGraph.empty() &&
           !singleReservedInfo.changedFramePaths.empty()) {
      if (singleReservedInfo.changedFramePaths.size() == 1)
        reservedInfo.pathsStack.top() =
            singleReservedInfo.changedFramePaths.front();
      else {
        GID frameCurrGraphID =
            singleReservedInfo.changedFrameGraph.front().first;
        auto &pathCovers = stGraphs[frameCurrGraphID].pathCovers;
        auto &frameCombinedPaths = stGraphs[frameCurrGraphID].combinedPaths;
        auto &coveredPaths = stGraphs[frameCurrGraphID].coveredPaths;
        const auto &temporaryCoveredPaths =
            stGraphs[frameCurrGraphID].temporaryCoveredPaths;
        const auto &currLikelyPaths =
            singleReservedInfo.changedFramePaths.front();

        std::unordered_set<CID> selectedCovers;
        Utility::merge(coveredPaths, currLikelyPaths);
        for (std::size_t pathHash : currLikelyPaths)
          Utility::merge(selectedCovers,
                         frameCombinedPaths.at(pathHash).second);
        for (std::size_t pathHash : temporaryCoveredPaths)
          Utility::merge(selectedCovers,
                         frameCombinedPaths.at(pathHash).second);

        for (CID coverID = 0; coverID < pathCovers.size(); ++coverID)
          if (selectedCovers.find(coverID) == selectedCovers.end())
            pathCovers[coverID].selected = false;

        reservedInfo.graphCallStack.pop();
        reservedInfo.pathsStack.pop();
      }

      singleReservedInfo.changedFrameGraph.pop_front();
      singleReservedInfo.changedFramePaths.pop_front();
    }
  } break;

  case StackChangeType::POP_PUSH_GRAPH: {
    reservedInfo.currVertex = nextVertex;

    // Handle pop
    while (!singleReservedInfo.changedFrameGraph.empty() &&
           !singleReservedInfo.changedFramePaths.empty()) {
      if (singleReservedInfo.changedFramePaths.size() == 1)
        reservedInfo.pathsStack.top() =
            singleReservedInfo.changedFramePaths.front();
      else {
        GID frameCurrGraphID =
            singleReservedInfo.changedFrameGraph.front().first;
        auto &pathCovers = stGraphs[frameCurrGraphID].pathCovers;
        auto &frameCombinedPaths = stGraphs[frameCurrGraphID].combinedPaths;
        auto &coveredPaths = stGraphs[frameCurrGraphID].coveredPaths;
        const auto &temporaryCoveredPaths =
            stGraphs[frameCurrGraphID].temporaryCoveredPaths;
        const auto &currLikelyPaths =
            singleReservedInfo.changedFramePaths.front();

        std::unordered_set<CID> selectedCovers;
        Utility::merge(coveredPaths, currLikelyPaths);
        for (std::size_t pathHash : currLikelyPaths)
          Utility::merge(selectedCovers,
                         frameCombinedPaths.at(pathHash).second);
        for (std::size_t pathHash : temporaryCoveredPaths)
          Utility::merge(selectedCovers,
                         frameCombinedPaths.at(pathHash).second);

        for (CID coverID = 0; coverID < pathCovers.size(); ++coverID)
          if (selectedCovers.find(coverID) == selectedCovers.end())
            pathCovers[coverID].selected = false;

        reservedInfo.graphCallStack.pop();
        reservedInfo.pathsStack.pop();
      }

      singleReservedInfo.changedFrameGraph.pop_front();
      singleReservedInfo.changedFramePaths.pop_front();
    }

    // Handle push
    reservedInfo.pathsStack.top() =
        singleReservedInfo.addiChangedFramePaths.front();
    auto pathsIter = ++singleReservedInfo.addiChangedFramePaths.cbegin();
    auto graphIter = ++singleReservedInfo.addiChangedFrameGraph.cbegin();
    for (; pathsIter != singleReservedInfo.addiChangedFramePaths.cend() &&
           graphIter != singleReservedInfo.addiChangedFrameGraph.cend();
         ++pathsIter, ++graphIter) {
      reservedInfo.pathsStack.push(*pathsIter);
      reservedInfo.graphCallStack.push(*graphIter);
    }
  } break;

  case StackChangeType::EXIT: {
    reservedInfo.currVertex = stGraphs[0].tVertex;

    while (!reservedInfo.graphCallStack.empty()) {
      GID frameCurrGraphID = reservedInfo.graphCallStack.top().first;
      auto &pathCovers = stGraphs[frameCurrGraphID].pathCovers;
      auto &frameCombinedPaths = stGraphs[frameCurrGraphID].combinedPaths;
      auto &coveredPaths = stGraphs[frameCurrGraphID].coveredPaths;
      const auto &temporaryCoveredPaths =
          stGraphs[frameCurrGraphID].temporaryCoveredPaths;
      const auto &currLikelyPaths = reservedInfo.pathsStack.top();

      std::unordered_set<CID> selectedCovers;
      Utility::merge(coveredPaths, currLikelyPaths);
      for (std::size_t pathHash : currLikelyPaths)
        Utility::merge(selectedCovers, frameCombinedPaths.at(pathHash).second);
      for (std::size_t pathHash : temporaryCoveredPaths)
        Utility::merge(selectedCovers, frameCombinedPaths.at(pathHash).second);

      for (CID coverID = 0; coverID < pathCovers.size(); ++coverID)
        if (selectedCovers.find(coverID) == selectedCovers.end())
          pathCovers[coverID].selected = false;

      reservedInfo.graphCallStack.pop();
      reservedInfo.pathsStack.pop();
    }
  } break;

  default:
    assert(false);
    break;
  }

  // Record visited nodes
  if (!reservedInfo.graphCallStack.empty()) {
    GID currGraphID = reservedInfo.graphCallStack.top().first;
    auto &currSTGraph = stGraphs[currGraphID];
    currSTGraph.unvisitedVertices.erase(nextVertex);
  }

  // Iterate s-t graphs and clear something
  for (GID graphID = 0; graphID < stGraphs.size(); ++graphID) {
    auto &currSTGraph = stGraphs[graphID];
    currSTGraph.allCalleeVisited = true;
    for (GID calleeGraphID : stGraphCalls.at(graphID)) {
      auto &calleeSTGraph = stGraphs[calleeGraphID];
      if (calleeSTGraph.allCalleeVisited &&
          calleeSTGraph.unvisitedVertices.empty())
        continue;
      else {
        currSTGraph.allCalleeVisited = false;
        break;
      }
    }

    currSTGraph.temporaryCoveredPaths.clear();
  }

  tempReservedStackInfo.clear();
}

template <class _Tp>
bool DirectedAcyclicGraph<_Tp>::queryStepNode(
    const _Tp &currNode, const _Tp &nextNode,
    const PathReservedInfo &reservedInfo) {
  return queryStepVertex(rVertexMap.find(currNode) == rVertexMap.end()
                             ? 0
                             : rVertexMap.at(currNode),
                         rVertexMap.at(nextNode), reservedInfo);
}

template <class _Tp>
void DirectedAcyclicGraph<_Tp>::ensureStepNode(const _Tp &nextNode,
                                               PathReservedInfo &reservedInfo,
                                               bool isExit, bool forceNext) {
  VID nextVertex = rVertexMap.at(nextNode);
  if (isExit) {
    assert(graph.at(nextVertex).empty());
    assert(!reservedInfo.graphCallStack.empty());

    tempReservedStackInfo[nextVertex].type = StackChangeType::EXIT;

    ensureStepVertex(nextVertex, reservedInfo, true);
  } else {
    ensureStepVertex(nextVertex, reservedInfo, forceNext);
  }
}
} // namespace Empc
} // namespace klee

#endif