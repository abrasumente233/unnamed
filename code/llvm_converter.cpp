
#include "llvm_converter.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

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

struct LLVM_Converter {
    LLVMContext *ctx;
    Module      *module;
    IRBuilder<> *builder;
    Array<BasicBlock *> blocks;
};

Value *get_previously_converted_value(LLVM_Converter *c, IL::Value *value_il) {

    assert(value_il);

    if (auto constant = value_il->as<IL::Constant>()) {
        return ConstantInt::get(*c->ctx, 
                APInt(32, constant->value, /* is signed */ true));
    }

    return value_il->llvm_value;

}

llvm::Type *convert_type(LLVM_Converter *c, AST::Type *type) {
    if (type->type == AST::Type::VOID) {
        return llvm::Type::getVoidTy(*c->ctx);
    } else if (type->type == AST::Type::INTEGER) {
        return llvm::Type::getInt32Ty(*c->ctx);
    } else {
        assert(false && "convering unknown type to LLVM IR");
        return nullptr;
    }
}

void convert_value(LLVM_Converter *c, Function *function, IL::Value *value_il) {

    // assert(value_il->llvm_value == nullptr);

    if (auto constant = value_il->as<IL::Constant>()) {

    } else if (auto alloca = value_il->as<IL::Alloca>()) {

        alloca->llvm_value = c->builder->CreateAlloca(
                   llvm::Type::getInt32Ty(*c->ctx),
                   ConstantInt::get(*c->ctx, APInt(32, (u64)alloca->size, false)));

    } else if (auto bi = value_il->as<IL::Binary_Expression>()) {

        Value *binary_value;
        auto lhs = get_previously_converted_value(c, bi->lhs);
        auto rhs = get_previously_converted_value(c, bi->rhs);

        switch (bi->op) {
            case '+': binary_value = c->builder->CreateAdd(lhs, rhs); break;
            case '-': binary_value = c->builder->CreateSub(lhs, rhs); break;
            case '*': binary_value = c->builder->CreateMul(lhs, rhs); break;
            //case '/': binary_value = c->builder->CreateAdd(lhs, rhs); break;
            case '<': binary_value = c->builder->CreateICmpSLT(lhs, rhs); break;
            default: assert(false && "converting unknown binary instruction to LLVM IR");
        }

        bi->llvm_value = binary_value;

    } else if (auto un = value_il->as<IL::Unary_Expression>()) {

        Value *unary_value;
        auto operand = get_previously_converted_value(c, un->operand);

        switch (un->op) {
            case '-': unary_value = c->builder->CreateSub(ConstantInt::get(*c->ctx, APInt(32, 0, true)), operand); break;
            case '+': unary_value = operand; break;
            default: assert(false && "converting unknown unary instruction to LLVM IR");
        }

        un->llvm_value = unary_value;

    } else if (auto call = value_il->as<IL::Function_Call>()) {
        Function *callee_function = c->module->getFunction(call->name);

        assert(callee_function);
        assert(call->arguments.size() == callee_function->arg_size());

        Array<Value *> arguments;
        for (auto arg_il : call->arguments) {
            auto arg_value = get_previously_converted_value(c, arg_il);
            arguments.push_back(arg_value);
        }

        call->llvm_value = c->builder->CreateCall(callee_function, arguments);

    } else if (auto load = value_il->as<IL::Load>()) {

        auto base = get_previously_converted_value(c, load->base);
        load->llvm_value = 
            c->builder->CreateLoad(llvm::Type::getInt32Ty(*c->ctx), base);

    } else if (auto store = value_il->as<IL::Store>()) {

        auto source = get_previously_converted_value(c, store->source);
        auto base   = get_previously_converted_value(c, store->base);
        store->llvm_value = c->builder->CreateStore(source, base);

    } else if (auto br = value_il->as<IL::Branch>()) {
        BasicBlock *true_target  = c->blocks[br->true_target->i];
        BasicBlock *false_target = c->blocks[br->false_target->i];

        auto cond = get_previously_converted_value(c, br->condition);

        c->builder->CreateCondBr(cond, true_target, false_target);
    } else if (auto jmp = value_il->as<IL::Jump>()) {
        BasicBlock *target = c->blocks[jmp->target->i];

        c->builder->CreateBr(target);
    } else if (auto ret = value_il->as<IL::Return>()) {
        
        Value *return_value = nullptr;
        if (ret->return_value) {
            return_value = get_previously_converted_value(c, ret->return_value);
        }

        c->builder->CreateRet(return_value);
    } else {
        assert(false && "converting unkonwn IL values to LLVM IR");
    }

}

Function *convert_function(LLVM_Converter *c, IL::Function *func_il) {
    auto arg_count = func_il->ast->func_type->arguments.size();
    Array<llvm::Type *> arg_type(arg_count, llvm::Type::getInt32Ty(*c->ctx));
    FunctionType *ft = FunctionType::get(
        convert_type(c, func_il->ast->func_type->return_type), arg_type, false);

    Function *f = Function::Create(ft, Function::ExternalLinkage, func_il->ast->name, c->module);

    if (func_il->ast->body == nullptr) {
        return f;
    }

    c->blocks.clear();
    for (auto bb_il : func_il->blocks) {
        auto bb = BasicBlock::Create(*c->ctx, "", f);
        c->blocks.push_back(bb);
    }

    for (u32 bb_index = 0;
         bb_index < func_il->blocks.size();
         bb_index++) {

        auto bb = c->blocks[bb_index];
        auto bb_il = func_il->blocks[bb_index];

        c->builder->SetInsertPoint(bb);

        for (auto instruction_il : bb_il->instructions) {
            convert_value(c, f, instruction_il);
        }
    }

    verifyFunction(*f);

    return f;
}

Module *convert_module(IL::Module *module_il) {

    // create a new context and module
    LLVM_Converter converter;
    converter.ctx     = new LLVMContext;
    converter.module  = new Module("unamed module", *converter.ctx);

    // create IR builder for the module
    converter.builder = new IRBuilder<>(*converter.ctx);

    // convert functions
    for (auto function_il : module_il->functions) {
        convert_function(&converter, function_il);
    }

    printf("\n\n");
    converter.module->print(errs(), nullptr);

    return converter.module;
}

// Returns true if succueed
bool emit_object_file(Module *llvm_module) {

    auto target_triple = sys::getDefaultTargetTriple();

    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

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
