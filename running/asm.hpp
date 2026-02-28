#ifndef COPL_ASM_HPP
#define COPL_ASM_HPP

enum OpKind { FLOAT, INT };

enum Opcode : int {
    OP_LOAD_CONST,
    OP_LOAD_NULL,
    OP_LOAD_TRUE,
    OP_LOAD_FALSE,
    OP_LOAD_NAME,
    OP_SET_NAME,
    OP_LOAD_IMMEDIATLY,
    OP_COPY,
    OP_LOAD_FUNC_ADDR,
    OP_LOAD_MODULE_METHOD,
    OP_LOAD_MODULE,

    OP_POP,
    OP_DUP,
    OP_PUSH,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_LEFT,
    OP_RIGHT,
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_NOT,

    OP_EQ, // ==
    OP_NE, // !=
    OP_LT, // <
    OP_LE, // <=
    OP_GT, // >
    OP_GE, // >=

    OP_NOT,
    OP_AND,
    OP_OR,

    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,

    OP_GET_GLOBAL,
    OP_SET_GLOBAL,

    OP_CALL,
    OP_RETURN,
    OP_LEAVE,
    OP_SPECIAL_CALL,

    OP_NEW_ARRAY,
    OP_GET_ELEMENT,
    OP_SET_ELEMENT,
    OP_NEW_OBJECT,
    OP_MEMBER_GET,
    OP_MEMBER_SET,

    OP_PRINT,
    OP_HALT,
    OP_NOP,
    OP_ROT,
    OP_SWAP
};

#endif
