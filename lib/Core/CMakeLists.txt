#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_library(kleeCore
  AddressSpace.cpp
  MergeHandler.cpp
  CallPathManager.cpp
  Context.cpp
  CoreStats.cpp
  ExecutionState.cpp
  ExecutionTree.cpp
  ExecutionTreeWriter.cpp
  Executor.cpp
  ExecutorUtil.cpp
  ExternalDispatcher.cpp
  ImpliedValue.cpp
  Memory.cpp
  MemoryManager.cpp
  Searcher.cpp
  SeedInfo.cpp
  SpecialFunctionHandler.cpp
  StatsTracker.cpp
  TimingSolver.cpp
  UserSearcher.cpp
)

target_link_libraries(kleeCore PRIVATE
  kleeBasic
  kleeModule
  kleaverSolver
  kleaverExpr
  kleeSupport
)

llvm_config(kleeCore "${USE_LLVM_SHARED}" core executionengine mcjit native support)
target_link_libraries(kleeCore PRIVATE ${SQLite3_LIBRARIES})
target_include_directories(kleeCore PRIVATE ${KLEE_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${SQLite3_INCLUDE_DIRS})
target_compile_options(kleeCore PRIVATE ${KLEE_COMPONENT_CXX_FLAGS})
target_compile_definitions(kleeCore PRIVATE ${KLEE_COMPONENT_CXX_DEFINES})

if (HAVE_PYTHON3)
  target_include_directories(kleeCore PRIVATE ${Python3_INCLUDE_DIRS})
  target_link_libraries(kleeCore PRIVATE ${Python3_LIBRARIES})
  # Derive Python home from the library directory (strip trailing /lib)
  get_filename_component(_python3_home "${Python3_LIBRARY_DIRS}" DIRECTORY)
  if (NOT _python3_home OR _python3_home STREQUAL "/")
    # Fallback: derive from library path
    foreach(_lib ${Python3_LIBRARIES})
      get_filename_component(_dir "${_lib}" DIRECTORY)
      get_filename_component(_python3_home "${_dir}" DIRECTORY)
      break()
    endforeach()
  endif()
  message(STATUS "Python3 home for embedded interpreter: ${_python3_home}")
  target_compile_definitions(kleeCore PRIVATE
    HAVE_PYTHON3=1
    KLEE_PYTHON_HOME="${_python3_home}"
  )
endif()
