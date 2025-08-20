/*
def greet (name)
var greeting = "hello! "
print greeting + name
end

greet ("me")
run

def add (a, b)
return a + b
end

print add (10, 2)
run


var counter = 100
var a = 0
var b = 1
while (counter != 0)
    var c = a + b
    print b
    var a = b
    var b = c
    var counter = counter - 1 
end
run

*/
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <functional>
using namespace std;

struct ROSdatatype {
    bool isVariable = false;
    string type; // "float", "string", "bool", "list"
    string stringValue;
    float floatValue = 0.0f;
    bool boolValue = false;
    vector<ROSdatatype> listValue;
};

struct ContextStackItem {
    bool isWhile = false;
    bool isFor = false;
    int forBeginLine = 0;
    int whileBeginLine = 0;
    unordered_map<string, ROSdatatype> thisContextScopeVars;
};

struct functionData {
    vector<string> body;
    int numArgs;
    vector<string> argNames;
    bool isC = false;
    function<ROSdatatype(const vector<ROSdatatype>&)> cfunc;
};

void print(const string& str) { cout << str << endl; }

string input(const string& prompt) {
    string got;
    cout << prompt;
    getline(cin, got);
    return got;
}

vector<ContextStackItem> ContextStack;
unordered_map<string, ROSdatatype> variables; // global scope
int lineIndex = 0;
bool hasErrored = false;

vector<unordered_map<string, ROSdatatype>> LocalScopeStack;
int InFunctionDepth = 0;
vector<bool> ReturnFlagStack;
vector<ROSdatatype> ReturnValueStack;

unordered_map<string, functionData> functions;

vector<unordered_set<string>> GlobalMarkStack; // per-function set of names marked global

const vector<vector<string>> precedence = {
    {"index"},
    {"++", "--"},
    {"not"},
    {"*", "/", "//"},
    {"+", "-"},
    {"<", "<=", ">", ">="},
    {"==", "!="},
    {"and"},
    {"or"}
};

const vector<string> binaryOP {
    "+", "-", "/", "//", ">", "<", "==", "!=", ">=", "<=", "and", "or", "*", "index"
};
const vector<string> unaryOP_prefix { "not" };
const vector<string> unaryOP_suffix { "++", "--" };

void error(const string& msg) {
    cerr << "Error: " << msg << " at line " << lineIndex << endl;
    hasErrored = true;
}

