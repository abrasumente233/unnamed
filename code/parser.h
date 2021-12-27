
// @TODO: only create new types when there are actually new types
// maybe use a type pool.

namespace IL {
    struct Value;
}

namespace AST {

    struct Type {
        enum _Type : u32 {
            VOID,
            INTEGER,
            FUNCTION
        } type;
    };

    struct Void_Type : Type {
        Void_Type() { type = VOID; }
    };

    struct Integer_Type : Type {
        u8 is_unsigned;
        u8 size; // size in bytes

        Integer_Type(u8 size, bool is_unsigned = false)
            : size(size), is_unsigned(is_unsigned) {
            type = INTEGER;
        }
    };

    struct Function_Type : Type {
        Function_Type() { type = FUNCTION; }

        Array<Type *> arguments;
        Type *return_type;
    };

    enum {
        INT_LITERAL,
        IDENTIFIER,
        BINARY,
        UNARY,
        CAST,
        FUNCTION_CALL,
        VARIABLE,
        FUNCTION,
        ASSIGN,
        WHILE,
        RETURN,
        BLOCK,
        MODULE
    };

    struct Node {
        u32 type;
    };

    // @TODO: Fix typo
    struct Expreesion : Node {
        Type *inferred_type;
    };

    struct Int_Literal : Expreesion {
        Int_Literal() { type = INT_LITERAL; }

        u64 value;
    };

    struct Identifier : Expreesion {
        Identifier() { type = IDENTIFIER; }

        char *name;
    };

    struct Binary : Expreesion {
        Binary() { type = BINARY; }

        u32 op;
        Expreesion *lhs, *rhs;
    };

    struct Unary : Expreesion {
        Unary() { type = UNARY; }

        u32 op;
        Expreesion *operand;
    };

    struct Cast : Expreesion {
        Cast() { type = CAST; }

        // @Naming
        Type *to_type;
        Expreesion *expr;
    };

    struct Function_Call : Expreesion {
        Function_Call() { type = FUNCTION_CALL; }

        char *name;
        Array<Expreesion *> arguments;
    };

    struct Variable : Node {
        Variable() { type = VARIABLE; }

        Type *var_type;
        char *name;
        Expreesion *initial_value;

        // Stack address for variable?
        // for IL and LLVM conversion
        // What about globals?
        void *address = nullptr;
    };

    struct Assign : Node {
        Assign() { type = ASSIGN; }

        Expreesion *lhs, *rhs;
    };

    struct Scope : Node {
        Scope *parent;
        Array<Variable *> variables;
    };

    struct Block : Node {
        Block() { type = BLOCK; }

        Scope *scope;
        Array<Node *> statements;
    };

    struct While : Node {
        While() { type = WHILE; }

        Expreesion *condition;
        Block *body;
    };

    struct Return : Node {
        Return() { type = RETURN; }

        Expreesion *return_value;
    };

    struct Function : Node {
        Function() { type = FUNCTION; }

        // @TODO: pack bools into an int
        bool is_c_function = false;

        Function_Type *func_type;
        Array<Variable *> arguments;
        char *name;
        Block *body;
    };

    struct Module : Node {
        Module() { type = MODULE; }

        Scope *scope;
        Array<Function *> functions;
    };

};

AST::Variable *find_variable_in_scope(AST::Scope *scope, char *name);
AST::Variable *find_variable(AST::Scope *scope, char *name);
