#include "map"
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

static std::unique_ptr<ExprAST> parseExpression();

static std::unique_ptr<NumberExprAST> parseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(NumberVal);
    getNextTok();
    return std::move(result);
}

static std::unique_ptr<ExprAST> parseParenExpr() {
    getNextTok();
    auto v = parseExpression();
    if (!v) {return nullptr;}
    if (curTok != ')') {return logErr("Expected ')'");}
    getNextTok();
    return v;
}

static std::unique_ptr<ExprAST> parseIdentifierExpr() {
    std::string idStr = IdentifierStr;
    getNextTok();
    if (curTok != '(') {
        return std::make_unique<VariableExprAST>(IdentifierStr);
    }
    getNextTok();
    std::vector<std::unique_ptr<ExprAST>> args;
    if (curTok != ')') {
        while (true) {
            if (auto arg = parseExpression()) {
                args.push_back(std::move(arg));
            }
            else { return nullptr; }
            if (curTok == ')') { break; }
            if (curTok != ',') {
                return logErr("Expected ')' or ',' in the argument list");
            }
            getNextTok();
        }
    }
    getNextTok();
    return std::make_unique<CallExprAST>(idStr, std::move(args));
}

static std::unique_ptr<ExprAST> primeryParser() {
    switch (curTok) {
        default:
            return logErr("Unknown token when expecting an expression");
        case tk_indentifier:
            return parseIdentifierExpr();
        case tk_number:
            return parseNumberExpr();
        case '(':
            return parseParenExpr();
    }
}

static std::map<char, int> binaryOpPrecedence;

static int getOpPrecedence() {
    if (!isascii(curTok)) return -1;
    int OpTok = binaryOpPrecedence[curTok];
    if (OpTok <= 0) return -1;
    return OpTok;
}

static std::unique_ptr<ExprAST> parseBinaryOpRHS(int precedence, std::unique_ptr<ExprAST> lhs) {
    while (true) {
        int curPrecedence = getOpPrecedence();
        if (curPrecedence < precedence) return lhs;
        int binaryOp = curTok;
        getNextTok();
        auto rhs = primeryParser();
        if (!rhs) return nullptr;
        int nextprecedence = getOpPrecedence();
        if (curPrecedence < nextprecedence) {
            rhs = parseBinaryOpRHS(curPrecedence+1, std::move(rhs));
            if (!rhs) return nullptr;
        }
        lhs = std::make_unique<BinaryExprAST>(binaryOp, std::move(lhs), std::move(rhs));
    }
}

static std::unique_ptr<ExprAST> parseExpression() {
    auto lhs = primeryParser();
    if (!lhs) return nullptr;
    return parseBinaryOpRHS(0, std::move(lhs));
}

int main() {
    binaryOpPrecedence['<'] = 10;
    binaryOpPrecedence['+'] = 20;
    binaryOpPrecedence['-'] = 30;
    binaryOpPrecedence['*'] = 40;
}