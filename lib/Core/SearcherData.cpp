//===-- SearcherData.cpp ----------------------------------------*- C++ -*-===//
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

#include "SearcherData.h"
#include "SearcherLog.h"

#include <queue>

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

namespace klee {
llvm::cl::OptionCategory EmpcSearcherDataCat(
    "Searcher data options",
    "These options control the Empc searcher data analysis configuration.");

llvm::cl::opt<unsigned> EmpcSearcherDataThreadCount(
    "empc-searcher-data-thread-count", llvm::cl::init(0),
    llvm::cl::desc("The thread count for thread pool used in analyzing "
                   "searcher data analysis (default=1). Hint: since LLVM API "
                   "doesn't support multi-thread, the value is suggested to be "
                   "1 to avoid some data race errors."),
    llvm::cl::cat(EmpcSearcherDataCat));

llvm::cl::opt<bool> EmpcSearcherDataShowAnalyzingProgress(
    "empc-searcher-data-show-progress", llvm::cl::init(true),
    llvm::cl::desc(
        "Show the progress of analyzing data dependence (default=true)"),
    llvm::cl::cat(EmpcSearcherDataCat));
} // namespace klee

namespace klee {
namespace Empc {
namespace Utility {
std::string getDesc(const llvm::Value *value) {
  if (!value)
    return "null";

  std::string result;
  llvm::raw_string_ostream rso(result);
  value->print(rso);
  rso.flush();

  return result;
}

bool hasSubstring(const std::string &str1, const std::string &str2) {
  return str1.find(str2) != std::string::npos;
}

bool isOriginalInstruction(const llvm::Instruction *inst) {
  return inst ? (bool)(inst->getParent()) : false;
}
} // namespace Utility
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
static std::unique_ptr<llvm::ThreadPool> StaticThreadPool;

static bool isBlacklistedFunction(const llvm::Function &func) {
  if (func.empty() || func.isDeclaration() || func.isIntrinsic())
    return true;

  static const std::unordered_set<std::string> prefixBlackListFunctions = {
      "asan.",  "llvm.",   "sancov.", "__ubsan_handle_", "free",    "malloc",
      "calloc", "realloc", "fopen",   "fclose",          "fread",   "fwrite",
      "fgets",  "fputs",   "getchar", "putchar",         "feof",    "ferror",
      "perror", "rewind",  "ftell",   "fseek",           "clearerr"};

  static const std::unordered_set<std::string> postfixBlackListFunctions = {
      "printf", "scanf", "getc", "putc"};

  auto funcName = func.getName();
  for (const auto &name : prefixBlackListFunctions)
    if (funcName.startswith(name))
      return true;

  for (const auto &name : postfixBlackListFunctions)
    if (funcName.endswith(name))
      return true;

  return false;
}

static std::string getBasicBlockIDName(const llvm::BasicBlock *bblock) {
  if (!bblock)
    return "null";

  const llvm::Function *func = bblock->getParent();
  unsigned index = 0;
  for (auto &BB : *func) {
    if (&BB == bblock)
      return func->getName().str() + "():" + std::to_string(index);
    ++index;
  }
  return func->getName().str() + "():-1";
}

} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
std::unordered_map<std::size_t, std::shared_ptr<Variable>> Variable::varMap;
std::unordered_map<std::size_t, std::shared_ptr<Dependee>> Dependee::depMap;
uint64_t Variable::nextId = 0;
uint64_t Dependee::nextId = 0;
std::mutex Variable::sMutex;
std::mutex Dependee::sMutex;

Variable::Type Variable::analyzeType(llvm::Type *preType) {
  if (preType->isArrayTy())
    return Variable::ARRAY;
  else if (preType->isIntegerTy() || preType->isFloatingPointTy())
    return Variable::INT_OR_FLOAT;
  else if (preType->isStructTy())
    return Variable::STRUCT;
  if (preType->isPointerTy()) {
    // In LLVM 16+ with opaque pointers, getPointerElementType() is not
    // available. Opaque pointers have no element type information, so we
    // cannot distinguish struct pointers from other pointers at the type
    // level. Simply classify all pointers as POINTER.
    return Variable::POINTER;
  } else
    return Variable::OTHER;
}

void Variable::revise(std::shared_ptr<Variable> &preVar) {
  std::size_t hashValue = preVar->hash();
  {
    std::unique_lock<std::mutex> locker(sMutex);
    auto iter = varMap.find(hashValue);
    if (iter == varMap.end()) {
      preVar->id = nextId++;
      varMap[hashValue] = preVar;
    } else {
      preVar = iter->second;
    }
  }
}

std::size_t Variable::hash() const {
  std::vector<std::size_t> hashValues;
  hashValues.push_back(std::hash<int>()(kind));
  hashValues.push_back(std::hash<int>()(type));
  hashValues.push_back(definition ? std::hash<const llvm::Value *>()(definition)
                                  : 0);
  hashValues.push_back(
      calleeFunc ? std::hash<const llvm::Function *>()(calleeFunc) : 0);
  hashValues.push_back(std::hash<uint64_t>()(structOffset));
  hashValues.push_back(std::hash<uint64_t>()(pointerOffset));
  hashValues.push_back(
      offsetVar ? std::hash<const Variable *>()(offsetVar.get()) : 0);

  std::size_t result = hashValues.front();
  for (std::size_t i = 1; i < hashValues.size(); ++i)
    result ^= (hashValues[i] << i);

  return result;
}

std::string Variable::str() const {
  std::string result = "ID: " + std::to_string(id);

  result += ";\nVariable Kind: ";
  switch (kind) {
  case Kind::GLOBAL:
    result += "global";
    break;
  case Kind::LOCAL:
    result += "local";
    break;
  case Kind::ARGUMENT:
    result += "argument";
    break;
  case Kind::RETURN:
    result += "callee return";
    break;
  case Kind::CONST:
    result += "constant";
    break;
  default:
    result += "unknown";
    break;
  }

  result += ";\nVariable Type: ";
  switch (type) {
  case Type::CONSTANT:
    result += "constant";
    break;
  case Type::INT_OR_FLOAT:
    result += "integer/floating";
    break;
  case Type::POINTER:
    result += "pointer";
    break;
  case Type::STRUCT:
    result += "struct";
    break;
  case Type::STRUCT_POINTER:
    result += "struct pointer";
    break;
  case Type::ARRAY:
    result += "array";
    break;
  case Type::OTHER:
    result += "miscellaneous";
    break;
  default:
    result += "unknown";
    break;
  }

  result += ";\nDefinition: ";
  result += Utility::getDesc(definition);

  result += ";\nFunction: ";
  if (function)
    result += function->getName().str();
  else
    result += "null";

  result += ";\nCallee: ";
  if (calleeFunc)
    result += calleeFunc->getName().str();
  else
    result += "null";

  result += ";\nStruct Offset: " + std::to_string(structOffset);
  result += ";\nPointer Offset: " + std::to_string(pointerOffset);

  result += ";\nOffset Variable: ";
  if (offsetVar)
    result += std::to_string(offsetVar->id);
  else
    result += "no";
  result += ";\n";

  return result;
}

void Dependee::revise(std::shared_ptr<Dependee> &preDep) {
  std::size_t hashValue = preDep->hash();
  {
    std::unique_lock<std::mutex> locker(sMutex);
    auto iter = depMap.find(hashValue);
    if (iter == depMap.end()) {
      preDep->id = nextId++;
      depMap[hashValue] = preDep;
    } else {
      preDep = iter->second;
    }
  }
}

std::size_t Dependee::hash() const {
  std::vector<std::size_t> hashValues;
  hashValues.push_back(variableDef ? variableDef->hash() : 0);
  hashValues.push_back(bblock ? std::hash<const llvm::BasicBlock *>()(bblock)
                              : 0);
  hashValues.push_back(
      userInst ? std::hash<const llvm::Instruction *>()(userInst) : 0);
  hashValues.push_back(
      storeInst ? std::hash<const llvm::StoreInst *>()(storeInst) : 0);
  hashValues.push_back(
      assignValue ? std::hash<const llvm::Value *>()(assignValue) : 0);
  hashValues.push_back(
      depender ? std::hash<const llvm::Instruction *>()(depender) : 0);

  std::size_t result = hashValues.front();
  for (std::size_t i = 1; i < hashValues.size(); ++i)
    result ^= (hashValues[i] << i);

  return result;
}

std::string Dependee::str() const {
  std::string result = "ID: " + std::to_string(id);

  result += ";\nVariable ID: " +
            std::to_string(variableDef ? variableDef->id : (uint64_t)(-1));
  result += ";\nUser Instruction: " + Utility::getDesc(userInst);
  result += ";\nStore Instruction: " + Utility::getDesc(storeInst);
  result += ";\nAssign Value: " + Utility::getDesc(assignValue);
  result += ";\nDepender: " + Utility::getDesc(depender);
  result += ";\n";

  return result;
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {
uint64_t BasicBlockData::nextIndex = 2;
uint64_t FunctionData::nextIndex = 2;
uint64_t CallerData::nextIndex = 2;
std::unordered_map<const llvm::BasicBlock *, std::unique_ptr<BasicBlockData>>
    BasicBlockData::bblockMap;
std::unordered_map<const llvm::Function *, std::unique_ptr<FunctionData>>
    FunctionData::funcMap;
std::unordered_map<const llvm::Instruction *, std::unique_ptr<CallerData>>
    CallerData::callerInstMap;
std::mutex BasicBlockData::indexMutex;
std::mutex FunctionData::indexMutex;
std::mutex CallerData::indexMutex;
std::mutex BasicBlockData::mapMutex;
std::mutex FunctionData::mapMutex;
std::mutex CallerData::mapMutex;

uint64_t BasicBlockData::getNextIndex() {
  std::unique_lock<std::mutex> locker(indexMutex);
  return nextIndex++;
}

BasicBlockData *
BasicBlockData::getBasicBlockData(const llvm::BasicBlock *_bblock) {
  std::unique_lock<std::mutex> locker(mapMutex);
  return bblockMap.find(_bblock) == bblockMap.end()
             ? nullptr
             : bblockMap.at(_bblock).get();
}

void BasicBlockData::storeBasicBlock(
    const llvm::BasicBlock *_bblock,
    const std::unique_ptr<BasicBlockData> &blockDataPtr) {
  std::unique_lock<std::mutex> locker(mapMutex);
  bblockMap[_bblock] = blockDataPtr->clone();
}

uint64_t FunctionData::getNextIndex() {
  std::unique_lock<std::mutex> locker(indexMutex);
  return nextIndex++;
}

FunctionData *FunctionData::getFunctionData(const llvm::Function *_func) {
  std::unique_lock<std::mutex> locker(mapMutex);

  return _func
             ? (funcMap.find(_func) == funcMap.end() ? nullptr
                                                     : funcMap.at(_func).get())
             : nullptr;
}

void FunctionData::storeFunction(
    const llvm::Function *_func,
    const std::unique_ptr<FunctionData> &funcDataPtr) {
  std::unique_lock<std::mutex> locker(mapMutex);
  funcMap[_func] = funcDataPtr->clone();
}

uint64_t CallerData::getNextIndex() {
  std::unique_lock<std::mutex> locker(indexMutex);
  return nextIndex++;
}

CallerData *CallerData::getCallerData(const llvm::Instruction *_inst) {
  std::unique_lock<std::mutex> locker(mapMutex);
  return callerInstMap.find(_inst) == callerInstMap.end()
             ? nullptr
             : callerInstMap.at(_inst).get();
}

void CallerData::storeCallerInst(
    const llvm::Instruction *_inst,
    const std::unique_ptr<CallerData> &callerDataPtr) {
  std::unique_lock<std::mutex> locker(mapMutex);
  callerInstMap[_inst] = callerDataPtr->clone();
}
} // namespace Empc
} // namespace klee

namespace klee {
namespace Empc {

IntraProcDataAnalyzer::IntraProcDataAnalyzer(
    llvm::Function *_func, std::unordered_set<const llvm::Function *> &callees)
    : func(_func) {
  assert(func && !func->empty());
  // assert(StaticThreadPool);

  callees.clear();

  // Construct the control-flow graph
  for (auto &BB : *func) {
    const auto *bblock = &BB;

    graph[bblock];
    reverseGraph[bblock];

    // Basic block's successors
    for (llvm::const_succ_iterator SI = llvm::succ_begin(bblock),
                                   SE = llvm::succ_end(bblock);
         SI != SE; ++SI) {
      auto successor = *SI;
      graph[bblock].emplace(successor);
      graph[successor];
      reverseGraph[successor].emplace(bblock);
    }
  }
  assert(graph.size() == func->size());
  assert(reverseGraph.size() == func->size());

  // Construct the complementary usage map
  for (auto &BB : *func) {
    for (auto &I : BB) {
      for (unsigned i = 0; i < I.getNumOperands(); ++i) {
        llvm::Value *operand = I.getOperand(i);
        if (auto *constExpr = llvm::dyn_cast<llvm::ConstantExpr>(operand)) {
          if (auto *constExprInst = constExpr->getAsInstruction()) {
            for (unsigned j = 0; j < constExprInst->getNumOperands(); ++j) {
              llvm::Value *value = constExprInst->getOperand(j);
              complemUseMap[value].emplace_back(&I, constExprInst);
            }
          }
        }
      }
    }
  }

  //
  // Find returns and calls via traversing

  std::list<std::pair<llvm::ReturnInst *, llvm::Value *>> returnValues;
  for (auto &BB : *func) {
    for (auto &I : BB) {
      if (auto *returnInst = llvm::dyn_cast<llvm::ReturnInst>(&I)) {
        // Get the return value
        llvm::Value *returnValue = returnInst->getReturnValue();
        if (returnValue)
          returnValues.emplace_back(returnInst, returnValue);
      } else if (I.getOpcode() == llvm::Instruction::Call ||
                 I.getOpcode() == llvm::Instruction::Invoke) {
        bool handleFlag = false;
        std::list<llvm::Value *> paramValues;
        if (auto *callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
          if (auto *calleeFunc = callInst->getCalledFunction()) {
            if (!isBlacklistedFunction(*calleeFunc)) {
              handleFlag = true;
              callees.emplace(calleeFunc);
              for (unsigned i = 0; i < callInst->getNumOperands() - 1; ++i) {
                llvm::Value *arg = callInst->getArgOperand(i);
                paramValues.push_back(arg);
              }
            }
          }
        } else if (auto *invokeInst = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
          if (auto *calleeFunc = invokeInst->getCalledFunction()) {
            if (!isBlacklistedFunction(*calleeFunc)) {
              handleFlag = true;
              callees.emplace(calleeFunc);
              for (unsigned i = 0; i < invokeInst->arg_size(); ++i) {
                llvm::Value *arg = invokeInst->getArgOperand(i);
                paramValues.push_back(arg);
              }
            }
          }
        }

        if (handleFlag) {
          if (!CallerData::getCallerData(&I)) {
            auto callerDataPtr = std::make_unique<CallerData>(&I);
            CallerData::storeCallerInst(&I, callerDataPtr);
          }
          auto *callerData = CallerData::getCallerData(&I);
          callerData->paramValues = paramValues;

          // Add to thread pool
          // StaticThreadPool->async(std::bind(&IntraProcDataAnalyzer::analyzeParameterDataDependency,
          // this, callerData));
          analyzeParameterDataDependency(callerData);
        }
      }
    }
  }

  // Analyze data dependence for return values
  if (!FunctionData::getFunctionData(func)) {
    auto funcDataPtr = std::make_unique<FunctionData>(func);
    FunctionData::storeFunction(func, funcDataPtr);
  }
  auto *funcData = FunctionData::getFunctionData(func);
  funcData->returnValues = returnValues;

  // Add to thread pool
  // StaticThreadPool->async(std::bind(&IntraProcDataAnalyzer::analyzeReturnDataDependency,
  // this, funcData));
  analyzeReturnDataDependency(funcData);

  //
  // Find branches via BFS

  std::unordered_set<const llvm::BasicBlock *> bfsVisitedBlocks;
  std::queue<llvm::BasicBlock *> bfsQueue;
  bfsQueue.push(&func->getEntryBlock());

  while (!bfsQueue.empty()) {
    llvm::BasicBlock *bblock = bfsQueue.front();
    bfsQueue.pop();

    if (bfsVisitedBlocks.find(bblock) != bfsVisitedBlocks.end())
      continue;
    bfsVisitedBlocks.emplace(bblock);

    // Basic block data
    if (!BasicBlockData::getBasicBlockData(bblock)) {
      auto bblockDataPtr = std::make_unique<BasicBlockData>(bblock);
      BasicBlockData::storeBasicBlock(bblock, bblockDataPtr);
    }
    BasicBlockData *bblockData = BasicBlockData::getBasicBlockData(bblock);

    // The terminator instruction
    llvm::Instruction *termInst = bblock->getTerminator();
    assert(termInst);

    // Add successors
    for (unsigned i = 0; i < termInst->getNumSuccessors(); ++i) {
      llvm::BasicBlock *successor = termInst->getSuccessor(i);
      bfsQueue.push(successor);

      // Initialize basic block data
      if (!BasicBlockData::getBasicBlockData(successor)) {
        auto succBBDataPtr = std::make_unique<BasicBlockData>(successor);
        BasicBlockData::storeBasicBlock(successor, succBBDataPtr);
      }
    }

    // Analyze each instruction
    // Only consider conditional branches and switches
    switch (termInst->getOpcode()) {
    case llvm::Instruction::Br: {
      llvm::BranchInst *branchInst = llvm::dyn_cast<llvm::BranchInst>(termInst);
      assert(branchInst);

      if (branchInst->isConditional()) {
        assert(branchInst->getNumOperands() == 3);
        assert(branchInst->getNumSuccessors() == 2);

        llvm::Value *condition = branchInst->getCondition();
        bblockData->endCondition = condition;

        // Check condition instruction
        // llvm::Instruction *condInst =
        //     llvm::dyn_cast<llvm::Instruction>(condition);
        llvm::LLVMContext llvmContext;

        llvm::BasicBlock *succBB1 = branchInst->getSuccessor(0);
        llvm::BasicBlock *succBB2 = branchInst->getSuccessor(1);

        llvm::ConstantData *constTrue = llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(llvmContext), 1, false);
        llvm::ConstantData *constFalse = llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(llvmContext), 0, false);

        BasicBlockData *succBBData1 =
            BasicBlockData::getBasicBlockData(succBB1);
        BasicBlockData *succBBData2 =
            BasicBlockData::getBasicBlockData(succBB2);

        succBBData1->startConditions.emplace_back(bblock, condition, constTrue);
        succBBData2->startConditions.emplace_back(bblock, condition,
                                                  constFalse);

        // Add to thread pool
        // StaticThreadPool->async(std::bind(&IntraProcDataAnalyzer::analyzeBranchDataDependency,
        // this, bblockData));
        analyzeBranchDataDependency(bblockData);
      }
    } break;

    case llvm::Instruction::Switch: {
      llvm::SwitchInst *switchInst = llvm::dyn_cast<llvm::SwitchInst>(termInst);
      assert(switchInst);

      assert(switchInst->getNumOperands() >= 3);
      assert(switchInst->getNumSuccessors() >= 2);

      llvm::Value *condition = switchInst->getCondition();
      bblockData->endCondition = condition;

      std::vector<llvm::ConstantData *> condValues;
      for (unsigned i = 2; i < switchInst->getNumOperands(); ++i) {
        llvm::ConstantData *condValue =
            llvm::dyn_cast<llvm::ConstantData>(switchInst->getOperand(i));
        assert(condValue);
        llvm::BasicBlock *successor =
            llvm::dyn_cast<llvm::BasicBlock>(switchInst->getOperand(++i));
        assert(successor);
        BasicBlockData *succBBData =
            BasicBlockData::getBasicBlockData(successor);
        assert(succBBData);

        succBBData->startConditions.emplace_back(bblock, condition, condValue);
        condValues.push_back(condValue);
      }

      // Handle break sentence
      llvm::BasicBlock *breakSucc =
          llvm::dyn_cast<llvm::BasicBlock>(switchInst->getOperand(1));
      assert(breakSucc);
      BasicBlockData *succBBData = BasicBlockData::getBasicBlockData(breakSucc);
      assert(succBBData);
      succBBData->startConditions.emplace_back(bblock, condition, condValues);

      // Add to thread pool
      // StaticThreadPool->async(std::bind(&IntraProcDataAnalyzer::analyzeBranchDataDependency,
      // this, bblockData));
      analyzeBranchDataDependency(bblockData);
    } break;

    case llvm::Instruction::IndirectBr:
      break;

    default:
      break;
    }
  }
}

void IntraProcDataAnalyzer::analyzeBranchDataDependency(
    BasicBlockData *blockData) {
  llvm::Value *condition = blockData->endCondition;

  if (!condition)
    return;

  // // [DEBUG]
  // llvm::outs() << "Condition: " << *condition << "\n";

  llvm::Instruction *condInst = llvm::dyn_cast<llvm::Instruction>(condition);
  if (!condInst)
    return;

  std::string debugOutput;

  findDefStack.clear();
  auto varDefs = findVariableDefinition(condition);
  for (const auto &var : varDefs) {
    // [DEBUG]
    if (Logging::check(Logging::Type::DATA))
      debugOutput += "[DEFINITION] (Condition) (1st)\n" + var->str() + "\n\n";

    auto varDeps = findVariableDependence(condInst, var);

    // Record the 1st-level dependence
    blockData->firstLevelDependence.emplace_back(var, varDeps);

    // Find the 2nd-level dependence
    for (const auto &dep : varDeps) {
      if (dep->bblock)
        blockData->firstLevelDepBlocks.emplace(dep->bblock);

      // [DEBUG]
      if (Logging::check(Logging::Type::DATA))
        debugOutput += "[DEPENDENCE] (Condition) (1st)\n" + dep->str() + "\n";

      // Only consider the dependee with valid store instructions and assigned
      // values
      if (dep->variableDef && dep->variableDef->kind != Variable::CONST &&
          dep->variableDef->kind != Variable::RETURN && dep->storeInst &&
          dep->assignValue) {
        // [DEBUG]
        if (Logging::check(Logging::Type::DATA))
          debugOutput +=
              "Assigned Value: " + Utility::getDesc(dep->assignValue) + "\n";

        findDefStack.clear();
        auto varDefs2 = findVariableDefinition(dep->assignValue);
        blockData->dependenceLinks.emplace_back(dep, varDefs2);

        for (const auto &var2 : varDefs2) {
          // [DEBUG]
          if (Logging::check(Logging::Type::DATA))
            debugOutput +=
                "[DEFINITION] (Condition) (2nd)\n" + var2->str() + "\n\n";

          auto varDeps2 = findVariableDependence(dep->storeInst, var2);
          blockData->secondLevelDependence.emplace_back(var2, varDeps2);

          for (const auto &dep2 : varDeps2) {
            if (dep2->bblock)
              blockData->secondLevelDepBlocks.emplace(dep2->bblock);

            // [DEBUG]
            if (Logging::check(Logging::Type::DATA)) {
              // [DEBUG]
              debugOutput +=
                  "[DEPENDENCE] (Condition) (2nd)\n" + dep2->str() + "\n";
            }
          }
        }
      }
    }
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::DATA)) {
    debugOutput += "\n";

    Logging::all(Logging::Type::DATA,
                 "Branch Condition: " + Utility::getDesc(condition),
                 debugOutput);
  }

