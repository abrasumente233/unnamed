
namespace BC {

struct BC_Converter {
    Module *module;
    u8 *cursor; // current bytecode instruction being code generated
    AST::Scope *scope;
};

void push_immediate(BC_Converter *c, u32 imm) {
    *c->cursor++ = PUSH_IMM;
    *((u32 *)(c->cursor)) = imm;
    c->cursor += 4;
}

void push_register(BC_Converter *c, u8 reg) {
    *c->cursor++ = PUSH_REG;
    *c->cursor++ = reg;
}

void pop(BC_Converter *c, u8 reg) {
    *c->cursor++ = POP;
    *c->cursor++ = reg;
}

void convert_expression(BC_Converter *converter, AST::Expression *expr, bool is_lvalue = false) {
    switch (expr->type) {

        case AST::INT_LITERAL: {
            auto lit = (AST::Int_Literal *) expr;
            push_immediate(converter, lit->value);
        } break;

        case AST::IDENTIFIER: {
            auto id = (AST::Identifier *) expr;

            // @TODO: handle global variable
            // @performance
            auto var = find_variable(ctx->scope, id->name);

            ldr(r1, var->offest);

            assert(var->address);

            if (is_lvalue) {
                return var->address;
            } else {
                return ctx->f->insert_load(ctx->bb, var->address);
            }
        } break;

        case AST::BINARY: {
            auto bi = (AST::Binary *) expr;

            convert_expression(ctx, bi->lhs);
            convert_expression(ctx, bi->rhs);

            pop(r0);
            pop(r1);

            u8 bc_op;
            switch (bi->op) {
                case '+': bc_op = ADD; break;
                case '-': bc_op = SUB; break;
                case '*': bc_op = MUL; break;
                case '/': bc_op = DIV; break;
                default: assert(false && "converting unknown binary operator to bytecode");
            }
            *(c->cursor)++ = bc_op;
            push_register(r0);

        } break;

        case AST::UNARY: {
            auto un = (AST::Unary *) expr;

            convert_expression(ctx, un->operand);

            if (un->op == '+') return;

            pop(r0);
            u8 bc_op;
            switch (un->op) {
                case '-': bc_op = NEGATE; break;
                default: assert(false && "converting unknown unary operator to bytecode");
            }
            *(c->cursor)++ = bc_op;
            push_register(r0);

        } break;

        case AST::FUNCTION_CALL: {
            auto call_ast = (AST::Function_Call *) expr;

            if (call_ast->arguments.size() != 0) {
                for (i32 arg_index = call_ast->arguments.size()-1; // @FIXME: overflow!!!!!
                     arg_index >= 0;
                     arg_index--) {
                    convert_expression(ctx, call_ast->arguments[arg_index]);
                }
            }

            call();

        } break;

        default: {
            assert(false && "Interal Compiler Error: converting unknown expreesion to bytecode");
            return nullptr;
        }

    }


}

Module *convert_block(BC_Converter *converter, AST::Block *block_ast) {

    auto old_scope = converter->scope;
    converter->scope = block_ast->scope;

    for (auto stmt : block_ast->statements) {

        switch (stmt->type) {

            case AST::INT_LITERAL:
            case AST::IDENTIFIER:
            case AST::BINARY:
            case AST::UNARY:
            case AST::FUNCTION_CALL:
                convert_expression(converter, (AST::Expreesion *)stmt);
                break;

            case AST::VARIABLE: {
                assert(false && "Sorry! variables are not allowed at this moment.")
                auto var = (AST::Variable *) stmt;

                // @TODO: allocate depending on size of the type
                assert(var->address == nullptr);
                auto alloca = converter->f->insert_alloca(converter->bb, 4);

                if (var->initial_value) {
                    auto initial_value = convert_expression(converter, var->initial_value);
                    converter->f->insert_store(converter->bb, initial_value, alloca);
                }

                var->address = alloca;
            } break;

            case AST::ASSIGN: {
                assert(false && "Sorry! Assigning to variables are not allowed at this moment.")
                auto assign = (AST::Assign *) stmt;

                auto source = convert_expression(converter, assign->rhs);
                auto dest   = convert_expression(converter, assign->lhs, true);

                converter->f->insert_store(converter->bb, source, dest);

            } break;

            case AST::WHILE: {
                assert(false);
                auto wh = (AST::While *) stmt;

                // @TODO: insert into blocks
                auto header = converter->f->insert_block();
                converter->f->insert_jump(converter->bb, header);

                converter->bb = header;
                auto cond = convert_expression(converter, wh->condition);

                auto body = converter->f->insert_block();
                converter->bb = body;
                convert_block(converter, wh->body);
                converter->f->insert_jump(converter->bb, header);

                // @TODO: take care of the case if we have return in
                // the loop
                auto out = converter->f->insert_block();
                converter->bb = out;

                converter->f->insert_branch(header, cond, body, out);

            } break;

            case AST::RETURN: {
                push();
                auto ret_ast = (AST::Return *) stmt;

                Value *return_value = nullptr;
                if (ret_ast->return_value) {
                    return_value = convert_expression(converter, ret_ast->return_value);
                }

                converter->f->insert_return(converter->bb, return_value);

                // @TODO: halt conversion of the statements that follows

            } break;

            case AST::BLOCK: {
                auto block_ast = (AST::Block *) stmt;

                convert_block(converter, block_ast);

                // @TODO: handle return in block

            }

            default: {
                assert(false && "Interal Compiler Error: converting unknown statement to bytecode.");
            }


        }

    }

}

Module *convert_module(AST::Module *module_ast) {

    BC_Converter converter;

    auto m = new Module;
    m->instructions = new u8[1024];
    m->data         = new u8[1024];
    m->main_offset  = 0xffffffff;

    converter->module = m;
    converter->i      = 0;

    for (auto function_ast : module_ast->functions) {
        if (string_match(function_ast, "main")) {
            m->main_entry = converter->i;
        }

        switch (stmt)
    }

}

};
