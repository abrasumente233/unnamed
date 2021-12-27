
#include "unnamed.h"
#include "compiler.h"

#include "lexer.cpp"
#include "parser.cpp"
#include "il.cpp"
#include "llvm_converter.cpp"
// #include "bytecode.cpp"

int main(i32 argc, char **argv) {

    for (i32 i = 1; i < argc; i++) {
        char *option = argv[i];

        if (option[0] == '-') {
            if (string_match(option, "-o")) {
                i += 1;
                assert(i < argc);
                options.output_filename = argv[i];
                continue;
            } else if (string_match(option, "-O0")) {
                options.optimization_level = 0;
            } else if (string_match(option, "-O1")) {
                options.optimization_level = 1;
            } else if (string_match(option, "-O2")) {
                options.optimization_level = 2;
            } else if (string_match(option, "-O3")) {
                options.optimization_level = 3;
            } else {
                printf("Usage: %s <filename>\n", argv[0]);
                return 1;
            }
        } else {
            options.input_filename = option;
        }
    }

    if (!options.input_filename) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char *source_content = read_entire_file(options.input_filename);

    if (!source_content) {
        printf("Cannot open source file: %s\n", options.input_filename);
        return 1;
    }

    Lexer lexer(source_content);
    lexer.tokenize();

    AST::Parser parser;    
    auto module_ast = parser.parse_module(&lexer);

    //auto module_il = IL::convert_module(module_ast);

    //print_il_module(module_il);

    auto llvm_module = llvm_conv::convert_module(module_ast);

    llvm_conv::emit_object_file(llvm_module);

    return 0;
}