  // Hint
  if (EmpcSearcherDataShowAnalyzingProgress) {
    std::string output =
        "[Program Data Analysis] |Branch Data Dependence| Basic Block: " +
        getBasicBlockIDName(blockData->bblock) + "\n";
    llvm::outs() << output;
  }
}

void IntraProcDataAnalyzer::analyzeReturnDataDependency(
    FunctionData *funcData) {
  assert(funcData->func);

  const auto &returnValues = funcData->returnValues;

  // // [DEBUG]
  // llvm::outs() << "Function: " << funcData->func->getName() << "\n";

  std::string debugOutput;

  for (const auto &returnValue : returnValues) {
    // [DEBUG]
    if (Logging::check(Logging::Type::DATA))
      debugOutput += "Return: " + Utility::getDesc(returnValue.first) + "\n";

    findDefStack.clear();
    auto varDefs = findVariableDefinition(returnValue.second);
    for (const auto &var : varDefs) {
      if (Logging::check(Logging::Type::DATA))
        debugOutput += "[DEFINITION] (Return)\n" + var->str() + "\n\n";

      auto varDeps = findVariableDependence(returnValue.first, var);
      funcData->dependence.emplace_back(var, varDeps);
      for (const auto &dep : varDeps) {
        if (dep->bblock)
          funcData->depBlocks.emplace(dep->bblock);

        // [DEBUG]
        if (Logging::check(Logging::Type::DATA))
          debugOutput += "[DEPENDENCE] (Return)\n" + dep->str() + "\n";
      }
    }
    if (Logging::check(Logging::Type::DATA))
      debugOutput += "\n";
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::DATA)) {
    Logging::all(Logging::Type::DATA,
                 "Return: " + funcData->func->getName().str(), debugOutput);
  }

  // Hint
  if (EmpcSearcherDataShowAnalyzingProgress) {
    std::string output =
        "[Program Data Analysis] |Function Return Data Dependence| Function: " +
        funcData->func->getName().str() + "\n";
    llvm::outs() << output;
  }
}

