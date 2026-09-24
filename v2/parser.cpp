#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "map"
#include "ast.h"
#include "lexer.cpp"

using namespace llvm;

static int curTok;
static std::unique_ptr<LLVMContext> context;
static std::unique_ptr<IRBuilder<>> builder;
static std::unique_ptr<Module> theModule;
static std::map<std::string, Value *> namedValues;

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

Value *logErrV(const char *Str) {
    logErr(Str);
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

static std::unique_ptr<PrototypeAST> parsePrototype() {
    if (curTok != tk_indentifier) return logErrP("Expected a function name");
    std::string f_name = IdentifierStr;
    getNextTok();
    if (curTok != '(') return logErrP("Expected '(' after function name");
    std::vector<std::string> argNames;
    while (getNextTok() == tk_indentifier) {
        argNames.push_back(IdentifierStr);
    }
    if (curTok != ')') return logErrP("Expected ')' after parameters");
    getNextTok();
    return std::make_unique<PrototypeAST>(f_name, std::move(argNames));
}

static std::unique_ptr<PrototypeAST> parseExtern() {
    getNextTok();
    return parsePrototype();
}

static std::unique_ptr<FunctionAST> parseDefinition() {
    getNextTok();
    auto proto = parsePrototype();
    if (!proto) return nullptr;
    if (auto expr = parseExpression()) {
        return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
    }
    return nullptr;
}

static std::unique_ptr<FunctionAST> parseTopLevelExpr() {
    if (auto expr = parseExpression()) {
        auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
    }
    return nullptr;
}

static std::unique_ptr<ExprAST> parseExpression() {
    auto lhs = primeryParser();
    if (!lhs) return nullptr;
    return parseBinaryOpRHS(0, std::move(lhs));
}

Value *NumberExprAST::codegen() {
    return ConstantFP::get(*context, APFloat(val));
}

Value *VariableExprAST::codegen() {
    Value *v = namedValues[name];
    if (!v) {
        logErrV("Unknown variable name");
    }
    return v;
}

Value *BinaryExprAST::codegen() {
    Value *l = lhs->codegen();
    Value *r = rhs->codegen();
    if (!l || !r) {
        return nullptr;
    }
    switch (op) {
    case '+':
        return builder->CreateFAdd(l, r, "addtmp");
    case '-':
        return builder->CreateFSub(l, r, "subtmp");
    case '*':
        return builder->CreateFMul(l, r, "multmp");
    case '<':
        l = builder->CreateFCmpULT(l, r, "cmptmp");
        return builder->CreateUIToFP(l, Type::getDoubleTy(*context), "booltmp");
    default:
        return logErrV("Invalid boolean operator");
    }
}

Value *CallExprAST::codegen() {
    Function *calleeF = theModule->getFunction(callee);
    if (!calleeF) {
        return logErrV("Unknown function");
    }
    if (calleeF->arg_size() == args.size()) {
        return logErrV("Invalid number of arguments");
    }
    std::vector<Value *> argsV;
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
        argsV.push_back(args[i]->codegen());
        if (!argsV.back()) {
            return nullptr;
        }
    }
    return builder->CreateCall(calleeF, argsV, "calltmp");
}

Function *PrototypeAST::codegen() {
    std::vector<Type *> Doubles(args.size(), Type::getDoubleTy(*context));
    FunctionType *ft = FunctionType::get(Type::getDoubleTy(*context), Doubles, false);
    Function *f = Function::Create(ft, Function::ExternalLinkage, name, theModule.get());
    unsigned idx = 0;
    for (auto &arg : f->args()) {
        arg.setName(args[idx++]);
    }
    return f;
}

Function *FunctionAST::codegen() {
    Function *function = theModule->getFunction(proto->getName());
    if (!function) {
        function = proto->codegen();
    }
    if (!function) {
        return nullptr;
    }
    if (!function->empty()) {
        return (Function *)logErrV("Function can not be redefined!");
    }
    BasicBlock *bb = BasicBlock::Create(*context, "entity", function);
    builder->SetInsertPoint(bb);
    namedValues.clear();
    for (auto &arg : function->args()) {
        namedValues[std::string(arg.getName())] = &arg;
    }
    if (Value *retVal = body->codegen()) {
        builder->CreateRet(retVal);
        verifyFunction(*function);
        return function;
    }
    function->eraseFromParent();
    return nullptr;
}

static void handleDefinition() {
  if (parseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    getNextTok();
  }
}

static void handleExtern() {
  if (parseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    getNextTok();
  }
}

static void handleTopLevelExpr() {
  if (parseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    getNextTok();
  }
}

static void mainLoop() {
    while (true) {
        fprintf(stderr, "ready>");
        switch (curTok) {
        case tk_eof:
            return;
        case ';':
            getNextTok();
            break;
        case tk_def:
            handleDefinition();
            break;
        case tk_extern:
            handleExtern();
            break;
        default:
            handleTopLevelExpr();
            break;
        }
    }
}

int main() {
    binaryOpPrecedence['<'] = 10;
    binaryOpPrecedence['+'] = 20;
    binaryOpPrecedence['-'] = 30;
    binaryOpPrecedence['*'] = 40;
    fprintf(stderr, "ready>");
    getNextTok();
    mainLoop();
    return 0;
}