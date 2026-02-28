#include "running/vm.hpp"
#include "front/parser.hpp"
#include "front/code_writer.hpp"
#include "front/compiler.hpp"
#include <iostream>
#include <fstream>

std::string read_file(std::string name) {
    std::ifstream ifs(name);
    std::string buffer, res;
    while (std::getline(ifs, buffer))
        res += buffer + '\n';
    return res;
}

void shell() {
    while (1) {
        std::string data;
        while (printf("> ") && std::getline(std::cin, data)) {
            Lexer lexer(data);
            for (auto i : lexer.tokens)
                i.debug(), printf("\n");
        }
    }
}

int release(int argc, char** argv) {
    if (argc != 3) {
        USAGE:
        printf("Usage: %s -r|-c|-d <SourceFile>\n", argv[0]);
        exit(0);
    }
    std::string decide = argv[1];
    std::string name = argv[2];
    if (decide == "-r") {
        VM vm(name, false);
        return 0;
    } else if (decide == "-c") {
        std::string data = read_file(name);
        Lexer lexer(data);
        Parser parser(lexer.tokens);
        CompileOutput opt;
		ModuleManager* mg = new ModuleManager;
        Compiler compiler(&opt, parser.ast, mg);
        save_code(get_file_name(name) + ".copl", &opt);
        return 0;
    } else if (decide == "-d") {
        for (auto i : load_bytecode(name, builtins))
            if (!i->is_build_in)
                disassemble_chunk(i->codes, i->func_name.c_str());
    } else {
        goto USAGE;
    }
    return 0;
}

void file() {
    std::string data = read_file("D:\\CLionProjects\\COPL\\test\\main");
    Lexer lexer(data);
    Parser parser(lexer.tokens);
    CompileOutput opt;
	ModuleManager* mg = new ModuleManager;
    Compiler compiler(&opt, parser.ast, mg);
    VM vm(opt.funcs);
}

void compile(std::string __in__, std::string __out__) {
	std::ifstream ifs(__in__);
	std::string buffer, res;
	while (std::getline(ifs, buffer))
		res += buffer + '\n';
	Lexer lexer(res);
	Parser parser(lexer.tokens);
	ModuleManager* mg = new ModuleManager;
	CompileOutput op;
	Compiler compiler(&op, parser.ast, mg);
	save_code(__out__, &op);
}

void import_test() {

}

int main(int argc, char **argv) {
    return release(argc, argv);
}