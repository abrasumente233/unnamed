
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

namespace llvm_conv {

using namespace llvm;
using namespace llvm::sys;

// perform typical -O2 optimization pipeline on a module
// Ref: https://llvm.org/docs/NewPassManager.html#just-tell-me-how-to-run-the-default-optimization-pipeline-with-the-new-pass-manager
internal void optimize_module(Module *module, u32 optimization_level) {

    // create the analysis managers
    LoopAnalysisManager     LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager    CGAM;
    ModuleAnalysisManager   MAM;

    PassBuilder PB;

    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    OptimizationLevel llvm_optimization_level;
    switch (optimization_level) {
        case 1: llvm_optimization_level = OptimizationLevel::O1; break;
        case 2: llvm_optimization_level = OptimizationLevel::O2; break;
        case 3: llvm_optimization_level = OptimizationLevel::O3; break;
        default: assert(false && "unknown optimization level");
    }
    ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm_optimization_level);

    MPM.run(*module, MAM);
}

struct LLVM_Converter {
    LLVMContext *ctx;
    Module      *module;
    Function    *function;
    IRBuilder<> *builder;
    AST::Scope  *scope;
    Array<BasicBlock *> blocks;
};

internal llvm::Type *convert_type(LLVM_Converter *c, AST::Type *type) {
    if (type->type == AST::Type::VOID) {
        return llvm::Type::getVoidTy(*c->ctx);
    } else if (type->type == AST::Type::INTEGER) {
        auto int_type = (AST::Integer_Type *) type;
        return llvm::IntegerType::get(*c->ctx, int_type->size * 8);
    } else {
        assert(false && "converting unknown type to LLVM IR");
        return nullptr;
    }
}

internal llvm::Value *convert_expression(LLVM_Converter *c,
                                         AST::Expreesion *expr,
                                         bool is_lvalue = false) {

    switch (expr->type) {

        case AST::INT_LITERAL: {
            auto lit = (AST::Int_Literal *) expr;
            // @TODO: reuse previously created constant if values are the same
            //return ctx->f->insert_constant(ctx->bb, lit->value);
            return ConstantInt::get(
                *c->ctx, APInt(32, lit->value, /* is signed */ true));
        }

        case AST::IDENTIFIER: {
            auto id = (AST::Identifier *) expr;

            // @TODO: handle global variable
            // @performance
            auto var = find_variable(c->scope, id->name);
            assert(var->address);

            auto address = (llvm::Value *)var->address;

            if (is_lvalue) {
                return address;
            } else {
                return c->builder->CreateLoad(llvm::Type::getInt32Ty(*c->ctx),
                                              address);
            }

        }

        case AST::BINARY: {
            auto bi = (AST::Binary *) expr;

            // @TODO:
            // can we do the iterative version of this?
            // because recursion might blow up the stack!
            auto lhs = convert_expression(c, bi->lhs);
            auto rhs = convert_expression(c, bi->rhs);

            llvm::Value *binary_value;
            switch (bi->op) {
                case '+': binary_value = c->builder->CreateAdd(lhs, rhs); break;
                case '-': binary_value = c->builder->CreateSub(lhs, rhs); break;
                case '*': binary_value = c->builder->CreateMul(lhs, rhs); break;
                //case '/': binary_value = c->builder->CreateSDiv(lhs, rhs); break;
                case '<': binary_value = c->builder->CreateICmpSLT(lhs, rhs); break;
                default: assert(false && "converting unknown binary instruction to LLVM IR");
            }

            return binary_value;

        }

        case AST::UNARY: {
            auto un = (AST::Unary *) expr;

            // @TODO: see the todo above
            auto operand = convert_expression(c, un->operand);

            llvm::Value *unary_value;
            switch (un->op) {
                case '-': unary_value = c->builder->CreateSub(ConstantInt::get(*c->ctx, APInt(32, 0, true)), operand); break;
                case '+': unary_value = operand; break;
                default: assert(false && "converting unknown unary instruction to LLVM IR");
            }

            return unary_value;

        }

        case AST::FUNCTION_CALL: {
            auto call_ast = (AST::Function_Call *) expr;
            auto callee_function = c->module->getFunction(call_ast->name);

            assert(callee_function);
            assert(call_ast->arguments.size() == callee_function->arg_size());

            Array<llvm::Value *> arguments;
            for (auto arg_expr : call_ast->arguments) {
                auto arg_value = convert_expression(c, arg_expr);
                arguments.push_back(arg_value);
            }

            return c->builder->CreateCall(callee_function, arguments);
        }

        default: {
            assert(false && "Interal Compiler Error: converting unknown expreesion to intermediate language");
            return nullptr;
        }

    }
}

