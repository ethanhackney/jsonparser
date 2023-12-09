#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;

#include <err.h>
#include <sysexits.h>

enum {
        TOK_EOF         = EOF,
        TOK_LBRACE      = '{',
        TOK_RBRACE      = '}',
        TOK_LBRACK      = '[',
        TOK_RBRACK      = ']',
        TOK_COMMA       = ',',
        TOK_COLON       = ':',
        TOK_STR         = UCHAR_MAX + 1,
        TOK_NUM,
        TOK_TRUE,
        TOK_FALSE,
};

static const std::unordered_map<int,string> g_tok_names {
        { TOK_EOF,      "TOK_EOF" },
        { TOK_LBRACE,   "TOK_LBRACE" },
        { TOK_RBRACE,   "TOK_RBRACE" },
        { TOK_LBRACK,   "TOK_LBRACK" },
        { TOK_RBRACK,   "TOK_RBRACK" },
        { TOK_COMMA,    "TOK_COMMA" },
        { TOK_COLON,    "TOK_COLON" },
        { TOK_STR,      "TOK_STR" },
        { TOK_NUM,      "TOK_NUM" },
        { TOK_TRUE,     "TOK_TRUE" },
        { TOK_FALSE,    "TOK_FALSE" },
};

struct Token {
        string  _lex;
        int     _type;

        Token(int type, string lex = "")
                : _lex {lex},
                _type {type}
        {
                auto p = g_tok_names.find(_type);
                if (p == g_tok_names.end())
                        errx(EX_USAGE, "invalid token type: %d", _type);
        }

        int Type(void) const
        {
                return _type;
        }

        const string &Lex(void) const
        {
                return _lex;
        }

        const string &Name(void) const
        {
                return g_tok_names.find(_type)->second;
        }
};

class Lexer {
private:
        Token   _curr;
        FILE    *_fp;
        int     _putback;

        string read_str(void)
        {
                string str = "";

                int c = fgetc(_fp);
                while (c != EOF && c != '"') {
                        str += c;
                        c = fgetc(_fp);
                }

                if (c != '"')
                        errx(EX_USAGE, "malformed string");

                return str;
        }

        string read_num(int first)
        {
                string num = "";

                int c = first;
                while (isdigit(c)) {
                        num += c;
                        c = fgetc(_fp);
                }

                if (c == '.') {
                        num += c;

                        c = fgetc(_fp);
                        while (isdigit(c)) {
                                num += c;
                                c = fgetc(_fp);
                        }

                        if (c == 'e' || c == 'E') {
                                num += c;

                                c = fgetc(_fp);
                                if (c == '+' || c == '-') {
                                        num += c;
                                        c = fgetc(_fp);
                                }

                                while (isdigit(c)) {
                                        num += c;
                                        c = fgetc(_fp);
                                }
                        }
                }

                _putback = c;
                return num;
        }

        int read_bool(int first)
        {
                string b = "";

                int c = first;
                while (isalpha(c)) {
                        b += c;
                        c = fgetc(_fp);
                }
                _putback = c;

                if (b == "true")
                        return TOK_TRUE;
                if (b == "false")
                        return TOK_FALSE;

                errx(EX_USAGE, "expected bool, got %s", b.c_str());
        }
public:
        Lexer(FILE *fp)
                : _curr {TOK_EOF},
                _fp {fp},
                _putback {'\0'}
        {
                if (fp == nullptr)
                        errx(EX_USAGE, "nil FILE passed to lexer");
        }

        const Token &Curr(void)
        {
                return _curr;
        }

        void Match(int type)
        {
                auto p = g_tok_names.find(type);
                if (p == g_tok_names.end())
                        errx(EX_USAGE, "expected an invalid type: %d", type);

                if (_curr.Type() != type) {
                        errx(EX_USAGE,
                             "expected %s, got %s",
                             p->second.c_str(),
                             _curr.Name().c_str());
                }

                Next();
        }

