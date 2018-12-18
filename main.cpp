#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <string>
#include <stack>
#include <set>
#include <vector>

using namespace std;

typedef struct {
    const char * name;
    void * pointer;
} symbol_t;

map<string, int> operations = {
    {"+", 0},
    {"-", 0},
    {"$", 1}, // unary minus
    {"*", 2}
};

map<string, unsigned int> addresses;

set<string> functions, variables;

map<string, unsigned int> args_num;

string next_token(const string& expression, size_t& l) {
    string token;
    set<char> stop_chars = {'+', '-', '*', ',', '(', ')', ' '};
    while (l < expression.size() && expression[l] == ' ') {
        l++;
    }
    if (l == expression.size()) {
        return "";
    }
    if (stop_chars.count(expression[l])) {
        token = string(1, expression[l++]);
        return token;
    }
    size_t r = l;
    while (r < expression.size() && !stop_chars.count(expression[r])) {
        r++;
    }
    token = expression.substr(l, r - l);
    l = r;
    return token;
}

void parse_operation(stack<string>& operator_stack, vector<string>& res, const string& token) {
    while (
        operator_stack.size() && 
        token != "$" &&
        operator_stack.top() != "(" && (
            functions.count(operator_stack.top()) ||
            operations[operator_stack.top()] >= operations[token]
        )
    ) {
        res.push_back(operator_stack.top());
        operator_stack.pop();
    }
    operator_stack.push(token);
}

void parse_function(stack<string>& operator_stack, vector<string>&, const string& token) {
    operator_stack.push(token);
}

void parse_right_bracket(stack<string>& operator_stack, vector<string>& res, const string&) {
    while (operator_stack.top() != "(") {
        res.push_back(operator_stack.top());
        operator_stack.pop();
    }
    operator_stack.pop();
}


vector<string> get_prefix_notation(const string& _expression) {
    string expression = '(' + _expression + ')';
    size_t n = expression.size();
    size_t l = 0;
    stack<string> operator_stack;
    vector<string> res;
    string previous;
    stack<size_t> args_count;
    args_count.push(0);
    while (l < n) {
        string token = next_token(expression, l);
        if (token == "-" && (previous == "(" || previous == "," || previous == "$")) {
            token = '$';
        }
        if (operations.count(token)) {
            parse_operation(operator_stack, res, token);
        }
        else if (addresses.count(token) && l < n && expression[l] == '(') {
            parse_function(operator_stack, res, token);
            functions.insert(token);
        }
        else if (isdigit(token[0]) || addresses.count(token)) {
            res.push_back(token);
            if (addresses.count(token)) {
                variables.insert(token);
            }
        }
        else if (token == "(") {
            operator_stack.push(token);
            args_count.push(0);
        }
        else if (token == ")") {
            parse_right_bracket(operator_stack, res, token);
            if (operator_stack.size() && functions.count(operator_stack.top())) {
                if (args_count.top() == 0 && previous != "(") {
                    args_count.top() = 1;
                }
                args_num[operator_stack.top()] = args_count.top();
            }
            args_count.pop();
        }
        else if (token == ",") {
            while (operator_stack.size() && operator_stack.top() != "(") {
                res.push_back(operator_stack.top());
                operator_stack.pop();
            }
            if (!args_count.top()) {
                args_count.top() = 2;
            }
            else {
                args_count.top()++;
            }
        }
        previous = token;
    }
    return res;
}

const unsigned int ADD_CODE = 0xe0800000; // 3, 4, 0
const unsigned int SUB_CODE = 0xe0400000; // 3, 4, 0
const unsigned int MUL_CODE = 0xe0000090; // 4, 0, 3
const unsigned int MOV_CODE = 0xe1a00000; // 3, 0
const unsigned int LDR_CODE = 0xe5900000; // 3, 4
const unsigned int MVN_CODE = 0xe3e00000; // 3, 0
const unsigned int PUSH_CODE = 0xe52d0004; // 3
const unsigned int POP_CODE = 0xe49d0004; // 3
const unsigned int BX_CODE = 0xe12fff10; // 0

const unsigned int CONST_CODE = 0x02000000;

const unsigned int lr = 14;
const unsigned int pc = 15;

unsigned int mov_cmd(unsigned int first, unsigned int second) {
    return MOV_CODE + (1 << 12) * first + second;
}

unsigned int add_cmd(unsigned int first, unsigned int second, unsigned int third, bool is_const = false) {
    return ADD_CODE + (1 << 12) * first + (1 << 16) * second + third + CONST_CODE * is_const;
}