internal void convert_block(LLVM_Converter *c, AST::Block *block_ast) {

    auto old_scope = c->scope;
    c->scope = block_ast->scope;

    // @curious
    // How does this way of dynamic dispatching affects I$?
    // How can we profile it?
    for (auto stmt : block_ast->statements) {

        switch (stmt->type) {

            case AST::INT_LITERAL:
            case AST::IDENTIFIER:
            case AST::BINARY:
            case AST::UNARY:
            case AST::FUNCTION_CALL:
                convert_expression(c, (AST::Expreesion *)stmt);
                break;

            case AST::VARIABLE: {
                auto var = (AST::Variable *) stmt;

                // @TODO: allocate depending on size of the type
                assert(var->address == nullptr);
                auto alloca = c->builder->CreateAlloca(
                    llvm::Type::getInt32Ty(*c->ctx), nullptr);

                if (var->initial_value) {
                    auto initial_value = convert_expression(c, var->initial_value);
                    c->builder->CreateStore(initial_value, alloca);
                }

                var->address = (void *)alloca;
            } break;

            case AST::ASSIGN: {
                auto assign = (AST::Assign *) stmt;

                auto source = convert_expression(c, assign->rhs);
                auto dest   = convert_expression(c, assign->lhs, true);

                c->builder->CreateStore(source, dest);
            } break;

            case AST::WHILE: {
                auto wh = (AST::While *) stmt;

                auto header = BasicBlock::Create(*c->ctx, "", c->function);
                auto out    = BasicBlock::Create(*c->ctx, "", c->function);
                auto body   = BasicBlock::Create(*c->ctx, "", c->function);

                c->builder->CreateBr(header);

                c->builder->SetInsertPoint(header);
                auto cond = convert_expression(c, wh->condition);
                c->builder->CreateCondBr(cond, body, out);

                c->builder->SetInsertPoint(body);
                convert_block(c, wh->body);
                c->builder->CreateBr(header);

                // @TODO: take care of the case if we have return in
                // the loop

                c->builder->SetInsertPoint(out);

            } break;

            case AST::RETURN: {
                auto ret_ast = (AST::Return *) stmt;

                llvm::Value *return_value = nullptr;
                if (ret_ast->return_value) {
                    return_value = convert_expression(c, ret_ast->return_value);
                }

                c->builder->CreateRet(return_value);

                // @TODO: halt conversion of the statements that follows

            } break;

            case AST::BLOCK: {
                auto block_ast = (AST::Block *) stmt;

                convert_block(c, block_ast);

                // @TODO: handle return in block

            }

            default: {
                assert(false && "Interal Compiler Error: converting unknown statement to intermidiate language.");
            }

        }
    }

    c->scope = old_scope;

}

internal Function *convert_function(LLVM_Converter *c, AST::Function *func_ast) {
    auto arg_count = func_ast->func_type->arguments.size();

    //Array<llvm::Type *> arg_type(arg_count, llvm::Type::getInt32Ty(*c->ctx));
    Array<llvm::Type *> arg_type;
    for (auto arg : func_ast->func_type->arguments) {
        arg_type.push_back(convert_type(c, arg));
    }

    FunctionType *ft = FunctionType::get(
        convert_type(c, func_ast->func_type->return_type), arg_type, false);

    c->function = Function::Create(ft, Function::ExternalLinkage,
                                   func_ast->name, c->module);

    if (func_ast->body == nullptr) {
        return c->function;
    }

    auto entry = BasicBlock::Create(*c->ctx, "", c->function);
    c->builder->SetInsertPoint(entry);

    convert_block(c, func_ast->body);

    verifyFunction(*c->function);

    return c->function;
}

internal Module *convert_module(AST::Module *module_ast) {

    // create a new context and module
    LLVM_Converter converter;
    converter.ctx     = new LLVMContext;
    converter.module  = new Module("unamed module", *converter.ctx);
    converter.scope   = module_ast->scope;

    // create IR builder for the module
    converter.builder = new IRBuilder<>(*converter.ctx);

    // @FIXME: Deal with global variables!
    // m->globals = std::move(module_ast->scope->variables);

    // convert functions
    for (auto function_ast : module_ast->functions) {
        convert_function(&converter, function_ast);
    }

    // @FIXME: DONT DEPEND ON GLOBAL VARIABLE
    if (options.optimization_level != 0) {
        optimize_module(converter.module, options.optimization_level);
    }

    converter.module->print(errs(), nullptr);

    return converter.module;
}

// Returns true if succueed
internal bool emit_object_file(Module *llvm_module) {

    auto target_triple = sys::getDefaultTargetTriple();

    InitializeNativeTarget();
    //InitializeNativeTargetAsmParser();
    InitializeNativeTargetAsmPrinter();

    std::string error;
    auto target = TargetRegistry::lookupTarget(target_triple, error);

    if (!target) {
        errs() << error;
        return false;
    }

    auto cpu = "generic";
    auto features = "";

    TargetOptions opt;
    auto rm = Optional<Reloc::Model>();
    auto target_machine =
        target->createTargetMachine(target_triple, cpu, features, opt, rm);

    // @note(44)
    // According to the frontend performance guide,
    // optimizations benefit from knowning about the target and data layout.
    // @TODO: remember to do optimizations after setting this!
    llvm_module->setDataLayout(target_machine->createDataLayout());
    llvm_module->setTargetTriple(target_triple);

    // @FIXME: DONT DEPEND ON GLOBAL VARIABLE
    const char *obj_filename;
    if (options.output_filename) {
        obj_filename = options.output_filename;
    } else {
        obj_filename = get_object_filename(options.input_filename);
    }

    std::error_code ec;
    raw_fd_ostream dest(obj_filename, ec, sys::fs::OF_None);

    if (ec) {
        errs() << "Could not open file: " << ec.message();
        return false;
    }

    legacy::PassManager pm;

    if (target_machine->addPassesToEmitFile(pm, dest, nullptr, CGFT_ObjectFile)) {
        errs() << "TargetMachine can't emit a file of this type";
        return false;
    }

    pm.run(*llvm_module);
    dest.flush();

    return true;
}

};
