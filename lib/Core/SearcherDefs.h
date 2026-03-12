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

#include <deque>
#include <map>
#include <utility>

namespace Empc {
  enum class StateStepType { COMMON, PUSH, POP };
}

namespace klee {

/// [SGS]: Subpath type — a sequence of (instruction_id, branch_direction) pairs
typedef std::deque<std::pair<unsigned, unsigned>> subpath_ty;

/// [SGS]: Map from subpath to visit count
typedef std::map<subpath_ty, unsigned long> subpathCount_ty;

} // namespace klee

#endif // KLEE_SEARCHERDEFS_H
