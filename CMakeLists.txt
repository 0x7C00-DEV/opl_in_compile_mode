cmake_minimum_required(VERSION 3.25)
project(COPL)

set(CMAKE_CXX_STANDARD 17)

add_executable(COPL main.cpp
        front/lexer.hpp
        front/parser.hpp
        front/ast.hpp
        running/vm.hpp
        running/asm.hpp
        front/compiler.hpp
        running/value.hpp
        running/native_proc.hpp
        front/code_writer.hpp
        running/program_loader.hpp
        resfile_types.hpp)
