//===-- SearcherLog.h -------------------------------------------*- C++ -*-===//
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

#ifndef SEARCHERLOG_H_
#define SEARCHERLOG_H_

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "llvm/Support/raw_ostream.h"

namespace klee {
namespace Empc {
class Logging {
public:
  enum Type {
    GRAPH,
    DATA,
    STATE,
    DEBUG,
    OTHER,
  };

private:
  static Type reservedType;
  static std::unordered_map<Type, bool> loggingCheckMap;
  static std::unordered_map<Type, std::unique_ptr<llvm::raw_fd_ostream>>
      loggingFileMap;
  static std::mutex mutex;

public:
  static void
  init(std::function<std::unique_ptr<llvm::raw_fd_ostream>(const std::string &)>
           openFileHandler);

  static bool check(Type _type);
  static void start(Type _type, const std::string &_title);
  static void log(const std::string &_log);
  static llvm::raw_fd_ostream &log();
  static void stop();

  static void all(Type _type, const std::string &_title,
                  const std::string &_log);
};
} // namespace Empc
} // namespace klee

#endif