void IntraProcDataAnalyzer::analyzeParameterDataDependency(
    CallerData *callerData) {
  assert(callerData->callerInst);
  const auto &paramValues = callerData->paramValues;

  std::string debugOutput;

  // // [DEBUG]
  // llvm::outs() << "Caller: " << *callerData->callerInst << "\n";

  for (llvm::Value *paramValue : paramValues) {
    // [DEBUG]
    if (Logging::check(Logging::Type::DATA))
      debugOutput += "Parameter: " + Utility::getDesc(paramValue) + "\n";

    // // [DEBUG]
    // llvm::outs() << "Parameter: " + Utility::getDesc(paramValue) + "\n";

    findDefStack.clear();
    auto varDefs = findVariableDefinition(paramValue);
    for (const auto &var : varDefs) {
      if (Logging::check(Logging::Type::DATA))
        debugOutput += "[DEFINITION] (Call)\n" + var->str() + "\n\n";

      // // [DEBUG]
      // llvm::outs() << "[DEFINITION] (Call)\n" + var->str() + "\n\n";

      auto varDeps = findVariableDependence(callerData->callerInst, var);
      callerData->dependence.emplace_back(var, varDeps);

      for (const auto &dep : varDeps) {
        if (dep->bblock)
          callerData->depBlocks.emplace(dep->bblock);

        // [DEBUG]
        if (Logging::check(Logging::Type::DATA))
          debugOutput += "[DEPENDENCE] (Call)\n" + dep->str() + "\n";

        // // [DEBUG]
        // llvm::outs() << "[DEPENDENCE] (Call)\n" + dep->str() + "\n";
      }
    }
    if (Logging::check(Logging::Type::DATA))
      debugOutput += "\n";
    // // [DEBUG]
    // llvm::outs() << "==================\n";
  }

  // [DEBUG]
  if (Logging::check(Logging::Type::DATA)) {
    Logging::all(Logging::Type::DATA,
                 "Call: " + Utility::getDesc(callerData->callerInst),
                 debugOutput);
  }

  // Hint
  if (EmpcSearcherDataShowAnalyzingProgress) {
    std::string output =
        "[Program Data Analysis] |Function Parameter Data Dependence| Call: " +
        Utility::getDesc(callerData->callerInst) + "\n";
    llvm::outs() << output;
  }
}

