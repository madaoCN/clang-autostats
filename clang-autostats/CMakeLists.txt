set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_executable(clang-autostats
  ClangAutoStats.cpp
  )

#add_llvm_loadable_module(clang-autostats 
#  ClangAutoStats.cpp
#  )

#add_llvm_library(clang-autostats MODULE
#  ClangAutoStats.cpp 
#  PLUGIN_TOOL
#  clang
#  )

clang_target_link_libraries(clang-autostats
  PRIVATE
  clangAST
  clangBasic
  clangDriver
  clangFormat
  clangLex
  clangParse
  clangSema
  clangFrontend
  clangTooling
  clangToolingCore
  clangRewrite
  clangRewriteFrontend
  )
