//===-- UserSearcher.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UserSearcher.h"

#include "Executor.h"
#include "MergeHandler.h"
#include "Searcher.h"

#include "klee/Config/config.h"
#include "klee/Support/ErrorHandling.h"

#include "llvm/Support/CommandLine.h"

#include <algorithm>
#include <cstdlib>
#include <string>

using namespace llvm;
using namespace klee;

namespace klee {
llvm::cl::OptionCategory
    SearchCat("Search options", "These options control the search heuristic.");

cl::list<Searcher::CoreSearchType> CoreSearch(
    "search",
    cl::desc("Specify the search heuristic (default=random-path interleaved "
             "with nurs:covnew)"),
    cl::values(
        clEnumValN(Searcher::DFS, "dfs", "use Depth First Search (DFS)"),
        clEnumValN(Searcher::BFS, "bfs",
                   "use Breadth First Search (BFS), where scheduling decisions "
                   "are taken at the level of (2-way) forks"),
        clEnumValN(Searcher::RandomState, "random-state",
                   "randomly select a state to explore"),
        clEnumValN(Searcher::RandomPath, "random-path",
                   "use Random Path Selection (see OSDI'08 paper)"),
        clEnumValN(Searcher::NURS_CovNew, "nurs:covnew",
                   "use Non Uniform Random Search (NURS) with Coverage-New"),
        clEnumValN(Searcher::NURS_MD2U, "nurs:md2u",
                   "use NURS with Min-Dist-to-Uncovered"),
        clEnumValN(Searcher::NURS_Depth, "nurs:depth", "use NURS with depth"),
        clEnumValN(Searcher::NURS_RP, "nurs:rp", "use NURS with 1/2^depth"),
        clEnumValN(Searcher::NURS_ICnt, "nurs:icnt",
                   "use NURS with Instr-Count"),
        clEnumValN(Searcher::NURS_CPICnt, "nurs:cpicnt",
                   "use NURS with CallPath-Instr-Count"),
        clEnumValN(Searcher::NURS_QC, "nurs:qc", "use NURS with Query-Cost"),
        /* [SGS]: Subpath guided searcher */
        clEnumValN(Searcher::SGS, "sgs", "use SGS (subpath guided searcher)"),
        clEnumValN(Searcher::CGS, "cgs",
                   "use Concrete-constraint Guided Search (CGS, ICSE'24)"),
        clEnumValN(Searcher::CBC, "cbc",
                   "use Concolic-Based Coverage (CBC)")
#ifdef HAVE_PYTHON3
        ,clEnumValN(Searcher::Learch, "learch",
                   "use Learch ML-based search (feedforward model, CCS'21)")
#endif
        ),
    cl::cat(SearchCat));

cl::opt<bool> UseIterativeDeepeningTimeSearch(
    "use-iterative-deepening-time-search",
    cl::desc(
        "Use iterative deepening time search (experimental) (default=false)"),
    cl::init(false),
    cl::cat(SearchCat));

cl::opt<bool> UseBatchingSearch(
    "use-batching-search",
    cl::desc("Use batching searcher (keep running selected state for N "
             "instructions/time, see --batch-instructions and --batch-time) "
             "(default=false)"),
    cl::init(false),
    cl::cat(SearchCat));

cl::opt<unsigned> BatchInstructions(
    "batch-instructions",
    cl::desc("Number of instructions to batch when using "
             "--use-batching-search.  Set to 0 to disable (default=10000)"),
    cl::init(10000),
    cl::cat(SearchCat));

cl::opt<std::string> BatchTime(
    "batch-time",
    cl::desc("Amount of time to batch when using "
             "--use-batching-search.  Set to 0s to disable (default=5s)"),
    cl::init("5s"),
    cl::cat(SearchCat));

void initializeSearchOptions() {
  // default values
  if (CoreSearch.empty()) {
    if (UseMerge) {
      CoreSearch.push_back(Searcher::NURS_CovNew);
      klee_warning("--use-merge enabled. Using NURS_CovNew as default searcher.");
    } else {
      CoreSearch.push_back(Searcher::RandomPath);
      CoreSearch.push_back(Searcher::NURS_CovNew);
    }
  }
}

bool userSearcherRequiresMD2U() {
  return (std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_MD2U) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_CovNew) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_ICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_CPICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_QC) != CoreSearch.end());
}

bool userSearcherRequiresInMemoryExecutionTree() {
  return std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::RandomPath) != CoreSearch.end();
}

// [SGS]:
bool userSearcherRequiresSGS() {
  return std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::SGS) !=
         CoreSearch.end();
}

bool userSearcherRequiresCGS() {
  return std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::CGS) !=
         CoreSearch.end();
}

