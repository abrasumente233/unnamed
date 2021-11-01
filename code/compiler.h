
struct Compiler_Options {
    // @note that we have only one input now.
    const char *input_filename;
    const char *output_filename;

    // optimization levels to enable for LLVM backend.
    // defaults to 0.
    // can be 0, 1, 2, 3.
    u32 optimization_level;
};

static Compiler_Options options = {};