unsigned int sub_cmd(unsigned int first, unsigned int second, unsigned int third, bool is_const = false) {
    return SUB_CODE + (1 << 12) * first + (1 << 16) * second + third + CONST_CODE * is_const;
}

unsigned int mul_cmd(unsigned int first, unsigned int second, unsigned int third) {
    return MUL_CODE + (1 << 16) * first + second + (1 << 12) * third;
}

unsigned int ldr_cmd(unsigned int first, unsigned int second) {
    return LDR_CODE + (1 << 12) * first + (1 << 16) * second;
}

unsigned int mvn_cmd(unsigned int first, unsigned int second) {
    return MVN_CODE + (1 << 12) * first + second;
}

unsigned int push_cmd(unsigned int first) {
    return PUSH_CODE + (1 << 12) * first;
}

unsigned int pop_cmd(unsigned int first) {
    return POP_CODE + (1 << 12) * first;
}

unsigned int bx_cmd(unsigned int first) {
    return BX_CODE + first;
}

void pop_registers(int count, vector<unsigned int> & res) {
    for (int i = count - 1; i >= 0; i--) {
        res.push_back(pop_cmd(i));
    }
}

void push_value(unsigned int token, vector<unsigned int>& res, bool is_variable = false) {
    res.push_back(mov_cmd(pc, pc));
    res.push_back(token);
    res.push_back(sub_cmd(0, pc, 12, true));
    res.push_back(ldr_cmd(0, 0));
    if (is_variable) {
        res.push_back(ldr_cmd(0, 0));
    }
    res.push_back(push_cmd(0));
}

void add(vector<unsigned int> &res) {
    pop_registers(2, res);
    res.push_back(add_cmd(0, 0, 1));
    res.push_back(push_cmd(0));
}

void sub(vector<unsigned int> &res) {
    pop_registers(2, res);
    res.push_back(sub_cmd(0, 0, 1));
    res.push_back(push_cmd(0));
}

void unary_minus(vector<unsigned int> &res) {
    pop_registers(1, res);
    res.push_back(mvn_cmd(1, 0));
    res.push_back(mul_cmd(0, 1, 0));
    res.push_back(push_cmd(0));
}

void mul(vector<unsigned int> &res) {
    pop_registers(2, res);
    res.push_back(mul_cmd(0, 1, 0));
    res.push_back(push_cmd(0));
}

void call_function(string token, vector<unsigned int>& res) {
    unsigned int addr = addresses[token];
    push_value(addr, res, false);
    res.push_back(pop_cmd(4));
    pop_registers(args_num[token], res);
    res.push_back(mov_cmd(lr, pc));
    res.push_back(bx_cmd(4));
    res.push_back(push_cmd(0));
}

void print_res(vector<unsigned int> res) {
    std::ofstream os("out.txt", std::ios::binary);
    for (auto code : res) {
        os.write(reinterpret_cast<char*>(&code), sizeof(code));
    }
}

vector<unsigned int> solve(const string& expression) {
    vector<string> tokens = get_prefix_notation(expression);
    
    vector<unsigned int> res;

    res.push_back(push_cmd(lr));
    res.push_back(push_cmd(4));

    for (size_t i = 0; i < tokens.size(); i++) {
        string token = tokens[i];
        if (isdigit(token[0])) {
            push_value(stoi(token), res, false);
        }
        else if (variables.count(token)) {
            push_value(addresses[token], res, true);
        }
        else if (token == "+") {
            add(res);
        }
        else if (token == "-") {
            sub(res);
        }
        else if (token == "*") {
            mul(res);
        }
        else if (token == "$") {
            unary_minus(res);
        }
        else if (functions.count(token)) {
            call_function(token, res);
        }
    }

    res.push_back(pop_cmd(0));
    res.push_back(pop_cmd(4));
    res.push_back(pop_cmd(lr));
    
    res.push_back(bx_cmd(lr));

    return res;
}

void write_to_buffer(vector<unsigned int>& res, void * out_buffer) {
    unsigned int * out = (unsigned int*) out_buffer;
    for (auto command : res) {
        (*out) = command;
        out++;
    }
}

extern "C" void jit_compile_expression_to_arm(const char * expression,
                                              const symbol_t * externs,
                                              void * out_buffer) {
    while (externs->name != 0 || externs->pointer != 0) {
        addresses[externs->name] =
            (unsigned int)(size_t)(externs->pointer);
        externs++;
    } 
    vector<unsigned int> res = solve(expression);
    write_to_buffer(res, out_buffer);
}
