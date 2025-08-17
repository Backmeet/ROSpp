#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;

struct ROSdatatype {
    bool isVariable = false;
    string type; // "float", "string", "bool", "list"
    string stringValue;
    float floatValue = 0.0f;
    bool boolValue = false;
    vector<ROSdatatype> listValue;
};

void print(const string& str) { cout << str << endl; }

string input(const string& prompt) {
    string got;
    cout << prompt;
    getline(cin, got);
    return got;
}

unordered_map<string, ROSdatatype> variables;
int lineIndex = 0;
bool hasErrored = false;

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

vector<string> toknize(const string& str) {
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
    for (size_t i = 0; i < vec.size(); i++) {
        if (vec[i] == item) return true;
    }
    return false;
}

string literalToken(const ROSdatatype& v) {
    if (v.type == "string") return "\"" + v.stringValue + "\"";
    if (v.type == "float") {
        ostringstream ss;
        ss << v.floatValue;
        return ss.str();
    }
    if (v.type == "bool") return v.boolValue ? "true" : "false";
    if (v.type == "list") {
        // simple string representation of list for replacement; not re-parsable to real list
        // but we don't currently evaluate list expressions beyond stringification.
        string s = "[";
        for (size_t i = 0; i < v.listValue.size(); i++) {
            s += literalToken(v.listValue[i]);
            if (i + 1 < v.listValue.size()) s += ", ";
        }
        s += "]";
        return s;
    }
    return "";
}

// ---------- Safe casting ----------
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
        } else {
            error("Cannot cast type " + value.type + " to float");
        }
    }
    else if (targetType == "string") {
        if (value.type == "float") {
            ostringstream ss;
            ss << value.floatValue;
            result.stringValue = ss.str();
        } else if (value.type == "bool") {
            result.stringValue = value.boolValue ? "true" : "false";
        } else if (value.type == "string") {
            result.stringValue = value.stringValue;
        } else if (value.type == "list") {
            string s = "[";
            for (size_t i = 0; i < value.listValue.size(); i++) {
                s += cast(value.listValue[i], "string").stringValue;
                if (i + 1 < value.listValue.size()) s += ", ";
            }
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
                ROSdatatype ch;
                ch.type = "string";
                ch.stringValue = string(1, c);
                result.listValue.push_back(ch);
            }
            result.type = "list";
        } else if (value.type == "list") {
            result = value;
        } else {
            error("Cannot cast " + value.type + " to list");
        }
    }
    else {
        error("Unknown target type: " + targetType);
    }
    return result;
}

ROSdatatype parseValue(const string& valueStr) {
    ROSdatatype r;
    string s = valueStr;
    if (isNumber(s)) {
        r.floatValue = stof(s);
        r.type = "float";
    } else if ((s.size() >= 2) &&
               ((s.front() == '\'' && s.back() == '\'') ||
                (s.front() == '"' && s.back() == '"'))) {
        r.stringValue = s.substr(1, s.size() - 2);
        r.type = "string";
    } else if (s == "true" || s == "false") {
        r.type = "bool";
        r.boolValue = (s == "true");
    } else if (variables.find(s) != variables.end()) {
        r = variables[s];
    } else {
        // treat unknown bareword as string literal
        r.stringValue = s;
        r.type = "string";
    }
    return r;
}

