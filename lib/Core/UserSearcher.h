//===-- UserSearcher.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_USERSEARCHER_H
#define KLEE_USERSEARCHER_H

namespace klee {
  class Executor;
  class Searcher;

  // XXX gross, should be on demand?
  bool userSearcherRequiresMD2U();
  bool userSearcherRequiresInMemoryExecutionTree();
  bool userSearcherRequiresCGS();
  bool userSearcherRequiresCBC();

  /// @brief [SGS]: Whether SGS searcher is requested
  bool userSearcherRequiresSGS();

  void initializeSearchOptions();

  Searcher *constructUserSearcher(Executor &executor);
}

#endif /* KLEE_USERSEARCHER_H */
