#include "il.h"

namespace IL {

    // @FIXME: do we really need to insert constants into basic blocks?
    Constant *Function::insert_constant(Basic_Block *bb, u64 value) {
        auto c = new Constant;
        c->n = value_count++;
        c->value = value;
        bb->instructions.push_back(c);

        return c;
    }

    Alloca *Function::insert_alloca(Basic_Block *bb, u32 size) {
        auto alloca = new Alloca;
        alloca->n    = value_count++;
        alloca->size = size;
        bb->instructions.push_back(alloca);

        return alloca;
    }

    Binary_Expression *Function::insert_binary(Basic_Block *bb, u32 op, Value *lhs, Value *rhs) {
        auto bi = new Binary_Expression;
        bi->n  = value_count++;
        bi->op  = op;
        bi->lhs = lhs;
        bi->rhs = rhs;
        bb->instructions.push_back(bi);

        return bi;
    }

    Unary_Expression *Function::insert_unary(Basic_Block *bb, u32 op, Value *operand) {
        auto un = new Unary_Expression;
        un->n  = value_count++;
        un->op = op;
        un->operand = operand;
        bb->instructions.push_back(un);

        return un;
    }

    // @note: remember to fill in the arguments
    // @TODO: note that we constructed arguements twice, which is stupid
    Function_Call *Function::insert_call(Basic_Block *bb, char *name, Array<Value *> *arguments) {
        auto call  = new Function_Call;
        call->n    = value_count++;
        call->name = name;

        for (auto arg : *arguments) {
            call->arguments.push_back(arg);
        }

        bb->instructions.push_back(call);

        return call;
    }

    Load *Function::insert_load(Basic_Block *bb, Value *base, Value *offset) {
        auto load    = new Load;
        load->n      = value_count++;
        load->base   = base;
        load->offset = offset;
        bb->instructions.push_back(load);

        return load;
    }

    Store *Function::insert_store(Basic_Block *bb, Value *source, Value *base, Value *offset) {
        auto store    = new Store;
        store->n      = value_count++;
        store->source = source;
        store->base   = base;
        store->offset = offset;
        bb->instructions.push_back(store);

        return store;
    }

    Branch *Function::insert_branch(Basic_Block *bb, Value *condition, Basic_Block *true_target, Basic_Block *false_target) {
        auto br          = new Branch;
        br->n            = value_count++;
        br->condition    = condition;
        br->true_target  = true_target;
        br->false_target = false_target;
        bb->instructions.push_back(br);

        return br;
    }

    Jump *Function::insert_jump(Basic_Block *bb, Basic_Block *target) {
        auto jmp    = new Jump;
        jmp->n      = value_count++;
        jmp->target = target;
        bb->instructions.push_back(jmp);

        return jmp;
    }

    Return *Function::insert_return(Basic_Block *bb, Value *return_value) {
        auto ret          = new Return;
        ret->n            = value_count++;
        ret->return_value = return_value;
        bb->instructions.push_back(ret);

        return ret;
    }

    struct Convert_Context {
        Function *f;
        Basic_Block *bb;
        AST::Scope *scope;
    };