std::list<VarPtr>
IntraProcDataAnalyzer::findVariableDefinition(llvm::Value *preVar) {
  assert(preVar);

  // // [DEBUG]
  // llvm::outs() << "Find Def: " << Utility::getDesc(preVar) << "\n";

  // Record in stack
  findDefStack.push_back(preVar);

  std::list<VarPtr> resVars;

  std::queue<llvm::Value *> bfsTraverseQueue;
  bfsTraverseQueue.push(preVar);
  std::unordered_set<llvm::Value *> bfsVisitedValues;
  while (!bfsTraverseQueue.empty()) {
    llvm::Value *currValue = bfsTraverseQueue.front();
    bfsTraverseQueue.pop();

    if (bfsVisitedValues.find(currValue) != bfsVisitedValues.end())
      continue;
    else
      bfsVisitedValues.emplace(currValue);

    // // [DEBUG]
    // llvm::outs() << "Find Def: BFS Current: " << Utility::getDesc(currValue)
    //              << "\n";

    llvm::Instruction *currInst = nullptr;
    llvm::Function *currFunc = nullptr;
    llvm::BasicBlock *currBasicBlock = nullptr;

    if (llvm::isa<llvm::ConstantData>(currValue)) {
      VarPtr resVar =
          VarPtr(new Variable(Variable::CONST, Variable::CONSTANT, currValue));
      resVars.push_back(resVar);
      continue;
    } else if (auto *globalVar =
                   llvm::dyn_cast<llvm::GlobalVariable>(currValue)) {
      VarPtr resVar = VarPtr(new Variable(
          Variable::GLOBAL, Variable::analyzeType(globalVar->getValueType()),
          currValue));
      resVars.push_back(resVar);
      continue;
    } else if (auto *constExpr =
                   llvm::dyn_cast<llvm::ConstantExpr>(currValue)) {
      currInst = constExpr->getAsInstruction();
    } else if (llvm::isa<llvm::Instruction>(currValue)) {
      currInst = llvm::dyn_cast<llvm::Instruction>(currValue);
      currBasicBlock = currInst->getParent();
      currFunc = currBasicBlock->getParent();
    }

    if (currInst) {
      if (auto *callInst = llvm::dyn_cast<llvm::CallInst>(currInst)) {
        VarPtr resVar = VarPtr(new Variable(
            Variable::RETURN, Variable::analyzeType(callInst->getType()),
            currValue, currFunc, currBasicBlock));
        resVar->calleeFunc = callInst->getCalledFunction();
        resVars.push_back(resVar);
      } else if (auto *callInst = llvm::dyn_cast<llvm::InvokeInst>(currInst)) {
        VarPtr resVar = VarPtr(new Variable(
            Variable::RETURN, Variable::analyzeType(callInst->getType()),
            currValue, currFunc, currBasicBlock));
        resVar->calleeFunc = callInst->getCalledFunction();
        resVars.push_back(resVar);
      } else if (auto *loadInst = llvm::dyn_cast<llvm::LoadInst>(currInst)) {
        // std::string currValueDesc = Utility::getDesc(currValue);
        // bool hashGlobal = Utility::hasSubstring(currValueDesc,
        // "@file_label");
        bfsTraverseQueue.push(loadInst->getPointerOperand());

        // auto *gepExpr =
        // llvm::dyn_cast<llvm::ConstantExpr>(loadInst->getPointerOperand()); if
        // (gepExpr)
        //     llvm::outs() << *gepExpr << "\n"
        //                  << *gepExpr->getOperand(0) << "\n";
      } else if (llvm::isa<llvm::StoreInst>(currInst)) {
        continue;
      } else if (auto *allocInst = llvm::dyn_cast<llvm::AllocaInst>(currInst)) {
        VarPtr resVar = VarPtr(
            new Variable(Variable::LOCAL,
                         Variable::analyzeType(allocInst->getAllocatedType()),
                         currValue, currFunc, currBasicBlock));

        // Trace some store instructions to check whether it is an argument
        for (auto *user : allocInst->users()) {
          if (auto *storeInst = llvm::dyn_cast<llvm::StoreInst>(user)) {
            if (llvm::dyn_cast<llvm::Argument>(storeInst->getOperand(0))) {
              resVar->kind = Variable::ARGUMENT;
              break;
            }
          }
        }

        resVars.push_back(resVar);
      } else if (auto *gepInst =
                     llvm::dyn_cast<llvm::GetElementPtrInst>(currInst)) {
        // Handle pointers

        VarPtr resVar =
            VarPtr(new Variable(Variable::LOCAL, Variable::POINTER, currValue,
                                currFunc, currBasicBlock));

        if (gepInst->getNumOperands() == 2) {
          // Poniter
          // Get the pointer itself and offset

          llvm::Value *pointerValue = gepInst->getOperand(0);
          if (std::find(findDefStack.cbegin(), findDefStack.cend(),
                        pointerValue) ==
              findDefStack.cend()) { // Avoid infinite finding
            auto inVars = findVariableDefinition(pointerValue);
            if (inVars.size() == 1) {
              resVar = inVars.front();

              auto *offsetValue = gepInst->getOperand(1);
              if (auto *constOffset =
                      llvm::dyn_cast<llvm::ConstantInt>(offsetValue))
                resVar->pointerOffset = constOffset->getZExtValue();
              else {
                if (std::find(findDefStack.cbegin(), findDefStack.cend(),
                              offsetValue) == findDefStack.cend()) {
                  inVars = findVariableDefinition(offsetValue);
                  if (inVars.size() == 1)
                    resVar->offsetVar = inVars.front();
                }
              }

              resVars.push_back(resVar);
            }
          }
          // else
          // {
          //     // [debug]
          //     llvm::outs() << "[>1] " << gepInst << "\n";
          // }
        } else if (gepInst->getNumOperands() == 3) {
          // Struct pointer

          llvm::Value *pointerValue = gepInst->getOperand(0);
          if (std::find(findDefStack.cbegin(), findDefStack.cend(),
                        pointerValue) == findDefStack.cend()) {
            auto inVars = findVariableDefinition(pointerValue);
            if (inVars.size() == 1) {
              resVar = inVars.front();

              auto *offsetValue = gepInst->getOperand(2);
              if (auto *constOffset =
                      llvm::dyn_cast<llvm::ConstantInt>(offsetValue))
                resVar->structOffset = constOffset->getZExtValue();

              resVars.push_back(resVar);
            }
            // else
            // {
            //     // [debug]
            //     llvm::outs() << "[>1] " << gepInst << "\n";
            // }
          }
        }
      } else {
        // Traverse the assignment

        for (unsigned i = 0; i < currInst->getNumOperands(); ++i)
          bfsTraverseQueue.push(currInst->getOperand(i));
      }
    }
  }

  // Remove redundant
  for (auto &resVar : resVars)
    Variable::revise(resVar);

  findDefStack.pop_back();

  return resVars;
}