        const Token &Next(void)
        {
                for (;;) {
                        int c;

                        if (_putback != '\0') {
                                c = _putback;
                                _putback = '\0';
                        } else {
                                c = fgetc(_fp);
                        }

                        if (isspace(c))
                                continue;

                        switch (c) {
                        case EOF:
                        case '{':
                        case '}':
                        case '[':
                        case ']':
                        case ',':
                        case ':':
                                return _curr = Token{c};
                        }

                        if (c == '"')
                                return _curr = Token{TOK_STR, read_str()};
                        if (isdigit(c))
                                return _curr = Token{TOK_NUM, read_num(c)};
                        if (isalpha(c))
                                return _curr = Token{read_bool(c)};
                }
        }
};

enum {
        NODE_OBJ,
        NODE_ARR,
        NODE_STR,
        NODE_NUM,
        NODE_BOOL,
};

struct json_node {
        std::unordered_map<string,json_node *>  _obj;
        std::vector<json_node *>                _arr;
        double                                  _num;
        string                                  _str;
        bool                                    _bool;
        int                                     _type;

        json_node(int type)
                : _type {type}
        {}

        json_node(string str)
                : _str {str},
                _type {NODE_STR}
        {}

        json_node(double num)
                : _num {num},
                _type {NODE_NUM}
        {}

        json_node(bool b)
                : _bool {b},
                _type {NODE_BOOL}
        {}
};

json_node *parse_json(Lexer &lex);
void json_node_free(json_node *n);

int main(void)
{
        Lexer lex {stdin};
        lex.Next();
        json_node *n = parse_json(lex);

        printf("%s\n", n->_arr[0]->_obj["_id"]->_str.c_str());

        json_node_free(n);
}

static json_node *parse_json_obj(Lexer &lex);
static json_node *parse_json_arr(Lexer &lex);

json_node *parse_json(Lexer &lex)
{
        auto t = lex.Curr().Type();

        if (t == TOK_LBRACE)
                return parse_json_obj(lex);

        if (t == TOK_LBRACK)
                return parse_json_arr(lex);

        if (t == TOK_STR) {
                json_node *n = new json_node{lex.Curr().Lex()};
                lex.Match(TOK_STR);
                return n;
        }

        if (t == TOK_NUM) {
                json_node *n = new json_node{atof(lex.Curr().Lex().c_str())};
                lex.Match(TOK_NUM);
                return n;
        }

        if (t == TOK_TRUE || t == TOK_FALSE) {
                json_node *n = new json_node{t == TOK_TRUE};
                lex.Next();
                return n;
        }

        errx(EX_USAGE, "unknown token type: %d", t);
}

static json_node *parse_json_obj(Lexer &lex)
{
        json_node *n = new json_node{NODE_OBJ};

        lex.Match(TOK_LBRACE);
        while (lex.Curr().Type() != TOK_EOF && lex.Curr().Type() != TOK_RBRACE) {
                string key = lex.Curr().Lex();
                lex.Match(TOK_STR);
                lex.Match(TOK_COLON);
                json_node *val = parse_json(lex);
                n->_obj[key] = val;
                if (lex.Curr().Type() == TOK_COMMA)
                        lex.Match(TOK_COMMA);
        }
        lex.Match(TOK_RBRACE);

        return n;
}

static json_node *parse_json_arr(Lexer &lex)
{
        json_node *n = new json_node{NODE_ARR};

        lex.Match(TOK_LBRACK);
        while (lex.Curr().Type() != TOK_EOF && lex.Curr().Type() != TOK_RBRACK) {
                n->_arr.push_back(parse_json(lex));
                if (lex.Curr().Type() == TOK_COMMA)
                        lex.Match(TOK_COMMA);
        }
        lex.Match(TOK_RBRACK);

        return n;
}

void
json_node_free(json_node *n)
{
        if (n->_type == NODE_STR ||
            n->_type == NODE_NUM ||
            n->_type == NODE_BOOL) {
                delete n;
                return;
        }

        if (n->_type == NODE_OBJ) {
                for (auto p : n->_obj)
                        json_node_free(p.second);
                delete n;
                return;
        }

        if (n->_type == NODE_ARR) {
                for (auto p : n->_arr)
                        json_node_free(p);
                delete n;
                return;
        }
}