bool isNumber(const string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i++;
    bool dotSeen = false;
    for (; i < s.size(); i++) {
        if (s[i] == '.') {
            if (dotSeen) return false;
            dotSeen = true;
        } else if (!isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

string strip(const string& str) {
    size_t start = 0;
    while (start < str.size() && isspace(static_cast<unsigned char>(str[start]))) start++;
    size_t end = str.size();
    while (end > start && isspace(static_cast<unsigned char>(str[end - 1]))) end--;
    return str.substr(start, end - start);
}

vector<string> tokenize(const string& str) {
    vector<string> tokens;
    string current;
    bool inQuote = false;
    char quoteChar = '\0';
    for (size_t i = 0; i < str.size(); i++) {
        char c = str[i];
        if (inQuote) {
            current += c;
            if (c == quoteChar) {
                tokens.push_back(current);
                current.clear();
                inQuote = false;
                quoteChar = '\0';
            }
        } else {
            if (c == '\'' || c == '\"') {
                if (!current.empty()) { tokens.push_back(current); current.clear(); }
                inQuote = true;
                quoteChar = c;
                current += c;
            } else if (isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) { tokens.push_back(current); current.clear(); }
            } else if (c == '(' || c == ')' || c == ',') {
                if (!current.empty()) { tokens.push_back(current); current.clear(); }
                tokens.push_back(string(1, c));
            } else {
                current += c;
            }
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

template <typename T>
vector<T> slice(const vector<T>& v, size_t start, size_t end = (size_t)-1) {
    if (end == (size_t)-1 || end > v.size()) end = v.size();
    if (start > end) start = end;
    return vector<T>(v.begin() + start, v.begin() + end);
}

string sliceStr(const string& s, size_t start, size_t end = (size_t)-1) {
    if (end == (size_t)-1 || end > s.size()) end = s.size();
    if (start > end) start = end;
    return string(s.begin() + start, s.begin() + end);
}

template <typename T>
bool contains(const vector<T>& vec, const T& item) {
    for (size_t i = 0; i < vec.size(); i++) if (vec[i] == item) return true;
    return false;
}

ROSdatatype cast(const ROSdatatype& value, const string& targetType) {
    ROSdatatype result;
    if (targetType == "float") {
        if (value.type == "float") result = value;
        else if (value.type == "string" && isNumber(value.stringValue)) {
            result.floatValue = stof(value.stringValue);
            result.type = "float";
        } else if (value.type == "bool") {
            result.floatValue = value.boolValue ? 1.0f : 0.0f;
            result.type = "float";
        } else { error("Cannot cast type " + value.type + " to float"); }
    }
    else if (targetType == "string") {
        if (value.type == "float") { ostringstream ss; ss << value.floatValue; result.stringValue = ss.str(); }
        else if (value.type == "bool") result.stringValue = value.boolValue ? "true" : "false";
        else if (value.type == "string") result.stringValue = value.stringValue;
        else if (value.type == "list") {
            string s = "[";
            for (size_t i = 0; i < value.listValue.size(); i++) { s += cast(value.listValue[i], "string").stringValue; if (i + 1 < value.listValue.size()) s += ", "; }
            s += "]";
            result.stringValue = s;
        }
        result.type = "string";
    }
    else if (targetType == "bool") {
        if (value.type == "bool") result.boolValue = value.boolValue;
        else if (value.type == "float") result.boolValue = (value.floatValue != 0.0f);
        else if (value.type == "string") result.boolValue = (!value.stringValue.empty());
        else if (value.type == "list") result.boolValue = (!value.listValue.empty());
        result.type = "bool";
    }
    else if (targetType == "list") {
        if (value.type == "string") {
            for (char c : value.stringValue) {
                ROSdatatype ch; ch.type = "string"; ch.stringValue = string(1, c);
                result.listValue.push_back(ch);
            }
            result.type = "list";
        } else if (value.type == "list") result = value;
        else error("Cannot cast " + value.type + " to list");
    } else { error("Unknown target type: " + targetType); }
    return result;
}

bool lookupVar(const string& name, ROSdatatype& out) {
    for (int i = (int)LocalScopeStack.size() - 1; i >= 0; --i) {
        auto& scope = LocalScopeStack[i];
        auto it = scope.find(name);
        if (it != scope.end()) { out = it->second; return true; }
    }
    auto itg = variables.find(name);
    if (itg != variables.end()) { out = itg->second; return true; }
    return false;
}

unordered_map<string, ROSdatatype>& currentScope() {
    if (!LocalScopeStack.empty()) return LocalScopeStack.back();
    return variables;
}

ROSdatatype parseValue(const string& valueStr) {
    ROSdatatype r;
    string s = valueStr;
    if (isNumber(s)) { r.floatValue = stof(s); r.type = "float"; }
    else if ((s.size() >= 2) && ((s.front() == '\'' && s.back() == '\'') || (s.front() == '"' && s.back() == '"'))) {
        r.stringValue = s.substr(1, s.size() - 2); r.type = "string";
    }
    else if (s == "true" || s == "false") { r.type = "bool"; r.boolValue = (s == "true"); }
    else {
        ROSdatatype got;
        if (lookupVar(s, got)) r = got;
        else { error("cannot parse value: " + s); }
    }
    return r;
}

vector<string> tokenizeExpression(const string& str) {
    vector<string> tokens;
    string current;
    bool inQuote = false;
    char quoteChar = '\0';

    vector<string> allOps;
    for (auto &group : precedence) for (auto &op : group) allOps.push_back(op);
    for (auto &op : unaryOP_prefix) allOps.push_back(op);
    for (auto &op : unaryOP_suffix) allOps.push_back(op);
    for (auto &op : binaryOP) allOps.push_back(op);
    sort(allOps.begin(), allOps.end());
    allOps.erase(unique(allOps.begin(), allOps.end()), allOps.end());
    sort(allOps.begin(), allOps.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

    auto flushCurrent = [&]() {
        if (!current.empty()) { tokens.push_back(current); current.clear(); }
    };

    for (size_t i = 0; i < str.size(); i++) {
        char c = str[i];

        if (inQuote) {
            current += c;
            if (c == quoteChar) {
                tokens.push_back(current);
                current.clear();
                inQuote = false;
                quoteChar = '\0';
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            flushCurrent();
            inQuote = true;
            quoteChar = c;
            current += c;
            continue;
        }

        if (isspace(static_cast<unsigned char>(c))) {
            flushCurrent();
            continue;
        }

        if (c == '(' || c == ')' || c == ',') {
            flushCurrent();
            tokens.push_back(string(1, c));
            continue;
        }

        bool matched = false;
        for (const string &op : allOps) {
            size_t len = op.size();
            if (len && i + len <= str.size() && str.compare(i, len, op) == 0) {
                flushCurrent();
                tokens.push_back(op);
                i += len - 1;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        current += c;
    }
    flushCurrent();
    return tokens;
}

static int __expr_placeholder_counter = 0;

ROSdatatype callFunction(const string& fname, const vector<string>& argExprs);

ROSdatatype binaryMath(const string& a, const string& op, const string& b) {
    ROSdatatype Adata = parseValue(a);
    ROSdatatype Bdata = parseValue(b);
    ROSdatatype result;

    if (Adata.type == "float" && Bdata.type == "float") {
        result.type = "float";
        if (op == "+") result.floatValue = Adata.floatValue + Bdata.floatValue;
        else if (op == "-") result.floatValue = Adata.floatValue - Bdata.floatValue;
        else if (op == "*") result.floatValue = Adata.floatValue * Bdata.floatValue;
        else if (op == "/") {
            if (Bdata.floatValue == 0) { error("Division by zero"); std::exit(1); }
            result.floatValue = Adata.floatValue / Bdata.floatValue;
        }
        else if (op == "//") {
            if (Bdata.floatValue == 0) { error("Division by zero"); std::exit(1); }
            result.floatValue = static_cast<int>(Adata.floatValue / Bdata.floatValue);
        }
        else if (op == "==") { result.type = "bool"; result.boolValue = (Adata.floatValue == Bdata.floatValue); }
        else if (op == "!=") { result.type = "bool"; result.boolValue = (Adata.floatValue != Bdata.floatValue); }
        else if (op == ">")  { result.type = "bool"; result.boolValue = (Adata.floatValue > Bdata.floatValue); }
        else if (op == "<")  { result.type = "bool"; result.boolValue = (Adata.floatValue < Bdata.floatValue); }
        else if (op == ">=") { result.type = "bool"; result.boolValue = (Adata.floatValue >= Bdata.floatValue); }
        else if (op == "<=") { result.type = "bool"; result.boolValue = (Adata.floatValue <= Bdata.floatValue); }
        else error("Unsupported float op: " + op);
    }
    else if (Adata.type == "string" && Bdata.type == "string") {
        if (op == "+") { result.type = "string"; result.stringValue = Adata.stringValue + Bdata.stringValue; }
        else if (op == "==") { result.type = "bool"; result.boolValue = (Adata.stringValue == Bdata.stringValue); }
        else if (op == "!=") { result.type = "bool"; result.boolValue = (Adata.stringValue != Bdata.stringValue); }
        else error("Unsupported string op: " + op);
    }
    else if (Adata.type == "string" && Bdata.type == "float" && op == "index") {
        int idx = static_cast<int>(Bdata.floatValue);
        if (idx < 0 || idx >= static_cast<int>(Adata.stringValue.size())) {
            error("String index out of range");
            return result;
        }
        result.type = "string";
        result.stringValue = string(1, Adata.stringValue[idx]);
    }
    else if (Adata.type == "bool" && Bdata.type == "bool") {
        result.type = "bool";
        if (op == "and") result.boolValue = Adata.boolValue && Bdata.boolValue;
        else if (op == "or") result.boolValue = Adata.boolValue || Bdata.boolValue;
        else if (op == "==") result.boolValue = (Adata.boolValue == Bdata.boolValue);
        else if (op == "!=") result.boolValue = (Adata.boolValue != Bdata.boolValue);
        else error("Unsupported bool op: " + op);
    }
    else {
        error("Type mismatch for op " + op + ", with values: " + a + ", " + b);
    }
    return result;
}

ROSdatatype unaryMath(const string& a, const string& op) {
    ROSdatatype Adata = parseValue(a);
    ROSdatatype result;

    if (Adata.type == "float") {
        if (op == "++") { result.type = "float"; result.floatValue = Adata.floatValue + 1; }
        else if (op == "--") { result.type = "float"; result.floatValue = Adata.floatValue - 1; }
        else if (op == "not") { result.type = "bool"; result.boolValue = !(Adata.floatValue != 0); }
        else error("Unsupported float unary op: " + op);
    }
    else if (Adata.type == "bool") {
        if (op == "not") { result.type = "bool"; result.boolValue = !Adata.boolValue; }
        else error("Unsupported bool unary op: " + op);
    }
    else if (Adata.type == "string") {
        if (op == "not") { result.type = "bool"; result.boolValue = Adata.stringValue.empty(); }
        else error("Unsupported string unary op: " + op);
    }
    else {
        error("Unsupported type for unary op: " + Adata.type);
    }
    return result;
}

ROSdatatype expression(const string& expr) {
    vector<string> tokens = tokenizeExpression(expr);
    if (tokens.empty()) { error("Empty expression"); return ROSdatatype(); }
    vector<string> placeholdersToErase;

    auto makePlaceholder = [&]() {
        string name = "__EXPR_PLACEHOLDER__" + to_string(__expr_placeholder_counter++);
        placeholdersToErase.push_back(name);
        return name;
    };

    // resolve parentheses and function calls
    for (int i = 0; i < (int)tokens.size(); i++) {
        if (tokens[i] == "(") {
            int depth = 1;
            int j = i + 1;
            while (j < (int)tokens.size() && depth > 0) {
                if (tokens[j] == "(") depth++;
                else if (tokens[j] == ")") depth--;
                j++;
            }
            if (depth != 0) { error("Unmatched parenthesis"); return ROSdatatype(); }

            vector<string> inner(tokens.begin() + i + 1, tokens.begin() + (j - 1));
            string innerExpr;
            for (size_t k = 0; k < inner.size(); k++) {
                if (k) innerExpr += " ";
                innerExpr += inner[k];
            }

            // function call if previous token exists and is a function name
            if (i - 1 >= 0 && functions.count(tokens[i - 1])) {
                string fname = tokens[i - 1];

                // enforce mandatory space before '('
                if (expr.find(fname + "(") != string::npos) {
                    error("function calls require a space before '('");
                    return ROSdatatype();
                }

                // parse arguments from innerExpr
                vector<string> args;
                string cur;
                int d = 0;
                for (char c : innerExpr) {
                    if (c == '(') d++;
                    else if (c == ')') d--;
                    if (c == ',' && d == 0) { args.push_back(strip(cur)); cur.clear(); }
                    else cur += c;
                }
                if (!cur.empty()) args.push_back(strip(cur));

                // evaluate call
                ROSdatatype callVal = callFunction(fname, args);

                string ph = makePlaceholder();
                currentScope()[ph] = callVal;

                // replace fname, '(', inner..., ')' with placeholder
                tokens.erase(tokens.begin() + (i - 1), tokens.begin() + j);
                tokens.insert(tokens.begin() + (i - 1), ph);
                i -= 1;
            } else {
                // normal parenthesized subexpression
                ROSdatatype value = expression(innerExpr);
                string ph = makePlaceholder();
                currentScope()[ph] = value;
                tokens.erase(tokens.begin() + i, tokens.begin() + j);
                tokens.insert(tokens.begin() + i, ph);
                i--;
            }
        }
    }

    // apply operators by precedence
    for (const vector<string>& pass : precedence) {
        for (int i = 0; i < (int)tokens.size(); i++) {
            string token = tokens[i];
            if (!contains(pass, token)) continue;

            if (contains(unaryOP_prefix, token)) {
                if (i + 1 >= (int)tokens.size()) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = unaryMath(tokens[i + 1], token);
                string ph = makePlaceholder();
                currentScope()[ph] = value;
                tokens[i] = ph;
                tokens.erase(tokens.begin() + (i + 1));
                i--;
                continue;
            }

            if (contains(unaryOP_suffix, token)) {
                if (i == 0) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = unaryMath(tokens[i - 1], token);
                string ph = makePlaceholder();
                currentScope()[ph] = value;
                tokens[i] = ph;
                tokens.erase(tokens.begin() + (i - 1));
                i--;
                continue;
            }

            if (contains(binaryOP, token)) {
                if (i == 0 || i + 1 >= (int)tokens.size()) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = binaryMath(tokens[i - 1], token, tokens[i + 1]);
                string ph = makePlaceholder();
                currentScope()[ph] = value;
                tokens[i] = ph;
                tokens.erase(tokens.begin() + (i + 1));
                tokens.erase(tokens.begin() + (i - 1));
                i--;
                continue;
            }
        }
    }

    ROSdatatype result;
    if (!tokens.empty() && tokens[0].find("__EXPR_PLACEHOLDER__") == 0) result = currentScope()[tokens[0]];
    else result = parseValue(tokens[0]);

    // cleanup placeholders
    for (const auto& name : placeholdersToErase) {
        auto& scope = currentScope();
        auto it = scope.find(name);
        if (it != scope.end()) scope.erase(it);
        auto itg = variables.find(name);
        if (itg != variables.end()) variables.erase(itg);
    }

    return result;
}

ROSdatatype callFunction(const string& fname, const vector<string>& argExprs) {
    functionData func = functions[fname];
    if (func.isC) {
        vector<ROSdatatype> args;

        for (int i = 0; i < func.numArgs; i++) {
            if (i < (int)argExprs.size()) {
                args.push_back(expression(argExprs[i]));
            } else {
                ROSdatatype def; def.type = "float"; def.floatValue = 0.0f;
                args.push_back(expression(argExprs[i]));
            }
        }

        return func.cfunc(args);
        
    }

    
    // push new local scope if not cfunc
    LocalScopeStack.push_back(unordered_map<string, ROSdatatype>());
    GlobalMarkStack.push_back(unordered_set<string>());
    InFunctionDepth++;
    ReturnFlagStack.push_back(false);


    for (int i = 0; i < func.numArgs; i++) {
        if (i < (int)argExprs.size()) {
            currentScope()[func.argNames[i]] = expression(argExprs[i]);
        } else {
            ROSdatatype def; def.type = "float"; def.floatValue = 0.0f;
            currentScope()[func.argNames[i]] = def;
        }
    }

    int savedLine = lineIndex;
    lineIndex = 0;
    // execute function body
    bool savedError = hasErrored;
    hasErrored = false;
    // Reuse execBlock to execute function body
    // forward declaration
    void execBlock(const vector<string>&);
    execBlock(func.body);

    ROSdatatype retVal;
    if (!ReturnValueStack.empty()) {
        retVal = ReturnValueStack.back();
        ReturnValueStack.pop_back();
    } else {
        retVal.type = "float";
        retVal.floatValue = 0.0f;
    }
    if (!ReturnFlagStack.empty()) ReturnFlagStack.pop_back();

    LocalScopeStack.pop_back();
    GlobalMarkStack.pop_back();
    InFunctionDepth--;
    lineIndex = savedLine;
    hasErrored = savedError || hasErrored;
    return retVal;
}

bool truthy(const ROSdatatype& v) {
    if (v.type == "bool") return v.boolValue;
    if (v.type == "float") return v.floatValue != 0.0f;
    if (v.type == "string") return !v.stringValue.empty();
    if (v.type == "list") return !v.listValue.empty();
    return false;
}

void execBlock(const vector<string>& block) {
    int savedLineIndex = lineIndex;
    lineIndex = 0;
    while (lineIndex < (int)block.size()) {
        string line = block[lineIndex];
        vector<string> tokens = tokenize(line);
        if (tokens.empty()) { lineIndex++; continue; }
        string cmd = tokens[0];
        
        if (cmd == "var") {
            if (tokens.size() < 4 || tokens[2] != "=") { error("invalid var syntax"); lineIndex++; continue; }
            size_t eqpos = line.find('=');
            if (eqpos == string::npos) { error("missing = in var"); lineIndex++; continue; }
            string name = tokens[1];
            string exprStr = strip(sliceStr(line, eqpos + 1));
            ROSdatatype val = expression(exprStr);

            if (InFunctionDepth > 0 && !GlobalMarkStack.empty() && GlobalMarkStack.back().count(name)) {
                variables[name] = val;
            } else {
                currentScope()[name] = val;
            }
        }
        else if (cmd == "global") {
            if (InFunctionDepth == 0) { lineIndex++; continue; }
            if (tokens.size() < 2) { error("global requires a name"); lineIndex++; continue; }
            GlobalMarkStack.back().insert(tokens[1]);
        }
        else if (cmd == "def") {
            if (tokens.size() < 2) { error("function name missing"); lineIndex++; continue; }
            string fname = tokens[1];
            size_t lp = line.find("("), rp = line.find(")");
            vector<string> params;
            if (lp != string::npos && rp != string::npos && rp > lp) {
                string inside = sliceStr(line, lp + 1, rp);
                string cur;
                for (char c : inside) { if (c == ',') { params.push_back(strip(cur)); cur.clear(); } else cur += c; }
                if (!cur.empty()) params.push_back(strip(cur));
            }
            vector<string> fb;
            int depth = 1;
            lineIndex++;
            while (lineIndex < (int)block.size() && depth > 0) {
                vector<string> t = tokenize(block[lineIndex]);
                if (!t.empty()) {
                    if (t[0] == "def") depth++;
                    else if (t[0] == "end") depth--;
                }
                if (depth > 0) fb.push_back(block[lineIndex]);
                lineIndex++;
            }

            functionData func;
            func.argNames = params;
            func.numArgs = (int)params.size();
            func.body = fb;

            functions[fname] = func;
            continue;
        }
        else if (functions.count(cmd)) {
            // standalone function call (no assignment)
            // enforce mandatory space before '('
            if (line.find(cmd + "(") != string::npos) {
                error("function calls require a space before '('");
                return;
            }
            size_t lp = line.find("("), rp = line.find(")");
            vector<string> args;
            if (lp != string::npos && rp != string::npos && rp > lp) {
                string inside = sliceStr(line, lp + 1, rp);
                string cur;
                int depth = 0;
                for (char c : inside) {
                    if (c == '(') depth++; else if (c == ')') depth--;
                    if (c == ',' && depth == 0) { args.push_back(strip(cur)); cur.clear(); }
                    else cur += c;
                }
                if (!cur.empty()) args.push_back(strip(cur));
            }

            (void)callFunction(cmd, args);
            lineIndex++;
            continue;
        }
        else if (cmd == "return") {
            string exprStr = sliceStr(line, line.find("return") + 6);
            ReturnValueStack.push_back(expression(strip(exprStr)));
            if (!ReturnFlagStack.empty()) ReturnFlagStack.back() = true;
            return;
        }
        else if (cmd == "while") {
            size_t lp = line.find("("), rp = line.find_last_of(')');
            if (lp == string::npos || rp == string::npos || rp <= lp) { error("invalid while syntax"); lineIndex++; continue; }
            string condExpr = strip(sliceStr(line, lp + 1, rp));

            vector<string> body;
            int depth = 1;
            lineIndex++;
            while (lineIndex < (int)block.size() && depth > 0) {
                vector<string> t = tokenize(block[lineIndex]);
                if (!t.empty()) {
                    if (t[0] == "while") depth++;
                    else if (t[0] == "end") depth--;
                }
                if (depth > 0) body.push_back(block[lineIndex]);
                lineIndex++;
            }

            while (true) {
                ROSdatatype cv = expression(condExpr);
                if (!truthy(cast(cv, "bool"))) break;
                execBlock(body);
                if (hasErrored) break;
            }
            continue;
        }
        else if (cmd == "for") {
            size_t lp = line.find("("), rp = line.find_last_of(')');
            if (lp == string::npos || rp == string::npos || rp <= lp) { error("invalid for syntax"); lineIndex++; continue; }
            string inside = strip(sliceStr(line, lp + 1, rp));

            // split by ';' into init; cond; inc
            vector<string> parts;
            string cur; int depth = 0;
            for (char c : inside) {
                if (c == '(') depth++; else if (c == ')') depth--;
                if (c == ';' && depth == 0) { parts.push_back(strip(cur)); cur.clear(); }
                else cur += c;
            }
            if (!cur.empty()) parts.push_back(strip(cur));
            if (parts.size() != 3) { error("for requires 3 parts"); lineIndex++; continue; }
            string init = parts[0], cond = parts[1], inc = parts[2];

            // execute init (support "i = x" or "var i = x")
            auto doAssign = [&](const string& s) {
                string lhs, rhs; size_t eq = s.find('=');
                if (eq != string::npos) {
                    lhs = strip(sliceStr(s, 0, eq));
                    rhs = strip(sliceStr(s, eq + 1));
                    currentScope()[lhs] = expression(rhs);
                } else {
                    // allow "i + 1" style increment in init too (rare)
                    vector<string> ts = tokenize(s);
                    if (ts.size() == 3 && ts[1] == "+") {
                        ROSdatatype curv = parseValue(ts[0]);
                        ROSdatatype addv = expression(ts[2]);
                        ROSdatatype sum = binaryMath(ts[0], "+", ts[2]);
                        currentScope()[ts[0]] = sum;
                    }
                }
            };
            if (init.rfind("var ", 0) == 0) {
                string rest = strip(sliceStr(init, 4));
                doAssign(rest);
            } else {
                doAssign(init);
            }

            // capture body
            vector<string> body;
            int depth2 = 1;
            lineIndex++;
            while (lineIndex < (int)block.size() && depth2 > 0) {
                vector<string> t = tokenize(block[lineIndex]);
                if (!t.empty()) {
                    if (t[0] == "for") depth2++;
                    else if (t[0] == "end") depth2--;
                }
                if (depth2 > 0) body.push_back(block[lineIndex]);
                lineIndex++;
            }

            auto doInc = [&]() {
                size_t eq = inc.find('=');
                if (eq != string::npos) {
                    string lhs = strip(sliceStr(inc, 0, eq));
                    string rhs = strip(sliceStr(inc, eq + 1));
                    currentScope()[lhs] = expression(rhs);
                } else {
                    vector<string> ts = tokenize(inc);
                    if (ts.size() == 3 && ts[1] == "+") {
                        ROSdatatype sum = binaryMath(ts[0], "+", ts[2]);
                        currentScope()[ts[0]] = sum;
                    } else {
                        // generic expression evaluated but result ignored
                        (void)expression(inc);
                    }
                }
            };

            while (true) {
                ROSdatatype cv = expression(cond);
                if (!truthy(cast(cv, "bool"))) break;
                execBlock(body);
                if (hasErrored) break;
                doInc();
            }
            continue;
        }
        else if (cmd == "help") {
            print("ROS++ interpreter");
            print("Commands: print, var, def, return, while, for, global, end");
            print("var <name> = <expression>");
            print("var x = add (10, 2) + 32 / 3  * (54 + 2)");
            print("");
            print("def <name> (arg1, arg2 ...)");
            print("return <expression>");
            print("end");
            print("");
            print("def add (a, b)");
            print("return a + b");
            print("end");
            print("");
            print("while (<expression>)");
            print("end");
            print("while (true)");
            print("print 'yes'");
            print("end");
            print("");
            print("for (<var> = <expression>; <expression>; <expression>)");
            print("end");
            print("for (i = 0; i != 10; i + 1)");
            print("print i");
            print("end");
        }

        lineIndex++;
    }
    lineIndex = savedLineIndex;
}

ROSdatatype ROSprint(const vector<ROSdatatype>& args) {
    string toprint;
    for (const auto& arg : args) {
        toprint += cast(arg, "string").stringValue;
    }
    print(toprint);
    
    ROSdatatype r; r.floatValue = 0.0f; r.type = "float";
    return r;
}


int main() {
    cout << "Type 'help' for a list of cmds. \nafter typeing in the program type 'run' to run the program." << endl;
    string ask;
    vector<string> toExec;

    functionData bulitInPrint;
    bulitInPrint.isC = true;

    bulitInPrint.cfunc = ROSprint;
    functions["print"] = bulitInPrint;

    while (true) {
        cout << ">>> ";
        getline(cin, ask);

        if (ask == "run") {
            auto start = chrono::high_resolution_clock::now();

            
            execBlock(toExec);

            auto end = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = end - start;

            toExec.clear();
            cout << "completed running in " << elapsed.count() << " seconds" << endl;
        }
        else if (ask == "exit") {
            break;
        }
         else {
            toExec.push_back(ask);
        }
        
    }
}