bool userSearcherRequiresCBC() {
  return std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::CBC) !=
         CoreSearch.end();
}

} // namespace klee

Searcher *getNewSearcher(Searcher::CoreSearchType type, RNG &rng,
                         InMemoryExecutionTree *executionTree,
                         Executor &executor) {
  Searcher *searcher = nullptr;
  switch (type) {
    case Searcher::DFS: searcher = new DFSSearcher(); break;
    case Searcher::BFS: searcher = new BFSSearcher(); break;
    case Searcher::RandomState: searcher = new RandomSearcher(rng); break;
    case Searcher::RandomPath: searcher = new RandomPathSearcher(executionTree, rng); break;
    case Searcher::NURS_CovNew: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CoveringNew, rng); break;
    case Searcher::NURS_MD2U: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::MinDistToUncovered, rng); break;
    case Searcher::NURS_Depth: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::Depth, rng); break;
    case Searcher::NURS_RP: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::RP, rng); break;
    case Searcher::NURS_ICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::InstCount, rng); break;
    case Searcher::NURS_CPICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CPInstCount, rng); break;
    case Searcher::NURS_QC: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::QueryCost, rng); break;
    case Searcher::SGS: {
      std::vector<Searcher *> s;
      for (unsigned i = 0; i <= 3; i++)
        s.push_back(new SubpathGuidedSearcher(executor, i, rng));
      searcher = new InterleavedSearcher(s);
    } break;
    case Searcher::CGS: return nullptr; // handled in constructUserSearcher
    case Searcher::CBC: return nullptr; // handled in constructUserSearcher
#ifdef HAVE_PYTHON3
    case Searcher::Learch: {
      std::string modelPath;
      const char *envPath = std::getenv("KLEE_LEARCH_MODEL_DIR");
      if (envPath) {
        modelPath = std::string(envPath) + "/trained/feedforward_0.pt";
      } else {
        // Use installed path (works for nix builds and make install)
        modelPath = std::string(KLEE_INSTALL_LEARCH_DIR) + "/trained/feedforward_0.pt";
      }
      searcher = new MLSearcher(executor, modelPath);
      break;
    }
#endif
  }

  return searcher;
}

Searcher *klee::constructUserSearcher(Executor &executor) {
  auto *etree =
      llvm::dyn_cast<InMemoryExecutionTree>(executor.executionTree.get());

  if (userSearcherRequiresCBC() && CoreSearch.size() != 1) {
    klee_error("Searching strategy 'cbc' cannot be combined with other strategies");
  }

  Searcher *searcher = nullptr;
  if (CoreSearch[0] == Searcher::CGS) {
    klee_error("CGS searcher is not implemented on this branch");
  } else if (CoreSearch[0] == Searcher::CBC) {
    executor.pendingMode = true;
    executor.gatherSenstiveInstructions = true;
    auto *zestiPs = new ZESTIPendingSearcher(executor);
    searcher = new SwappingSearcher(
        new PendingSearcher(new DFSSearcher(), new EmptySearcher(), executor),
        zestiPs,
        [&executor]() {
          executor.gatherSenstiveInstructions = false;
          executor.normalMode();
        });
  } else {
    searcher = getNewSearcher(CoreSearch[0], executor.theRNG, etree, executor);
  }

  if (CoreSearch.size() > 1) {
    std::vector<Searcher *> s;
    s.push_back(searcher);

    for (unsigned i = 1; i < CoreSearch.size(); i++)
      s.push_back(getNewSearcher(CoreSearch[i], executor.theRNG, etree, executor));

    searcher = new InterleavedSearcher(s);
  }

  // [SGS]: Check single SGS searcher
  if (std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::SGS) !=
      CoreSearch.end()) {
    if (CoreSearch.size() != 1) {
      klee_error("Searching strategy 'SGS' can NOT be used together with other "
                 "strategies");
    }
  }

#ifdef HAVE_PYTHON3
  if (std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::Learch) != CoreSearch.end()) {
    executor.featureExtract = true;
    searcher = new GetFeaturesSearcher(searcher, executor);
    searcher = new BranchingSearcher(searcher, executor);
  }
#endif

  if (UseBatchingSearch) {
    searcher = new BatchingSearcher(searcher, time::Span(BatchTime),
                                    BatchInstructions);
  }

  if (UseIterativeDeepeningTimeSearch) {
    searcher = new IterativeDeepeningTimeSearcher(searcher);
  }

  if (UseMerge) {
    auto *ms = new MergingSearcher(searcher);
    executor.setMergingSearcher(ms);

    searcher = ms;
  }

  llvm::raw_ostream &os = executor.getHandler().getInfoStream();

  os << "BEGIN searcher description\n";
  searcher->printName(os);
  os << "END searcher description\n";

  return searcher;
}
