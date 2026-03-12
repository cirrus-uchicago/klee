//===-- SearcherDefs.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_SEARCHERDEFS_H
#define KLEE_SEARCHERDEFS_H

#include <cstddef>
#include <deque>
#include <map>
#include <utility>

namespace klee {
namespace Empc {
  enum class StateStepType { COMMON, PUSH, POP };

  // Empc's graph and data analyses grow superlinearly with basic blocks;
  // cjpeg's 32486-block encode_one_block exhausts a 16 GiB container and
  // a 10-minute budget.
  inline constexpr std::size_t MaxFunctionBasicBlocks = 12000;
} // namespace Empc

/// [SGS]: Subpath type — a sequence of (instruction_id, branch_direction) pairs
typedef std::deque<std::pair<unsigned, unsigned>> subpath_ty;

/// [SGS]: Map from subpath to visit count
typedef std::map<subpath_ty, unsigned long> subpathCount_ty;

} // namespace klee

#endif // KLEE_SEARCHERDEFS_H
