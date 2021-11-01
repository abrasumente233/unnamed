#include "debugbreak.h"

#define _TEXT_NORMAL "\033[0m"
#define _TEXT_RED    "\033[1;31m"
#define _TEXT_BLUE   "\033[1;34m"

inline void red_text() {
    printf(_TEXT_RED);
}

inline void blue_text() {
    printf(_TEXT_BLUE);
}

inline void normal_text() {
    printf(_TEXT_NORMAL);
}

struct Token {
    enum Token_Type {
        END = 0,

        IDENTIFIER = 256,
        INTEGER,
        STRING,

        KEYWORD_START = 500,

        KEYWORD_WHILE = KEYWORD_START,
        KEYWORD_FUNC,
        KEYWORD_I8,
        KEYWORD_I16,
        KEYWORD_I32,
        KEYWORD_I64,
        KEYWORD_VOID,
        KEYWORD_RETURN,

        DIRECTIVE_START,
        DIRECTIVE_C_FUNCTION = DIRECTIVE_START,
        DIRECTIVE_END,

        KEYWORD_END = DIRECTIVE_END,

        ARROW, // ->
    };

    u32 type;

    /* @note(44)
     * We want the lexer to print out the line where error occurs,
     * and there's probably two places where an error can happen.
     * 1) parsing, where we know which token we're on.
     * 2) type checking, where we don't.
     *
     * For case 1, we just store the line and column numbers for each token?
     * @TODO: do something for case 2.
     * @TODO: Also, 8 bytes might be too much to pack in the token sturct, because
     * cache untilization might suffer? I wish to see some benchmark on this.
     */
    u32 l, c;

    union {
        u64 int_value;
        char *str_value;
    };
};

const char *keywords[] = {
    "while",
    "func",
    "i8",
    "i16",
    "i32",
    "i64",
    "void",
    "return",

    "@c_function",
};

struct Lexer {
    char *source_text;
    char *buffer;
    Array<Token> tokens;
    u32 token_index = 0;
    Array<u32> line_offset;

    u32 l = 1, c = 0;

    Lexer(char *buffer);

    void tokenize();
    Token next_token();

    void advance(u32 count = 1);
    void report_error(const char *error_message);

    // for iterating thourgh the tokens array
    Token token();
    Token peek(u32 lookahead = 1);
    void eat();
    void expect(u32 type);
    void expect_and_eat(u32 type);

};

inline bool is_directive(Token t) {
    return (t.type >= Token::DIRECTIVE_START && t.type < Token::DIRECTIVE_END);
}

inline bool is_keyword(Token t) {
    return (t.type >= Token::KEYWORD_START && t.type < Token::KEYWORD_END);
}

internal char *get_object_filename(const char *input_filename);