std::list<DepPtr>
IntraProcDataAnalyzer::findVariableDependence(llvm::Instruction *dependerInst,
                                              VarPtr varDef) {
  std::list<DepPtr> resDeps;
  if (!dependerInst || !dependerInst->getParent()) {
  } else if (varDef->kind == Variable::Kind::CONST) {
  } else if (varDef->kind == Variable::Kind::RETURN) {
    // auto* inst = llvm::dyn_cast<llvm::Instruction>(varDef->definition);
    DepPtr resDep(new Dependee(varDef, varDef->bblock, nullptr, nullptr,
                               varDef->definition, dependerInst));
    resDeps.push_back(resDep);
  } else {
    // Global values or local values allocated by `alloca`

    std::unordered_map<llvm::Instruction *, llvm::Instruction *> usesMap;

    auto handleVarDef = varDef;
    while (true) {
      assert(handleVarDef->definition);

      for (auto *user : handleVarDef->definition->users()) {
        llvm::Value *userValue = llvm::dyn_cast<llvm::Value>(user);
        if (auto *userInst = llvm::dyn_cast<llvm::Instruction>(userValue)) {
          usesMap[userInst] = nullptr;
        }
      }

      // Add some uses from the complementary use map
      {
        std::unique_lock<std::mutex> locker(compMutex);
        auto iter = complemUseMap.find(handleVarDef->definition);
        if (iter != complemUseMap.end()) {
          for (const auto &complemPair : iter->second) {
            usesMap[complemPair.first] = complemPair.second;
          }
        }
      }

      // if (handleVarDef == varDef && handleVarDef->offsetVar)
      // {
      //     handleVarDef = handleVarDef->offsetVar;
      //     continue;
      // }
      // else
      // {
      //     break;
      // }

      break;
    }

    // We consider a condition about pointer-type or struct type variables.
    // These variables must be first loaded to a register value via
    // `getelementptr` and `load` instructions and then they can be assigned by
    // another value via `store` instruction. Thus, we give an assertion here,
    // which is the `load` and `getelementptr instructions (where the LLVM uses
    // lie in) must be in the same basic block as the basic block where the
    // `store` lies in.

    std::unordered_map<llvm::Instruction *, llvm::StoreInst *> useStoreInstMap;
    std::unordered_map<const llvm::BasicBlock *, llvm::Instruction *>
        useBlockMap;
    for (const auto &usesMapPair : usesMap) {
      // Check each dependee instruction including originality and offset

      llvm::Instruction *originInst = usesMapPair.first;
      llvm::Instruction *transInst = usesMapPair.second;
      if (!Utility::isOriginalInstruction(originInst))
        continue;
      if (originInst->getParent()->getParent() != this->func)
        continue;

      // Check offset for pointer and struct
      if (transInst &&
          transInst->getOpcode() == llvm::Instruction::GetElementPtr) {
        auto *gepInst = llvm::dyn_cast<llvm::GetElementPtrInst>(transInst);
        if (gepInst->getNumOperands() == 3 &&
            varDef->structOffset != (uint64_t)-1) {
          auto *offsetValue = gepInst->getOperand(2);
          if (auto *constOffset =
                  llvm::dyn_cast<llvm::ConstantInt>(offsetValue))
            if (varDef->structOffset != constOffset->getZExtValue())
              continue;
        } else if (gepInst->getNumOperands() == 2 &&
                   varDef->pointerOffset != (uint64_t)-1) {
          auto *offsetValue = gepInst->getOperand(1);
          if (auto *constOffset =
                  llvm::dyn_cast<llvm::ConstantInt>(offsetValue))
            if (varDef->pointerOffset != constOffset->getZExtValue())
              continue;
        }
      }

      auto *storeInst = getStoreInst(originInst);
      if (storeInst) {
        useStoreInstMap[originInst] = storeInst;
        useBlockMap[originInst->getParent()] = originInst;
      }
    }

    // Find a basic block which assign a value to the variable and there is a
    // path from this basic block to the depender basic block such that none of
    // the basic blocks on this path assign values to the variable. (potential
    // dependence and data dependence)

    std::queue<const llvm::BasicBlock *> bfsBackTrackQueue;
    bfsBackTrackQueue.push(dependerInst->getParent());
    std::unordered_set<const llvm::BasicBlock *> bfsVisitedBlocks = {
        dependerInst->getParent()};
    while (!bfsBackTrackQueue.empty()) {
      llvm::BasicBlock *currBB =
          const_cast<llvm::BasicBlock *>(bfsBackTrackQueue.front());
      bfsBackTrackQueue.pop();

      // depender basic block
      if (bfsBackTrackQueue.empty() && currBB == dependerInst->getParent())
        goto ADD_PREDS;

      // Do not consider visited blocks
      if (bfsVisitedBlocks.find(currBB) != bfsVisitedBlocks.end())
        continue;
      bfsVisitedBlocks.emplace(currBB);

      if (useBlockMap.find(currBB) != useBlockMap.end()) {
        // A basic block assigning values to the variable

        llvm::Instruction *useInst = useBlockMap.at(currBB);
        assert(useStoreInstMap.find(useInst) != useStoreInstMap.end());
        llvm::StoreInst *storeInst = useStoreInstMap.at(useInst);
        llvm::Value *assignValue = storeInst->getValueOperand();
        DepPtr resDep = DepPtr(new Dependee(varDef, currBB, useInst, storeInst,
                                            assignValue, dependerInst));
        resDeps.push_back(resDep);
        continue;
      }

    ADD_PREDS: {
      std::unique_lock<std::mutex> locker(reverseMutex);
      auto iter = reverseGraph.find(currBB);
      if (iter != reverseGraph.end())
        for (auto *pred : iter->second)
          bfsBackTrackQueue.push(pred);
    }
    }
  }

  for (auto &resDep : resDeps)
    Dependee::revise(resDep);

  return resDeps;
}

