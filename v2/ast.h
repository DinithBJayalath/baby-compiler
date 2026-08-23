#include <string>
#include <vector>

namespace
{
    class ExprAST {
    public:
        virtual ~ExprAST() = default;
    };

    class NumberExprAST {
        double val;
    public:
        NumberExprAST(double val) : val(val) {}
    };

    class VariableExprAST {
        std::string name;
    public:
        VariableExprAST(const std::string &name) : name(name) {}
    };

    class BinaryExprAST {
        char op;
        std::unique_ptr<ExprAST> lhs , rhs;
    public:
        BinaryExprAST(
            char op,
            std::unique_ptr<ExprAST> lhs,
            std::unique_ptr<ExprAST> rhs
        ) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    };

    class CallExprAST {
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;
    public:
        CallExprAST(
            const std::string &callee,
            std::vector<std::unique_ptr<ExprAST>> args
        ) : callee(callee), args(std::move(args)) {}
    };

    class PrototypeAST {
        std::string name;
        std::vector<std::unique_ptr<ExprAST>> args;
    public:
        PrototypeAST(
            const std::string name,
            std::vector<std::unique_ptr<ExprAST>> args
        ) : name(name), args(std::move(args)) {}
        const std::string &getName() const {return name;}
    };

    class FunctionAST {
        std::unique_ptr<PrototypeAST> proto;
        std::unique_ptr<ExprAST> body;
    public:
        FunctionAST(
            std::unique_ptr<PrototypeAST> proto,
            std::unique_ptr<ExprAST> body
        ) : proto(std::move(proto)), body(std::move(body)) {}
    };
}
