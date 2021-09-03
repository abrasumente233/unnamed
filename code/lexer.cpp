
char *read_entire_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    assert(length != -1);
    rewind(fp);

    char *buffer = (char *)malloc(length+1);

    if (!buffer) {
        return nullptr;
    }

    if (fread(buffer, 1, length, fp) == 0) {
        return nullptr;
    }

    buffer[length] = '\0';

    fclose(fp);

    return buffer;
}

Token make_token(u32 type, u32 l, u32 c) {
    Token t;
    t.l = l;
    t.c = c;
    t.type = type;
    return t;
}

bool is_space(char c) {
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\v') || (c == '\f');
}

bool starts_ident(char c) {
    return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool continues_ident(char c) {
    return starts_ident(c) || (c >= '0' && c <= '9');
}

Lexer::Lexer(char *b) {
    buffer = source_text = b;
}

void Lexer::tokenize() {
    char *b = buffer;

    // compute offsets for the start of each line
    u32 count = 0;
    line_offset.push_back(0);
    while (*b) {
        if (*b == '\n') {
            line_offset.push_back(count+1);
        }
        count++;
        b++;
    }

    while (true) {
        Token t = next_token();
        tokens.push_back(t);
        if (t.type == Token::END) break;
    }
}

Token Lexer::next_token() {

    // skip comments and spaces
    while(true) {
        char *b = buffer;

        while(is_space(*b)) b++;

        if (*b == '/' && *(b+1) == '/') {
            while(*b != '\n') b++;
            b++; // skip \n
        }

        if (b == buffer) break;
        advance(b - buffer);
    }

    char ch = *buffer;

    if (ch >= '0' && ch <= '9') {
        Token i = make_token(Token::INTEGER, l, c);
        char *b;
        i.int_value = strtoul(buffer, &b, 10);
        advance(b - buffer);
        return i;
    } else if (starts_ident(ch)) {
        char *end = buffer;

        while (*end) {
            if (continues_ident(*end)) end++;
            else break;
        }

        size_t length = end - buffer;

        for (u32 i = 0; i < (Token::KEYWORD_END - Token::KEYWORD_START); i++) {
            // @Performance
            if (strncmp(buffer, keywords[i], strlen(keywords[i])) == 0) {
                buffer += length;
                return make_token(Token::KEYWORD_START + i, l, c);
            }
        }

        // otherwise it's an identifier
        assert(length != 0);
        auto ident = (char *)malloc(length+1);

        strncpy(ident, buffer, length);
        ident[length] = '\0';

        Token id = make_token(Token::IDENTIFIER, l, c);
        id.str_value = ident;

        advance(length);
        return id;

    } else if (*buffer == '-' && *(buffer+1) == '>') {
        advance(2);
        return make_token(Token::ARROW, l, c);
    } else {
        advance(1);
        return make_token((u32)ch, l, c);
    }
}

Token Lexer::token() {
    return tokens[token_index];
}

Token Lexer::peek(u32 lookahead) {
    u32 i = token_index + lookahead;

    if (i >= tokens.size()) {
        i = tokens.size() - 1;
    }

    return tokens[i];
}

void Lexer::eat() {
    token_index++;
}

void Lexer::expect(u32 type) {
    if (token().type != type) {
        // @TODO: error reporting
        report_error("unexpected token");
    }
}

void Lexer::expect_and_eat(u32 type) {
    expect(type);
    eat();
}

// @cleanup, the overhead for error reporting seems too heavy
void Lexer::advance(u32 count) {
    while (count--) {
        c++;
        if (*buffer == '\n') {
            l++; 
            c = 0;
        }
        buffer++;
    }
}

void Lexer::report_error(const char *error_message) {
    u32 error_l = token().l;
    u32 error_c = token().c;

    printf("first.un:%u ", error_l);
    red_text();
    printf("error: %s\n\n", error_message);
    normal_text();

    char *error_line = source_text + line_offset[error_l-1];
    while (*error_line != '\n' && *error_line != '\r') {
        printf("%c", *error_line++);
    }
    printf("\n");

    for (u32 i = 0; i < error_c; i++) {
        printf(" ");
    }
    printf("^");

    printf("\n");
    __debugbreak();
}

char *get_object_filename(const char *input_filename) {
    i32 len = strlen(input_filename);
    auto obj_filename = (char *) malloc(len + 1); // @Leak

    i32 i;
    for (i = len-1; i >= 0; i--) {
        if (input_filename[i] == '.') {
            break;
        }
    }

    strcpy(obj_filename, input_filename);
    obj_filename[i+1] = 'o';
    obj_filename[i+2] = '\0';

    return obj_filename;
}