llvm::StoreInst *
IntraProcDataAnalyzer::getStoreInst(llvm::Instruction *preInst) {
  if (!preInst)
    return nullptr;
  if (preInst->getOpcode() == llvm::Instruction::Store)
    return llvm::dyn_cast<llvm::StoreInst>(preInst);
  if (preInst->getOpcode() != llvm::Instruction::Load &&
      preInst->getOpcode() != llvm::Instruction::GetElementPtr)
    return nullptr;

  // // [DEBUG]
  // llvm::outs() << "Pre Inst: " << *preInst << "\n";

  llvm::BasicBlock *preBlock = preInst->getParent();
  bool afterStart = false;
  std::unordered_set<const llvm::Instruction *> avaInsts;
  for (auto &I : *preBlock) {
    if (afterStart)
      avaInsts.emplace(&I);
    else if (&I == preInst)
      afterStart = true;
  }

  //
  // Find a post store instructions

  std::queue<llvm::Instruction *> bfsQueue;
  bfsQueue.push(preInst);
  std::unordered_set<const llvm::Instruction *> bfsVisitedInsts = {preInst};
  while (!bfsQueue.empty()) {
    llvm::Instruction *currInst = bfsQueue.front();
    bfsQueue.pop();

    if (bfsVisitedInsts.find(currInst) != bfsVisitedInsts.end())
      continue;
    bfsVisitedInsts.emplace(currInst);

    for (auto *user : currInst->users()) {
      if (auto *userInst = llvm::dyn_cast<llvm::Instruction>(user)) {
        if (auto *storeInst = llvm::dyn_cast<llvm::StoreInst>(userInst)) {
          return storeInst;
        } else if (avaInsts.find(userInst) != avaInsts.end()) {
          bfsQueue.push(userInst);
        }
      }
    }
  }

  return nullptr;
}

