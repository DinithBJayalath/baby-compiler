#include "ast.h"
#include "lexer.cpp"

static int curTok;

static int getNextTok() {
    return curTok = getTok();
}

std::unique_ptr<ExprAST> logErr(const char *Str) {
    fprintf(stderr, "Error : %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> logErrP(const char *Str) {
    fprintf(stderr, "Error : %s\n", Str);
    return nullptr;
}