    internal Value *convert_expression(Convert_Context *ctx, AST::Expreesion *expr, bool is_lvalue = false) {
        switch (expr->type) {

            case AST::INT_LITERAL: {
                auto lit = (AST::Int_Literal *) expr;
                // @TODO: reuse previously created constant if values are the same
                return ctx->f->insert_constant(ctx->bb, lit->value);
            }

            case AST::IDENTIFIER: {
                auto id = (AST::Identifier *) expr;

                // @TODO: handle global variable
                // @performance
                auto var = find_variable(ctx->scope, id->name);
                assert(var->address);

                if (is_lvalue) {
                    return var->address;
                } else {
                    return ctx->f->insert_load(ctx->bb, var->address);
                }
            }

            case AST::BINARY: {
                auto bi = (AST::Binary *) expr;

                // @TODO:
                // can we do the iterative version of this?
                // because recursion might blow up the stack!
                auto lhs = convert_expression(ctx, bi->lhs);
                auto rhs = convert_expression(ctx, bi->rhs);

                auto bi_value = ctx->f->insert_binary(ctx->bb, bi->op, lhs, rhs);

                return bi_value;

            }

            case AST::UNARY: {
                auto un = (AST::Unary *) expr;

                // @TODO: see the todo above
                auto operand = convert_expression(ctx, un->operand);

                auto un_value = ctx->f->insert_unary(ctx->bb, un->op, operand);

                return un_value;

            }

            case AST::FUNCTION_CALL: {
                auto call_ast = (AST::Function_Call *) expr;

                Array<Value *> arguments;

                for (auto arg_expr : call_ast->arguments) {
                    auto arg_value = convert_expression(ctx, arg_expr);
                    arguments.push_back(arg_value);
                }

                auto call = ctx->f->insert_call(ctx->bb, call_ast->name, &arguments);

                return call;

            }

            default: {
                assert(false && "Interal Compiler Error: converting unknown expreesion to intermediate language");
                return nullptr;
            }

        }
    }

    internal void convert_block(Convert_Context *ctx, AST::Block *block_ast) {

        auto old_scope = ctx->scope;
        ctx->scope = block_ast->scope;

        // @curious
        // How does this way of dynamic dispatching affters I$?
        // How can we profile it?
        for (auto stmt : block_ast->statements) {

            switch (stmt->type) {

                case AST::INT_LITERAL:
                case AST::IDENTIFIER:
                case AST::BINARY:
                case AST::UNARY:
                case AST::FUNCTION_CALL:
                    convert_expression(ctx, (AST::Expreesion *)stmt);
                    break;

                case AST::VARIABLE: {
                    auto var = (AST::Variable *) stmt;

                    // @TODO: allocate depending on size of the type
                    assert(var->address == nullptr);
                    auto alloca = ctx->f->insert_alloca(ctx->bb, 4);

                    if (var->initial_value) {
                        auto initial_value = convert_expression(ctx, var->initial_value);
                        ctx->f->insert_store(ctx->bb, initial_value, alloca);
                    }

                    var->address = alloca;
                } break;

                case AST::ASSIGN: {
                    auto assign = (AST::Assign *) stmt;

                    auto source = convert_expression(ctx, assign->rhs);
                    auto dest   = convert_expression(ctx, assign->lhs, true);

                    ctx->f->insert_store(ctx->bb, source, dest);

                } break;

                case AST::WHILE: {
                    auto wh = (AST::While *) stmt;

                    // @TODO: insert into blocks
                    auto header = ctx->f->insert_block();
                    ctx->f->insert_jump(ctx->bb, header);

                    ctx->bb = header;
                    auto cond = convert_expression(ctx, wh->condition);

                    auto body = ctx->f->insert_block();
                    ctx->bb = body;
                    convert_block(ctx, wh->body);
                    ctx->f->insert_jump(ctx->bb, header);

                    // @TODO: take care of the case if we have return in
                    // the loop
                    auto out = ctx->f->insert_block();
                    ctx->bb = out;

                    ctx->f->insert_branch(header, cond, body, out);

                } break;

                case AST::RETURN: {
                    auto ret_ast = (AST::Return *) stmt;

                    Value *return_value = nullptr;
                    if (ret_ast->return_value) {
                        return_value = convert_expression(ctx, ret_ast->return_value);
                    }

                    ctx->f->insert_return(ctx->bb, return_value);

                    // @TODO: halt conversion of the statements that follows

                } break;

                case AST::BLOCK: {
                    auto block_ast = (AST::Block *) stmt;

                    convert_block(ctx, block_ast);

                    // @TODO: handle return in block

                }

                default: {
                    assert(false && "Interal Compiler Error: converting unknown statement to intermidiate language.");
                }

            }
        }

        ctx->scope = old_scope;

    }

