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

#define KEYWORDS(F)                                                            \
    F(KEYWORD_WHILE = KEYWORD_START, "while")                                  \
    F(KEYWORD_FUNC, "func")                                                    \
    F(KEYWORD_I8, "i8")                                                        \
    F(KEYWORD_I16, "i16")                                                      \
    F(KEYWORD_I32, "i32")                                                      \
    F(KEYWORD_I64, "i64")                                                      \
    F(KEYWORD_VOID, "void")                                                    \
    F(KEYWORD_RETURN, "return")                                                \
    F(KEYWORD_CAST, "cast")                                                    \
    F(DIRECTIVE_C_FUNCTION, "@c_function")

struct Token {
    enum Token_Type {
        END = 0,

        IDENTIFIER = 256,
        INTEGER,
        STRING,

        KEYWORD_START = 500,
#define EXPAND_KEYWORD_TOKEN(token, _) token,
        KEYWORDS(EXPAND_KEYWORD_TOKEN)
#undef EXPAND_KEYWORD_TOKEN
        KEYWORD_END,

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
#define EXPAND_KEYWORD_STRING(_, string) string,
        KEYWORDS(EXPAND_KEYWORD_STRING)
#undef EXPAND_KEYWORD_STRING
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

inline bool is_keyword(Token t) {
    return (t.type >= Token::KEYWORD_START && t.type < Token::KEYWORD_END);
}

internal char *get_object_filename(const char *input_filename);
