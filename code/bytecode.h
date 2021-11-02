
namespace BC {

    enum Reg : u8 { r0, r1 };

    enum Type : u8 {
        PUSH_IMM,
        PUSH_REG,
        POP,
        ADD,
        SUB,
        MUL,
        DIV,
        NEGATE,
        INVALID = 0xff
    };

    struct Module {
        u8 *instructions;
        u8 *data;
        u32 main_offset; // offset of main into instructions array
    };

}