    internal Function *convert_function(Convert_Context *ctx, AST::Function *func_ast) {

        auto f = new Function;
        f->ast = func_ast;

        if (func_ast->body == nullptr) return f;

        auto entry_block = new Basic_Block;
        f->blocks.push_back(entry_block);

        ctx->bb = entry_block;
        ctx->f  = f;
        convert_block(ctx, func_ast->body);

        return f;

    }
    
    internal Module *convert_module(AST::Module *module_ast) {
        auto m = new Module;

        Convert_Context ctx;
        ctx.scope = module_ast->scope;

        // @TODO: get rid of std thing
        m->globals = std::move(module_ast->scope->variables);

        for (auto function_ast : module_ast->functions) {
            auto f = convert_function(&ctx, function_ast);
            m->functions.push_back(f);
        }

        return m;

    }

};

inline void print_value(IL::Value *v) {
    printf("$%u", v->n);
}

internal void print_il_module(IL::Module *module) {
    using namespace IL;

    for (u32 func_index = 0;
         func_index < module->functions.size();
         func_index++) {

        auto function = module->functions[func_index];
        printf("%s:\n", function->ast->name);

        for (u32 bb_index = 0;
             bb_index < function->blocks.size();
             bb_index++) {

            auto bb = function->blocks[bb_index];
            printf(".L%u_%u:\n", func_index, bb_index);

            for (auto I : bb->instructions) {
                printf("\t$%-3u ", I->n);
                if (auto c = I->as<Constant>()) {
                    printf("const\t%llu", c->value);
                } else if (auto alloca = I->as<Alloca>()) {
                    printf("alloca\t%uB", alloca->size);
                } else if (auto bi = I->as<Binary_Expression>()) {
                    printf("binary\t");
                    const char *op;
                    switch (bi->op) {
                        case '+': op = "+"; break;
                        case '-': op = "-"; break;
                        case '/': op = "/"; break;
                        case '*': op = "*"; break;
                        case '<': op = "<"; break;
                        default: assert(false && "Interal Compiler Error: printing unknown binary operator");
                    }
                    print_value(bi->lhs);
                    printf(" %s ", op);
                    print_value(bi->rhs);
                } else if (auto un = I->as<Unary_Expression>()) {
                    printf("unary\t");
                    const char *op;
                    switch (un->op) {
                        case '+': op = "+"; break;
                        case '-': op = "-"; break;
                        default: assert(false && "Interal Compiler Error: printing unknown unary operator");
                    }
                    printf("%s", op);
                    print_value(un->operand);
                } else if (auto call = I->as<Function_Call>()) {
                    printf("call\t%s", call->name);

                    printf("(");
                    for (u32 arg_index = 0;
                         arg_index < call->arguments.size();
                         arg_index++) {
                        auto arg = call->arguments[arg_index];
                        print_value(arg);

                        if (arg_index != call->arguments.size() - 1) {
                            printf(", ");
                        }
                    }
                    printf(")");

                } else if (auto load = I->as<Load>()) {
                    printf("load\t");
                    print_value(load->base);
                } else if (auto store = I->as<Store>()) {
                    printf("store\t");
                    print_value(store->source);
                    printf(" -> ");
                    print_value(store->base);
                } else if (auto br = I->as<Branch>()) {
                    printf("br \t");
                    print_value(br->condition);

                    printf(" ? .L_%u_%u : .L_%u_%u",
                            func_index,
                            br->true_target->i,
                            func_index,
                            br->false_target->i);

                } else if (auto jmp = I->as<Jump>()) {
                    printf("jmp\t.L_%u_%u", func_index, jmp->target->i);
                } else if (auto ret = I->as<Return>()) {
                    printf("ret");

                    if (ret->return_value) {
                        printf("\t");
                        print_value(ret->return_value);
                    }

                }

                printf("\n");
            }

        }
    }
}
