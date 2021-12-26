
namespace AST {

struct Parser {

    Lexer *lexer;
    Module *module;
    Array<Scope *> scope_stack;

    Type *parse_type() {
        Token t = lexer->token();
        lexer->eat();
        switch(t.type) {
            case Token::KEYWORD_VOID: return new Void_Type;
            case Token::KEYWORD_I8  : return new Integer_Type(1);
            case Token::KEYWORD_I16 : return new Integer_Type(2);
            case Token::KEYWORD_I32 : return new Integer_Type(4);
            case Token::KEYWORD_I64 : return new Integer_Type(8);
            default: lexer->report_error("unknown type"); return nullptr;
        }
    }

    Expreesion *parse_unary() {
        
        u32 op = lexer->token().type;
        u32 prec;
        bool is_unary_op = true;

        // @TODO: cleanup the prec
        switch(op) {
            case '+': prec = 15; break;
            case '-': prec = 15; break;
            default: is_unary_op = false;
        }

        if (is_unary_op) {
            lexer->eat();

            auto un = new Unary;
            un->op      = op;
            un->operand = parse_expression(prec);
            assert(un->operand);

            return un;
        } else if (lexer->token().type == Token::IDENTIFIER && 
                   lexer->peek().type == '(') {
            auto call = parse_function_call();

            return call;
        } else if (lexer->token().type == Token::IDENTIFIER) {
            auto id = new Identifier;
            id->name = lexer->token().str_value;

            lexer->eat();

            return id;
        } else if (lexer->token().type == Token::INTEGER) {
            auto lit = new Int_Literal;
            lit->value = lexer->token().int_value;

            lexer->eat();

            return lit;
        } else if (lexer->token().type == '(') {
            lexer->eat();
            auto expr = parse_expression();
            assert(expr);
            lexer->expect_and_eat(')');

            return expr;
        } else if (lexer->token().type == Token::KEYWORD_CAST) {
            lexer->eat();
            auto cast = new Cast;
            lexer->expect_and_eat('(');
            cast->to_type = parse_type();
            lexer->expect_and_eat(')');
            cast->expr = parse_unary();
            
            return cast;
        } else {
            return nullptr;
        }
    }

    Expreesion *parse_expression(u32 min_prec = 1) {
        auto lhs = parse_unary();

        if (!lhs) return nullptr;

        while (true) {

            u32 op = lexer->token().type;
            u32 prec;
            bool left_assoc = true;

            bool is_binary_op = true;

            switch(op) {
                case '<':
                case '>': prec = 1; break;

                case '+':
                case '-': prec = 2; break;

                case '*':
                case '/':
                case '%': prec = 3; break;

                default: is_binary_op = false;
            }

            if (!is_binary_op) break;

            if (prec < min_prec) break;

            lexer->eat();

            u32 next_prec = left_assoc ? (prec+1) : prec;
            auto rhs = parse_expression(next_prec);
            assert(rhs);

            auto bi = new Binary;
            bi->op  = op;
            bi->lhs = lhs;
            bi->rhs = rhs;
            lhs = bi;
        }

        return lhs;
    }

    Function_Call *parse_function_call() {
        lexer->expect(Token::IDENTIFIER);

        auto call = new Function_Call;
        call->name = lexer->token().str_value;

        lexer->eat(); // eats callee name
        lexer->expect_and_eat('(');

        while(true) {
            auto arg = parse_expression();
            assert(arg);
            call->arguments.push_back(arg);

            if (lexer->token().type == ',') {
                lexer->eat();
            } else if (lexer->token().type == ')') {
                break;
            } else {
                assert(false);
            }
        }

        lexer->expect_and_eat(')');

        return call;
    }

    Node *parse_statement() {

        if (lexer->peek().type == ':') {
            auto var = parse_variable();
            lexer->expect_and_eat(';');
            return var;
        } else if (lexer->token().type == Token::KEYWORD_RETURN) {
            lexer->eat();

            auto ret = new Return;
            ret->return_value = nullptr;

            if (lexer->token().type != ';') {
                ret->return_value = parse_expression();
                assert(ret->return_value);
            }

            lexer->expect_and_eat(';');
            return ret;
        } else if (lexer->token().type == Token::KEYWORD_WHILE) {
            lexer->eat();

            auto wh = new While;
            wh->condition = parse_expression();
            wh->body = parse_block();

            return wh;
        } else {
            auto expr = parse_expression();
            if (!expr) return nullptr;

            // is an assignment?
            if (lexer->token().type == '=') {
                lexer->eat();
                auto assign = new Assign;
                assign->lhs = expr;
                assign->rhs = parse_expression();

                lexer->expect_and_eat(';');
                return assign;
            } else {
                lexer->expect_and_eat(';');
                return expr;
            }
        }
    }

