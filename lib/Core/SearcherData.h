//===-- SearcherData.h ------------------------------------------*- C++ -*-===//
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

#ifndef EMPC_SEARCHERDATA_H_
#define EMPC_SEARCHERDATA_H_

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <mutex>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

namespace klee {
namespace Empc {
namespace Utility {
std::string getDesc(const llvm::Value *value);

bool hasSubstring(const std::string &str1, const std::string &str2);

bool isOriginalInstruction(const llvm::Instruction *inst);
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
class Variable {
private:
  static std::unordered_map<std::size_t, std::shared_ptr<Variable>> varMap;
  static uint64_t nextId;

  static std::mutex sMutex;

public:
  static void revise(std::shared_ptr<Variable> &preVar);

public:
  enum Kind {
    GLOBAL = 0,
    LOCAL,
    ARGUMENT,
    RETURN,
    CONST,
  } kind;

  enum Type {
    CONSTANT = 0,
    INT_OR_FLOAT,
    POINTER,
    ARRAY,
    STRUCT,
    STRUCT_POINTER,
    OTHER,
  } type;

  uint64_t id;

  llvm::Value *definition;

  llvm::Function *function;
  llvm::BasicBlock *bblock;

  llvm::Function *calleeFunc;
  uint64_t structOffset;
  uint64_t pointerOffset;

  std::shared_ptr<Variable> offsetVar;

  static Type analyzeType(llvm::Type *preType);

  Variable()
      : kind(Kind::LOCAL), type(Type::OTHER), id(-1), definition(nullptr),
        function(nullptr), bblock(nullptr), calleeFunc(nullptr),
        structOffset(-1), pointerOffset(-1), offsetVar(nullptr) {}
  Variable(Kind _kind, Type _type)
      : kind(_kind), type(_type), id(-1), definition(nullptr),
        function(nullptr), bblock(nullptr), calleeFunc(nullptr),
        structOffset(-1), pointerOffset(-1), offsetVar(nullptr) {}
  Variable(Kind _kind, Type _type, llvm::Value *def)
      : kind(_kind), type(_type), id(-1), definition(def), function(nullptr),
        bblock(nullptr), calleeFunc(nullptr), structOffset(-1),
        pointerOffset(-1), offsetVar(nullptr) {}
  Variable(Kind _kind, Type _type, llvm::Value *def, llvm::Function *func,
           llvm::BasicBlock *block)
      : kind(_kind), type(_type), id(-1), definition(def), function(func),
        bblock(block), calleeFunc(nullptr), structOffset(-1), pointerOffset(-1),
        offsetVar(nullptr) {}

  std::size_t hash() const;
  std::string str() const;
};

using VarPtr = std::shared_ptr<Variable>;

class Dependee {
private:
  static std::unordered_map<std::size_t, std::shared_ptr<Dependee>> depMap;
  static uint64_t nextId;

  static std::mutex sMutex;

public:
  static void revise(std::shared_ptr<Dependee> &preDep);

public:
  uint64_t id;

  VarPtr variableDef;

  llvm::BasicBlock *bblock;

  // The instruction using the variable (maybe `store` or other instructions
  // like `load`)
  llvm::Instruction *userInst;

  // The instruction assigning a value to the variable
  llvm::StoreInst *storeInst;

  // The value assigned to the variable
  llvm::Value *assignValue;

  llvm::Instruction *depender;

  Dependee()
      : id(-1), variableDef(nullptr), bblock(nullptr), userInst(nullptr),
        storeInst(nullptr), assignValue(nullptr), depender(nullptr) {}
  Dependee(VarPtr varDef, llvm::BasicBlock *bblock, llvm::Instruction *userInst,
           llvm::StoreInst *storeInst, llvm::Value *assignValue,
           llvm::Instruction *depender)
      : id(-1), variableDef(varDef), bblock(bblock), userInst(userInst),
        storeInst(storeInst), assignValue(assignValue), depender(depender) {}

  std::size_t hash() const;
  std::string str() const;
};

using DepPtr = std::shared_ptr<Dependee>;
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
class BasicBlockData {
private:
  static uint64_t nextIndex;
  static std::mutex indexMutex;
  static uint64_t getNextIndex();

