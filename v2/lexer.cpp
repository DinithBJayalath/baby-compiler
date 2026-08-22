#include <string>
#include <stdio.h>

enum Token {
    tk_eof = -1,
    //keywords
    tk_def = -2,
    tk_extern = -3,
    //elements
    tk_indentifier = -4,
    tk_number = -5
};

static std::string IdentifierStr;
static double NumberVal;

static int getTok() {
    static int lastChar = ' ';
    while (isblank(lastChar)) {
        lastChar = getchar();
    }
    if (isalpha(lastChar)) {
        IdentifierStr = lastChar;
        while (isalnum(lastChar = getchar())) {
            IdentifierStr += lastChar;
        }
        if (IdentifierStr == "def") {
            return tk_def;
        }
        else if (IdentifierStr == "extern") {
            return tk_extern;
        }
        return tk_indentifier;
    }
    if (isdigit(lastChar) || lastChar == '.') {
        std::string numStr;
        do {
            numStr += lastChar;
            lastChar = getchar();
        } while (isdigit(lastChar) || lastChar == '.');
        NumberVal = strtod(numStr.c_str(), 0);
        return tk_number;
    }
    if (lastChar == '#') {
        do {
            lastChar = getchar();
        } while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');
        if (lastChar != EOF) {
            return getTok();
        }
    }
    if (lastChar == EOF) return tk_eof;
    int thisChar = lastChar;
    lastChar = getchar();
    return thisChar;
}