    Block *parse_block(Scope *scope = nullptr) {
        lexer->expect_and_eat('{');
        
        auto block = new Block;
        block->scope = scope;
        if (!block->scope) {
            block->scope = new Scope;
            block->scope->parent = scope_stack.back();
        }

        scope_stack.push_back(block->scope);
        while(auto stmt = parse_statement()) {
            block->statements.push_back(stmt);
        }
        scope_stack.pop_back();

        lexer->expect_and_eat('}');

        return block;
    }

    Variable *parse_variable() {
        lexer->expect(Token::IDENTIFIER);

        assert(scope_stack.size() != 0);
        auto scope = scope_stack.back();

        auto variable = new Variable;
        scope->variables.push_back(variable);
        variable->name = lexer->token().str_value;
        variable->initial_value = nullptr;

        lexer->eat();

        lexer->expect_and_eat(':');
        variable->var_type = parse_type();

        if (lexer->token().type == '=') {
            lexer->eat();
            variable->initial_value = parse_expression();
            assert(variable->initial_value);
        }

        return variable;
    }

    Function *parse_function() {
        lexer->expect_and_eat(Token::KEYWORD_FUNC);

        auto func = new Function;

        if (is_keyword(lexer->token())) {
            while (true) {
                Token t = lexer->token();
                if (t.type == Token::DIRECTIVE_C_FUNCTION) {
                    lexer->eat();
                    func->is_c_function = true;
                } else if (t.type == Token::IDENTIFIER) {
                    break;
                } else {
                    lexer->report_error("expected function attributes (directives) or function name");
                }
            }
        }

        lexer->expect(Token::IDENTIFIER);

        func->name = lexer->token().str_value;

        lexer->eat();
        lexer->expect_and_eat('(');

        auto func_type = new Function_Type;

        auto func_scope = new Scope;
        func_scope->parent = scope_stack.back();
        scope_stack.push_back(func_scope);

        while (true) {
            auto arg = parse_variable();
            func->arguments.push_back(arg);
            func_type->arguments.push_back(arg->var_type); // @cleanup

            if (lexer->token().type == ',') {
                lexer->eat();
            } else if (lexer->token().type == ')') {
                break;
            } else {
                assert(false);
            }
        }

        scope_stack.pop_back();

        lexer->expect_and_eat(')');
        lexer->expect_and_eat(Token::ARROW);

        func_type->return_type = parse_type();
        func->func_type = func_type;

        if (lexer->token().type == '{') {
            func->body = parse_block(func_scope);
        } else if (lexer->token().type == ';') {
            lexer->eat();
            func->body = nullptr;
        } else {
            assert(false);
        }

        return func;
    }

    Module *parse_module(Lexer *l) {

        assert(scope_stack.size() == 0);

        lexer = l;
        module = new Module;
        module->scope = new Scope;
        module->scope->parent = nullptr;
        scope_stack.push_back(module->scope);

        while (true) {
            if (lexer->token().type == Token::KEYWORD_FUNC) {
                auto func = parse_function();
                module->functions.push_back(func);
            } else if (lexer->token().type == Token::IDENTIFIER) {
                auto var = parse_variable();
                module->scope->variables.push_back(var);
            } else {
                break;
            }
        }

        scope_stack.pop_back();

        return module;

    }

};

};

// @performance
// Checking for re-declaration is O(n^2) with a large coefficient,
// since we're using strcmp.
AST::Variable *find_variable_in_scope(AST::Scope *scope, char *name) {
    for (auto var : scope->variables) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
    }

    return nullptr;
}

// Find variable, walking up the chain of scopes
// @performance sucks
AST::Variable *find_variable(AST::Scope *scope, char *name) {
    while (scope) {
        if (auto var = find_variable_in_scope(scope, name)) {
            return var;
        }

        scope = scope->parent;
    }

    return nullptr;
}

