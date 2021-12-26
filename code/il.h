
namespace llvm {
    struct Value;
};

namespace IL {

    /* @TODO
     * Do we really need a numbering for each Value?
     */
    struct Value {
        enum _Tag {
            UNDEF,
            CONSTANT,
            ALLOCA,
            BINARY_EXPRESSION,
            UNARY_EXPRESSION,
            INT_ZERO_EXTEND,
            INT_SIGN_EXTEND,
            INT_TRUNCATE,
            FUNCTION_CALL,
            LOAD,
            STORE,
            BRANCH,
            JUMP,
            RETURN
        } tag;

        u32 n;

        /* @note
         * Paying 8 bytes to get O(1) lookup when looking for converted value
         * in LLVM Converter.
         */
        llvm::Value *llvm_value;

        template<typename VT>
        VT *as() {
            return (tag == VT::TAG) ? static_cast<VT *>(this) : nullptr;
        }
    };

    struct Basic_Block {
        u32 i; // index in function->blocks
        Array<Value *> instructions;
    };

    struct Constant : Value {
        static const Value::_Tag TAG = Value::CONSTANT;
        Constant() { tag = Value::CONSTANT; }

        u64 value;
    };

    struct Alloca : Value {
        static const Value::_Tag TAG = Value::ALLOCA;
        Alloca() { tag = Value::ALLOCA; }
        
        u32 size;
    };

    struct Binary_Expression : Value {
        static const Value::_Tag TAG = Value::BINARY_EXPRESSION;
        Binary_Expression() { tag = Value::BINARY_EXPRESSION; }

        u32 op;

        Value *lhs, *rhs;
    };

    struct Unary_Expression : Value {
        static const Value::_Tag TAG = Value::UNARY_EXPRESSION;
        Unary_Expression() { tag = Value::UNARY_EXPRESSION; }

        u32 op;

        Value *operand;
    };

    struct Int_Zero_Extend : Value {
        static const Value::_Tag TAG = Value::INT_ZERO_EXTEND;
        Int_Zero_Extend() { tag = Value::INT_ZERO_EXTEND; }

        Value *operand;
    };

    struct Function_Call : Value {
        static const Value::_Tag TAG = Value::FUNCTION_CALL;
        Function_Call() { tag = Value::FUNCTION_CALL; }

        char *name;
        Array<Value *> arguments;
    };

    struct Load : Value {
        static const Value::_Tag TAG = Value::LOAD;
        Load() { tag = Value::LOAD; }

        Value *base;
        Value *offset; // in bytes
    };

    struct Store : Value {
        static const Value::_Tag TAG = Value::STORE;
        Store() { tag = Value::STORE; }

        Value *base;
        Value *offset; // in bytes
        Value *source;
    };

    struct Branch : Value {
        static const Value::_Tag TAG = Value::BRANCH;
        Branch() { tag = Value::BRANCH; }

        Value *condition;
        Basic_Block *true_target,
                    *false_target;
    };

    struct Jump : Value {
        static const Value::_Tag TAG = Value::JUMP;
        Jump() { tag = Value::JUMP; }

        Basic_Block *target;
    };

    struct Return : Value {
        static const Value::_Tag TAG = Value::RETURN;
        Return() { tag = Value::RETURN; }

        Value *return_value;
    };

    struct Function {
        AST::Function *ast;
        Array<Basic_Block *> blocks;

        u32 value_count = 0;

        Basic_Block *insert_block() {
            auto bb = new Basic_Block;
            bb->i   = blocks.size();
            blocks.push_back(bb);
            return bb;
        }

        // @cleanup, FIXME
        Constant *insert_constant(Basic_Block *bb, u64 value);
        Alloca *insert_alloca(Basic_Block *bb, u32 size);
        Binary_Expression *insert_binary(Basic_Block *bb, u32 op, Value *lhs, Value *rhs);
        Unary_Expression *insert_unary(Basic_Block *bb, u32 op, Value *operand);
        Function_Call *insert_call(Basic_Block *bb, char *name, Array<Value *> *arguments);
        Load *insert_load(Basic_Block *bb, Value *base, Value *offset = nullptr);
        Store *insert_store(Basic_Block *bb, Value *source, Value *base, Value *offset = nullptr);
        Branch *insert_branch(Basic_Block *bb, Value *condition, Basic_Block *true_target, Basic_Block *false_target);
        Jump *insert_jump(Basic_Block *bb, Basic_Block *target);
        Return *insert_return(Basic_Block *bb, Value *return_value = nullptr);
    };

    struct Module {
        Array<AST::Variable *> globals; // @FIXME
        Array<Function *> functions;
    };

};

internal void print_il_module(IL::Module *module);