  static std::unordered_map<const llvm::BasicBlock *,
                            std::unique_ptr<BasicBlockData>>
      bblockMap;
  static std::mutex mapMutex;

public:
  static BasicBlockData *getBasicBlockData(const llvm::BasicBlock *_bblock);
  static void
  storeBasicBlock(const llvm::BasicBlock *_bblock,
                  const std::unique_ptr<BasicBlockData> &blockDataPtr);
  // static void storeBasicBlock(const llvm::BasicBlock *_bblock,
  // std::unique_ptr<BasicBlockData> &&blockDataPtr) {
  // bblockMap.emplace(_bblock, blockDataPtr); }

public:
  struct StartCondition {
    llvm::BasicBlock *predecessor;
    llvm::Value *condition;

    bool positive;
    std::vector<llvm::ConstantData *> values;

    StartCondition()
        : predecessor(nullptr), condition(nullptr), positive(true) {}
    StartCondition(llvm::BasicBlock *predecessor, llvm::Value *condition,
                   llvm::ConstantData *value)
        : predecessor(predecessor), condition(condition), positive(true),
          values({value}) {}
    StartCondition(llvm::BasicBlock *predecessor, llvm::Value *condition,
                   std::vector<llvm::ConstantData *> values)
        : predecessor(predecessor), condition(condition), positive(false),
          values(values) {}
  };

public:
  uint64_t id;
  llvm::BasicBlock *bblock;

  std::list<StartCondition> startConditions;

  llvm::Value *endCondition;

  std::list<std::pair<VarPtr, std::list<DepPtr>>> firstLevelDependence;
  std::list<std::pair<VarPtr, std::list<DepPtr>>> secondLevelDependence;
  std::list<std::pair<DepPtr, std::list<VarPtr>>> dependenceLinks;

  std::unordered_set<const llvm::BasicBlock *> firstLevelDepBlocks;
  std::unordered_set<const llvm::BasicBlock *> secondLevelDepBlocks;

private:
  BasicBlockData(const BasicBlockData &_other)
      : id(_other.id), bblock(_other.bblock),
        startConditions(_other.startConditions),
        endCondition(_other.endCondition),
        firstLevelDependence(_other.firstLevelDependence),
        secondLevelDependence(_other.secondLevelDependence),
        dependenceLinks(_other.dependenceLinks),
        firstLevelDepBlocks(_other.firstLevelDepBlocks),
        secondLevelDepBlocks(_other.secondLevelDepBlocks) {}

public:
  BasicBlockData(llvm::BasicBlock *_bblock)
      : id(getNextIndex()), bblock(_bblock), endCondition(nullptr) {}
  ~BasicBlockData() = default;

  BasicBlockData() = delete;
  BasicBlockData &operator=(const BasicBlockData &) = delete;

  std::unique_ptr<BasicBlockData> clone() {
    return std::unique_ptr<BasicBlockData>(new BasicBlockData(*this));
  }
};

class FunctionData {
private:
  static uint64_t nextIndex;
  static std::mutex indexMutex;
  static uint64_t getNextIndex();

  static std::unordered_map<const llvm::Function *,
                            std::unique_ptr<FunctionData>>
      funcMap;
  static std::mutex mapMutex;

public:
  static FunctionData *getFunctionData(const llvm::Function *_func);
  static void storeFunction(const llvm::Function *_func,
                            const std::unique_ptr<FunctionData> &funcDataPtr);

public:
  uint64_t id;
  llvm::Function *func;

  std::list<std::pair<llvm::ReturnInst *, llvm::Value *>> returnValues;

  std::list<std::pair<VarPtr, std::list<DepPtr>>> dependence;
  std::unordered_set<const llvm::BasicBlock *> depBlocks;

private:
  FunctionData(const FunctionData &_other)
      : id(_other.id), func(_other.func), returnValues(_other.returnValues),
        dependence(_other.dependence), depBlocks(_other.depBlocks) {}

public:
  FunctionData(llvm::Function *_func) : id(getNextIndex()), func(_func) {}
  ~FunctionData() = default;