// -------- tokenizer for expressions with operators and parentheses --------
vector<string> tokenizeExpression(const string& str) {
    vector<string> tokens;
    string current;
    bool inQuote = false;
    char quoteChar = '\0';

    // collect all operators dynamically
    vector<string> allOps;
    for (auto &group : precedence) for (auto &op : group) allOps.push_back(op);
    for (auto &op : unaryOP_prefix) allOps.push_back(op);
    for (auto &op : unaryOP_suffix) allOps.push_back(op);
    for (auto &op : binaryOP) allOps.push_back(op);

    // dedupe
    sort(allOps.begin(), allOps.end());
    allOps.erase(unique(allOps.begin(), allOps.end()), allOps.end());

    // longest first
    sort(allOps.begin(), allOps.end(), [](const string& a, const string& b){
        return a.size() > b.size();
    });

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

        if (c == '(' || c == ')') {
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

// ---------- math / logic ----------
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
            if (Bdata.floatValue == 0) { error("Division by zero"); return result; }
            result.floatValue = Adata.floatValue / Bdata.floatValue;
        }
        else if (op == "//") {
            if (Bdata.floatValue == 0) { error("Division by zero"); return result; }
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
        error("Type mismatch for op " + op);
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

    // resolve parentheses recursively
    for (int i = 0; i < static_cast<int>(tokens.size()); i++) {
        if (tokens[i] == "(") {
            int depth = 1;
            int j = i + 1;
            while (j < static_cast<int>(tokens.size()) && depth > 0) {
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

            ROSdatatype value = expression(innerExpr);

            // replace "( ... )" with a single literal token
            tokens.erase(tokens.begin() + i, tokens.begin() + j);
            tokens.insert(tokens.begin() + i, literalToken(value));
            i--; // recheck
        }
    }

    // resolve by precedence
    for (const vector<string>& pass : precedence) {
        for (int i = 0; i < static_cast<int>(tokens.size()); i++) {
            const string token = tokens[i];
            if (!contains(pass, token)) continue;

            // prefix unary
            if (contains(unaryOP_prefix, token)) {
                if (i + 1 >= static_cast<int>(tokens.size())) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = unaryMath(tokens[i + 1], token);
                tokens[i] = literalToken(value);
                tokens.erase(tokens.begin() + (i + 1));
                i--;
                continue;
            }

            // suffix unary
            if (contains(unaryOP_suffix, token)) {
                if (i == 0) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = unaryMath(tokens[i - 1], token);
                tokens[i] = literalToken(value);
                tokens.erase(tokens.begin() + (i - 1));
                i--;
                continue;
            }

            // binary
            if (contains(binaryOP, token)) {
                if (i == 0 || i + 1 >= static_cast<int>(tokens.size())) { error("Missing operand for " + token); return ROSdatatype(); }
                ROSdatatype value = binaryMath(tokens[i - 1], token, tokens[i + 1]);
                tokens[i] = literalToken(value);
                tokens.erase(tokens.begin() + (i + 1));
                tokens.erase(tokens.begin() + (i - 1));
                i--;
                continue;
            }
        }
    }

    if (tokens.empty()) { error("Empty expression after evaluation"); return ROSdatatype(); }
    return parseValue(tokens[0]);
}

// -------- command parsing --------
void parseLine(const string& line) {
    vector<string> tokens = toknize(line);
    if (tokens.empty()) return;

    string cmd = tokens[0];

    if (cmd == "print") {
        string expr = sliceStr(line, line.find("print") + 5);
        expr = strip(expr);
        ROSdatatype val = expression(expr);
        print(cast(val, "string").stringValue);
    }
    else if (cmd == "var") {
        if (tokens.size() < 4 || tokens[2] != "=") { error("invalid var syntax (use: var <name> = <expression>)"); return; }
        size_t eqpos = line.find('=');
        if (eqpos == string::npos) { error("missing = in var"); return; }
        string expr = strip(sliceStr(line, eqpos + 1));
        variables[tokens[1]] = expression(expr);
    }
    else if (cmd == "help") {
        print("Welcome to ROS++ (toy interpreter)");
        print("Commands:");
        print("  print <expression>");
        print("  var <name> = <expression>");
        print("Variables:");
        for (const auto& varData : variables) {print("  " + varData.first + ": " + cast(varData.second, "string").stringValue);}
        print("\nOperators: + - * / // ++ -- == != >= <= > < and or not index");
        print("Examples:");
        print("  var x = 10 + 2 * 3");
        print("  print x");
        print("  print (\"he\" + \"llo\") index 1");
        print("  print not false");
    }
    else {
        // allow entering an expression directly
        ROSdatatype val = expression(line);
        if (!hasErrored) print(cast(val, "string").stringValue);
    }
}

int main() {
    print("Type 'help' for a list of cmds.");
    string ask;
    while (true) {
        ask = input(">>> ");
        hasErrored = false;
        parseLine(ask);
    }
}