bool IntraProcDataAnalyzer::isRechable(const llvm::Instruction *startInst,
                                       const llvm::Instruction *goalInst) {
  assert(startInst && goalInst);
  if (startInst == goalInst)
    return true;

  const llvm::BasicBlock *startBlock = startInst->getParent();
  const llvm::BasicBlock *goalBlock = goalInst->getParent();

  assert(startBlock->getParent() == goalBlock->getParent());

  if (startBlock == goalBlock) {
    bool afterStart = false;
    for (auto &I : *startBlock) {
      if (afterStart) {
        if (goalInst == &I)
          return true;
      } else if (startInst == &I)
        afterStart = true;
    }

    return false;
  } else
    return isRechable(startBlock, goalBlock);
}

bool IntraProcDataAnalyzer::isRechable(const llvm::BasicBlock *startBlock,
                                       const llvm::BasicBlock *goalBlock) {
  assert(startBlock && goalBlock);
  assert(startBlock->getParent() && goalBlock->getParent());

  if (startBlock == goalBlock)
    return true;

  std::unordered_set<const llvm::BasicBlock *> visitedBlocks;
  std::queue<const llvm::BasicBlock *> bfsQueue;

  // Start BFS from the start node
  bfsQueue.push(startBlock);
  visitedBlocks.insert(startBlock);

  while (!bfsQueue.empty()) {
    const llvm::BasicBlock *current = bfsQueue.front();
    bfsQueue.pop();

    // Check all adjacent nodes
    bool foundCurrent = false;
    std::unordered_set<const llvm::BasicBlock *> successors;
    {
      std::unique_lock<std::mutex> locker(graphMutex);
      auto iter = graph.find(current);
      if (iter != graph.end()) {
        foundCurrent = true;
        successors = iter->second;
      }
    }

    if (foundCurrent) {
      for (const llvm::BasicBlock *neighbor : successors) {
        if (neighbor == goalBlock)
          return true;

        // If not visited, add to queue and mark as visited
        if (visitedBlocks.find(neighbor) == visitedBlocks.end()) {
          visitedBlocks.emplace(neighbor);
          bfsQueue.push(neighbor);
        }
      }
    }
  }

  return false;
}