  FunctionData() = delete;
  FunctionData &operator=(const FunctionData &) = delete;

  std::unique_ptr<FunctionData> clone() {
    return std::unique_ptr<FunctionData>(new FunctionData(*this));
  }
};

class CallerData {
private:
  static uint64_t nextIndex;
  static std::mutex indexMutex;
  static uint64_t getNextIndex();

  static std::unordered_map<const llvm::Instruction *,
                            std::unique_ptr<CallerData>>
      callerInstMap;
  static std::mutex mapMutex;

public:
  static CallerData *getCallerData(const llvm::Instruction *_inst);
  static void storeCallerInst(const llvm::Instruction *_inst,
                              const std::unique_ptr<CallerData> &callerDataPtr);

public:
  uint64_t id;
  llvm::Instruction *callerInst;

  std::list<llvm::Value *> paramValues;

  std::list<std::pair<VarPtr, std::list<DepPtr>>> dependence;

  std::unordered_set<const llvm::BasicBlock *> depBlocks;

private:
  CallerData(const CallerData &_other)
      : id(_other.id), callerInst(_other.callerInst),
        paramValues(_other.paramValues), dependence(_other.dependence),
        depBlocks(_other.depBlocks) {}

public:
  CallerData(llvm::Instruction *_inst)
      : id(getNextIndex()), callerInst(_inst) {}
  ~CallerData() = default;

  CallerData() = delete;
  CallerData &operator=(const CallerData &) = delete;

  std::unique_ptr<CallerData> clone() {
    return std::unique_ptr<CallerData>(new CallerData(*this));
  }
};
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

class IntraProcDataAnalyzer {
private:
  llvm::Function *func;

  std::unordered_map<const llvm::BasicBlock *,
                     std::unordered_set<const llvm::BasicBlock *>>
      graph;
  std::mutex graphMutex;

  std::unordered_map<const llvm::BasicBlock *,
                     std::unordered_set<const llvm::BasicBlock *>>
      reverseGraph;
  std::mutex reverseMutex;

  std::unordered_map<
      const llvm::Value *,
      std::list<std::pair<llvm::Instruction *, llvm::Instruction *>>>
      complemUseMap;
  std::mutex compMutex;

  std::list<llvm::Value *> findDefStack;

  void analyzeBranchDataDependency(BasicBlockData *blockData);

  void analyzeReturnDataDependency(FunctionData *funcData);

  void analyzeParameterDataDependency(CallerData *callerData);

  std::list<VarPtr> findVariableDefinition(llvm::Value *preVar);

  std::list<DepPtr> findVariableDependence(llvm::Instruction *dependerInst,
                                           VarPtr varDef);

  llvm::StoreInst *getStoreInst(llvm::Instruction *preInst);

  bool isRechable(const llvm::BasicBlock *startBlock,
                  const llvm::BasicBlock *goalBlock);

public:
  IntraProcDataAnalyzer(llvm::Function *_func,
                        std::unordered_set<const llvm::Function *> &callees);
  ~IntraProcDataAnalyzer() = default;

  bool isRechable(const llvm::Instruction *startInst,
                  const llvm::Instruction *goalInst);

  std::unordered_set<const llvm::BasicBlock *>
  getSuccessors(const llvm::BasicBlock *bblock) const;
};

class InterProcDataAnalyzer {
private:
  std::unordered_map<const llvm::Function *,
                     std::shared_ptr<IntraProcDataAnalyzer>>
      analyzerMap;

  std::unordered_map<const llvm::Function *,
                     std::unordered_set<const llvm::Function *>>
      callGraph;

public:
  InterProcDataAnalyzer(
      llvm::Module *_module,
      const std::unordered_map<std::string, bool> &definedFunctions);
  ~InterProcDataAnalyzer() = default;

  bool isRechable(const llvm::Instruction *baseInst,
                  const llvm::Instruction *endInst);

  bool addCallee(const llvm::BasicBlock *caller,
                 const llvm::BasicBlock *callee);

  std::unordered_set<const llvm::BasicBlock *>
  getSuccessors(const llvm::BasicBlock *bblock) const;
};

} // namespace Empc
} // namespace klee

#endif