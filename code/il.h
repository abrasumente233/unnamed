
namespace llvm {
    struct Value;
};

namespace IL {

    /* @TODO
     * Do we really need a numbering for each Value?
     */
    struct Value {
        enum _Type {
            UNDEF,
            CONSTANT,
            ALLOCA,
            BINARY_EXPRESSION,
            UNARY_EXPRESSION,
            FUNCTION_CALL,
            LOAD,
            STORE,
            BRANCH,
            JUMP,
            RETURN
        } type;

        u32 n;

        /* @note
         * Paying 8 bytes to get O(1) lookup when looking for converted value
         * in LLVM Converter.
         */
        llvm::Value *llvm_value;

        template<typename VT>
        VT *as() {
            return (type == VT::TYPE) ? static_cast<VT *>(this) : nullptr;
        }
    };

    struct Basic_Block {
        u32 i; // index in function->blocks
        Array<Value *> instructions;
    };

    struct Constant : Value {
        static const Value::_Type TYPE = Value::CONSTANT;
        Constant() { type = Value::CONSTANT; }

        u64 value;
    };

    struct Alloca : Value {
        static const Value::_Type TYPE = Value::ALLOCA;
        Alloca() { type = Value::ALLOCA; }
        
        u32 size;
    };

    struct Binary_Expression : Value {
        static const Value::_Type TYPE = Value::BINARY_EXPRESSION;
        Binary_Expression() { type = Value::BINARY_EXPRESSION; }

        u32 op;

        Value *lhs, *rhs;
    };

    struct Unary_Expression : Value {
        static const Value::_Type TYPE = Value::UNARY_EXPRESSION;
        Unary_Expression() { type = Value::UNARY_EXPRESSION; }

        u32 op;

        Value *operand;
    };

    struct Function_Call : Value {
        static const Value::_Type TYPE = Value::FUNCTION_CALL;
        Function_Call() { type = Value::FUNCTION_CALL; }

        char *name;
        Array<Value *> arguments;
    };

    struct Load : Value {
        static const Value::_Type TYPE = Value::LOAD;
        Load() { type = Value::LOAD; }

        Value *base;
        Value *offset; // in bytes
    };

    struct Store : Value {
        static const Value::_Type TYPE = Value::STORE;
        Store() { type = Value::STORE; }

        Value *base;
        Value *offset; // in bytes
        Value *source;
    };

    struct Branch : Value {
        static const Value::_Type TYPE = Value::BRANCH;
        Branch() { type = Value::BRANCH; }

        Value *condition;
        Basic_Block *true_target,
                    *false_target;
    };

    struct Jump : Value {
        static const Value::_Type TYPE = Value::JUMP;
        Jump() { type = Value::JUMP; }

        Basic_Block *target;
    };

    struct Return : Value {
        static const Value::_Type TYPE = Value::RETURN;
        Return() { type = Value::RETURN; }

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