std::unordered_set<const llvm::BasicBlock *>
IntraProcDataAnalyzer::getSuccessors(const llvm::BasicBlock *bblock) const {
  std::unordered_set<const llvm::BasicBlock *> result;
  if (bblock && graph.find(bblock) != graph.end())
    result = graph.at(bblock);
  return result;
}

InterProcDataAnalyzer::InterProcDataAnalyzer(
    llvm::Module *_module,
    const std::unordered_map<std::string, bool> &definedFunctions) {
  unsigned threadCount = EmpcSearcherDataThreadCount;
  if (!threadCount) {
    // unsigned threadCount = llvm::get_cpus();
    // threadCount = threadCount > 1 ? (threadCount / 2) : threadCount;
    threadCount = 1;
  }
  StaticThreadPool = std::make_unique<llvm::ThreadPool>(
      llvm::hardware_concurrency(threadCount));

  for (auto &F : *_module) {
    auto foundIter = definedFunctions.find(F.getName().str());
    if (foundIter == definedFunctions.end() || !foundIter->second ||
        isBlacklistedFunction(F))
      continue;

    auto &pda = analyzerMap[&F];
    auto &callees = callGraph[&F];
    // pda = std::make_shared<IntraProcDataAnalyzer>(&F, callees);
    StaticThreadPool->async(
        [&]() { pda = std::make_shared<IntraProcDataAnalyzer>(&F, callees); });
  }

  StaticThreadPool->wait();
}

bool InterProcDataAnalyzer::isRechable(const llvm::Instruction *startInst,
                                       const llvm::Instruction *goalInst) {
  if (!startInst || !goalInst)
    return false;

  const llvm::BasicBlock *startBlock = startInst->getParent();
  const llvm::BasicBlock *goalBlock = goalInst->getParent();
  if (!startBlock || !goalBlock)
    return false;

  const llvm::Function *startFunc = startBlock->getParent();
  const llvm::Function *goalFunc = goalBlock->getParent();
  if (!startFunc || !goalFunc)
    return false;
  if (startFunc != goalFunc) {
    if (callGraph[startFunc].find(goalFunc) != callGraph[startFunc].end())
      return true;
    else if (callGraph[goalFunc].find(startFunc) != callGraph[goalFunc].end())
      return true;
    else
      return false;
  }
  return analyzerMap.at(startFunc)->isRechable(startInst, goalInst);
}

bool InterProcDataAnalyzer::addCallee(const llvm::BasicBlock *caller,
                                      const llvm::BasicBlock *callee) {
  if (!caller || !callee)
    return false;

  const llvm::Function *callerFunc = caller->getParent();
  const llvm::Function *calleeFunc = callee->getParent();

  if (callGraph.find(callerFunc) == callGraph.end() ||
      callGraph.find(calleeFunc) == callGraph.end())
    return false;

  callGraph[caller->getParent()].emplace(callee->getParent());
  return true;
}

std::unordered_set<const llvm::BasicBlock *>
InterProcDataAnalyzer::getSuccessors(const llvm::BasicBlock *bblock) const {
  std::unordered_set<const llvm::BasicBlock *> result;
  if (bblock && analyzerMap.find(bblock->getParent()) != analyzerMap.end()) {
    result = analyzerMap.at(bblock->getParent())->getSuccessors(bblock);
  }
  return result;
}
} // namespace Empc
} // namespace klee