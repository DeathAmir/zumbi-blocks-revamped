#include "zs_compiler.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace zumbi {
namespace {
enum class Op : std::uint8_t {
    Constant, True, False, Null, LoadLocal, StoreLocal, Pop,
    Add, Sub, Mul, Div, Mod, Negate, Not,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    Jump, JumpFalse, CallFunction, CallNative, Return
};
struct Ins { Op op{}; std::int32_t a{}; std::int32_t b{}; };
struct Function { std::string name, symbol; std::vector<std::string> params; std::vector<Ins> code; std::uint32_t locals{}; };
enum class Tk { End, Id, Number, String, LParen, RParen, LBrace, RBrace, Comma, Semicolon, Dot, Plus, Minus, Star, Slash, Percent, Bang, Equal, EqualEqual, BangEqual, Less, LessEqual, Greater, GreaterEqual, AndAnd, OrOr };
struct Token { Tk kind; std::string text; double number{}; std::size_t line{}; };

class Lexer {
public:
    explicit Lexer(std::string source) : source_(std::move(source)) {}
    bool scan(std::vector<Token>& out, std::string& error) {
        while (!atEnd()) {
            skipSpace();
            if (atEnd()) break;
            const auto line = line_;
            const char c = advance();
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                std::string s(1, c);
                while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') s += advance();
                out.push_back({Tk::Id, s, 0, line});
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                std::string s(1, c);
                while (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.') s += advance();
                try { out.push_back({Tk::Number, s, std::stod(s), line}); }
                catch (...) { error = "Line " + std::to_string(line) + ": invalid number."; return false; }
            } else if (c == '"') {
                std::string s;
                bool closed = false;
                while (!atEnd()) {
                    char x = advance();
                    if (x == '"') { closed = true; break; }
                    if (x == '\n') { error = "Line " + std::to_string(line) + ": newline in string."; return false; }
                    if (x == '\\') {
                        if (atEnd()) break;
                        x = advance();
                        if (x == 'n') s += '\n';
                        else if (x == 't') s += '\t';
                        else if (x == '"' || x == '\\') s += x;
                        else { error = "Line " + std::to_string(line) + ": unsupported string escape."; return false; }
                    } else s += x;
                }
                if (!closed) { error = "Line " + std::to_string(line) + ": unterminated string."; return false; }
                out.push_back({Tk::String, s, 0, line});
            } else {
                Tk kind;
                switch (c) {
                    case '(': kind=Tk::LParen; break; case ')': kind=Tk::RParen; break;
                    case '{': kind=Tk::LBrace; break; case '}': kind=Tk::RBrace; break;
                    case ',': kind=Tk::Comma; break; case ';': kind=Tk::Semicolon; break;
                    case '.': kind=Tk::Dot; break; case '+': kind=Tk::Plus; break;
                    case '-': kind=Tk::Minus; break; case '*': kind=Tk::Star; break;
                    case '/': if (peek()=='/') { while (!atEnd() && peek()!='\n') advance(); continue; } kind=Tk::Slash; break;
                    case '%': kind=Tk::Percent; break;
                    case '!': kind=match('=')?Tk::BangEqual:Tk::Bang; break;
                    case '=': kind=match('=')?Tk::EqualEqual:Tk::Equal; break;
                    case '<': kind=match('=')?Tk::LessEqual:Tk::Less; break;
                    case '>': kind=match('=')?Tk::GreaterEqual:Tk::Greater; break;
                    case '&': if (!match('&')) { error="Line "+std::to_string(line)+": expected &&."; return false; } kind=Tk::AndAnd; break;
                    case '|': if (!match('|')) { error="Line "+std::to_string(line)+": expected ||."; return false; } kind=Tk::OrOr; break;
                    default: error="Line "+std::to_string(line)+": unexpected character."; return false;
                }
                out.push_back({kind, std::string(1,c), 0, line});
            }
        }
        out.push_back({Tk::End, {}, 0, line_});
        return true;
    }
private:
    bool atEnd() const { return pos_ >= source_.size(); }
    char peek() const { return atEnd()?'\0':source_[pos_]; }
    char advance() { char c=source_[pos_++]; if(c=='\n') ++line_; return c; }
    bool match(char c) { if(peek()!=c) return false; advance(); return true; }
    void skipSpace() {
        for (;;) {
            if (peek()==' '||peek()=='\r'||peek()=='\t'||peek()=='\n') { advance(); continue; }
            if (peek()=='/' && pos_+1<source_.size() && source_[pos_+1]=='/') {
                while(!atEnd()&&peek()!='\n') advance();
                continue;
            }
            break;
        }
    }
    std::string source_; std::size_t pos_{}; std::size_t line_{1};
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : t_(std::move(tokens)) {}
    bool parse(std::vector<Function>& functions, std::vector<std::string>& strings, std::string& error) {
        error_=&error;
        for (std::size_t i=0;i+1<t_.size();++i) {
            if (t_[i].kind==Tk::Id && t_[i].text=="fn" && t_[i+1].kind==Tk::Id) {
                const auto name=t_[i+1].text;
                if (functionIds_.contains(name)) return fail(t_[i+1],"duplicate function.");
                functionIds_[name]=static_cast<std::uint32_t>(functionNames_.size());
                functionNames_.push_back(name);
            }
        }
        if (functionNames_.empty()) return fail(peek(),"source has no functions.");
        for (std::uint32_t i=0;i<functionNames_.size();++i) {
            Function f; f.name=functionNames_[i]; functions.push_back(std::move(f));
        }
        for (std::size_t i=0;i<functions.size();++i) {
            current_=&functions[i]; locals_.clear();
            if (!consumeId("fn")) return fail(peek(),"expected fn.");
            if (!consume(Tk::Id, "expected function name.") || previous().text!=current_->name) return false;
            if (!consume(Tk::LParen,"expected '('.")) return false;
            if (!check(Tk::RParen)) {
                do {
                    if (!consume(Tk::Id,"expected parameter name.")) return false;
                    const auto p=previous().text;
                    if (locals_.contains(p)) return fail(previous(),"duplicate parameter.");
                    locals_[p]=static_cast<std::uint32_t>(locals_.size());
                    current_->params.push_back(p);
                } while (match(Tk::Comma));
            }
            if (!consume(Tk::RParen,"expected ')'." ) || !consume(Tk::LBrace,"expected '{'.")) return false;
            while (!check(Tk::RBrace) && !check(Tk::End)) if (!statement()) return false;
            if (!consume(Tk::RBrace,"expected '}'.")) return false;
            emit(Op::Null); emit(Op::Return);
            current_->locals=static_cast<std::uint32_t>(locals_.size());
        }
        if (!check(Tk::End)) return fail(peek(),"unexpected token after function.");
        strings=strings_;
        return true;
    }
private:
    const Token& peek() const { return t_[pos_]; }
    const Token& previous() const { return t_[pos_-1]; }
    bool check(Tk k) const { return peek().kind==k; }
    bool match(Tk k) { if(!check(k)) return false; ++pos_; return true; }
    bool consume(Tk k,const char* message) { if(match(k)) return true; return fail(peek(),message); }
    bool consumeId(const char* s) { if(check(Tk::Id)&&peek().text==s) { ++pos_; return true; } return false; }
    bool fail(const Token& tok,const std::string& msg) { if(error_) *error_="Line "+std::to_string(tok.line)+": "+msg; return false; }
    void emit(Op op,std::int32_t a=0,std::int32_t b=0) { current_->code.push_back({op,a,b}); }
    std::size_t emitJump(Op op) { emit(op,-1); return current_->code.size()-1; }
    void patch(std::size_t at) { current_->code[at].a=static_cast<std::int32_t>(current_->code.size()); }
    std::int32_t stringId(const std::string& s) {
        auto it=stringIds_.find(s); if(it!=stringIds_.end()) return static_cast<std::int32_t>(it->second);
        auto id=static_cast<std::uint32_t>(strings_.size()); strings_.push_back(s); stringIds_[s]=id; return static_cast<std::int32_t>(id);
    }
    bool statement() {
        if (consumeId("let")) {
            if (!consume(Tk::Id,"expected variable name.")) return false;
            const auto name=previous().text;
            if (locals_.contains(name)) return fail(previous(),"variable already declared.");
            const auto id=static_cast<std::uint32_t>(locals_.size()); locals_[name]=id;
            if (!consume(Tk::Equal,"expected '=' after variable name.") || !expression() || !consume(Tk::Semicolon,"expected ';'.")) return false;
            emit(Op::StoreLocal,static_cast<std::int32_t>(id)); return true;
        }
        if (consumeId("return")) {
            if (match(Tk::Semicolon)) emit(Op::Null);
            else if (!expression() || !consume(Tk::Semicolon,"expected ';' after return.")) return false;
            emit(Op::Return); return true;
        }
        if (consumeId("if")) {
            if (!consume(Tk::LParen,"expected '(' after if.") || !expression() || !consume(Tk::RParen,"expected ')' after condition.") || !consume(Tk::LBrace,"expected '{'.")) return false;
            auto otherwise=emitJump(Op::JumpFalse);
            while(!check(Tk::RBrace)&&!check(Tk::End)) if(!statement()) return false;
            if(!consume(Tk::RBrace,"expected '}'.")) return false;
            auto done=emitJump(Op::Jump); patch(otherwise);
            if(consumeId("else")) {
                if(!consume(Tk::LBrace,"expected '{' after else.")) return false;
                while(!check(Tk::RBrace)&&!check(Tk::End)) if(!statement()) return false;
                if(!consume(Tk::RBrace,"expected '}'.")) return false;
            }
            patch(done); return true;
        }
        if (consumeId("while")) {
            if(!consume(Tk::LParen,"expected '(' after while.")) return false;
            const auto start=current_->code.size();
            if(!expression()||!consume(Tk::RParen,"expected ')' after condition.")||!consume(Tk::LBrace,"expected '{'.")) return false;
            auto exit=emitJump(Op::JumpFalse);
            while(!check(Tk::RBrace)&&!check(Tk::End)) if(!statement()) return false;
            if(!consume(Tk::RBrace,"expected '}'.")) return false;
            emit(Op::Jump,static_cast<std::int32_t>(start)); patch(exit); return true;
        }
        if (check(Tk::Id) && pos_+1<t_.size() && t_[pos_+1].kind==Tk::Equal) {
            const auto name=peek().text; ++pos_; ++pos_;
            auto it=locals_.find(name); if(it==locals_.end()) return fail(previous(),"unknown variable.");
            if(!expression()||!consume(Tk::Semicolon,"expected ';'.")) return false;
            emit(Op::StoreLocal,static_cast<std::int32_t>(it->second)); return true;
        }
        if(!expression()||!consume(Tk::Semicolon,"expected ';' after expression.")) return false;
        emit(Op::Pop); return true;
    }
    bool expression() { return logicalOr(); }
    bool logicalOr() {
        if(!logicalAnd()) return false;
        while(match(Tk::OrOr)) { emit(Op::True); emit(Op::Equal); }
        return true;
    }
    bool logicalAnd() {
        if(!equality()) return false;
        while(match(Tk::AndAnd)) { emit(Op::False); emit(Op::NotEqual); }
        return true;
    }
    bool equality() {
        if(!comparison()) return false;
        while(match(Tk::EqualEqual)||match(Tk::BangEqual)) { auto op=previous().kind; if(!comparison()) return false; emit(op==Tk::EqualEqual?Op::Equal:Op::NotEqual); }
        return true;
    }
    bool comparison() {
        if(!term()) return false;
        while(match(Tk::Less)||match(Tk::LessEqual)||match(Tk::Greater)||match(Tk::GreaterEqual)) {
            auto op=previous().kind; if(!term()) return false;
            emit(op==Tk::Less?Op::Less:op==Tk::LessEqual?Op::LessEqual:op==Tk::Greater?Op::Greater:Op::GreaterEqual);
        }
        return true;
    }
    bool term() {
        if(!factor()) return false;
        while(match(Tk::Plus)||match(Tk::Minus)) { auto op=previous().kind; if(!factor()) return false; emit(op==Tk::Plus?Op::Add:Op::Sub); }
        return true;
    }
    bool factor() {
        if(!unary()) return false;
        while(match(Tk::Star)||match(Tk::Slash)||match(Tk::Percent)) { auto op=previous().kind; if(!unary()) return false; emit(op==Tk::Star?Op::Mul:op==Tk::Slash?Op::Div:Op::Mod); }
        return true;
    }
    bool unary() {
        if(match(Tk::Bang)) { if(!unary()) return false; emit(Op::Not); return true; }
        if(match(Tk::Minus)) { if(!unary()) return false; emit(Op::Negate); return true; }
        return primary();
    }
    bool primary() {
        if(match(Tk::Number)) { auto id=numberId(previous().number); emit(Op::Constant,id); return true; }
        if(match(Tk::String)) { emit(Op::Constant,stringId(previous().text)); current_->code.back().b=1; return true; }
        if(consumeId("true")) { emit(Op::True); return true; }
        if(consumeId("false")) { emit(Op::False); return true; }
        if(consumeId("null")) { emit(Op::Null); return true; }
        if(match(Tk::LParen)) { if(!expression()||!consume(Tk::RParen,"expected ')'.")) return false; return true; }
        if(match(Tk::Id)) {
            std::string name=previous().text;
            while(match(Tk::Dot)) { if(!consume(Tk::Id,"expected name after '.'.")) return false; name += "."+previous().text; }
            if(match(Tk::LParen)) {
                std::int32_t count=0;
                if(!check(Tk::RParen)) { do { if(!expression()) return false; ++count; } while(match(Tk::Comma)); }
                if(!consume(Tk::RParen,"expected ')' after arguments.")) return false;
                auto it=functionIds_.find(name);
                if(it!=functionIds_.end()) emit(Op::CallFunction,static_cast<std::int32_t>(it->second),count);
                else emit(Op::CallNative,stringId(name),count);
                return true;
            }
            auto it=locals_.find(name); if(it==locals_.end()) return fail(previous(),"unknown variable or missing function call.");
            emit(Op::LoadLocal,static_cast<std::int32_t>(it->second)); return true;
        }
        return fail(peek(),"expected expression.");
    }
    std::int32_t numberId(double n) {
        auto id=static_cast<std::uint32_t>(numbers_.size()); numbers_.push_back(n); return static_cast<std::int32_t>(id);
    }
    std::vector<Token> t_; std::size_t pos_{}; std::string* error_{};
    std::vector<std::string> functionNames_,strings_; std::vector<double> numbers_;
    std::unordered_map<std::string,std::uint32_t> functionIds_,locals_,stringIds_;
    Function* current_{};
public:
    const std::vector<double>& numbers() const { return numbers_; }
};

template<class T> void writeRaw(std::ostream& out,const T& v) { out.write(reinterpret_cast<const char*>(&v),sizeof(T)); }
template<class T> bool readRaw(std::istream& in,T& v) { return static_cast<bool>(in.read(reinterpret_cast<char*>(&v),sizeof(T))); }
void writeString(std::ostream& out,const std::string& s) { std::uint32_t n=static_cast<std::uint32_t>(s.size()); writeRaw(out,n); out.write(s.data(),n); }
bool readString(std::istream& in,std::string& s) { std::uint32_t n=0; if(!readRaw(in,n)||n>16*1024*1024) return false; s.resize(n); return n==0||static_cast<bool>(in.read(s.data(),n)); }

struct Module { std::vector<std::string> strings; std::vector<double> numbers; std::vector<Function> functions; };
bool loadModule(const std::filesystem::path& path,Module& m,std::string& error) {
    std::ifstream in(path,std::ios::binary); if(!in){error="Cannot open ZSC module.";return false;}
    std::array<char,8> magic{}; if(!in.read(magic.data(),magic.size())||magic!=std::array<char,8>{'Z','S','C','V','M','0','0','1'}){error="Invalid ZSC signature or version.";return false;}
    std::uint32_t ns=0,nn=0,nf=0;
    if(!readRaw(in,ns)||!readRaw(in,nn)||!readRaw(in,nf)||ns>1000000||nn>1000000||nf>100000) {error="Invalid ZSC tables.";return false;}
    for(std::uint32_t i=0;i<ns;++i){std::string s;if(!readString(in,s)){error="Truncated ZSC string table.";return false;}m.strings.push_back(std::move(s));}
    for(std::uint32_t i=0;i<nn;++i){double n;if(!readRaw(in,n)){error="Truncated ZSC constants.";return false;}m.numbers.push_back(n);}
    for(std::uint32_t i=0;i<nf;++i){
        Function f; std::uint32_t params=0,locals=0,count=0;
        if(!readString(in,f.symbol)||!readRaw(in,params)||!readRaw(in,locals)||!readRaw(in,count)||params>4096||locals>65536||count>10000000){error="Invalid ZSC function.";return false;}
        f.locals=locals; f.params.resize(params);
        for(auto& p:f.params) if(!readString(in,p)){error="Truncated ZSC parameter list.";return false;}
        for(std::uint32_t j=0;j<count;++j){std::uint8_t op;Ins x;if(!readRaw(in,op)||!readRaw(in,x.a)||!readRaw(in,x.b)||op>static_cast<std::uint8_t>(Op::Return)){error="Invalid ZSC instruction.";return false;}x.op=static_cast<Op>(op);f.code.push_back(x);}
        m.functions.push_back(std::move(f));
    }
    return true;
}
bool truthy(const ZsValue& v) {
    if(std::holds_alternative<std::monostate>(v)) return false;
    if(auto p=std::get_if<bool>(&v)) return *p;
    if(auto p=std::get_if<double>(&v)) return *p!=0.0;
    if(auto p=std::get_if<std::string>(&v)) return !p->empty();
    return false;
}
std::string valueString(const ZsValue& v) {
    if(auto p=std::get_if<std::string>(&v)) return *p;
    if(auto p=std::get_if<double>(&v)) { std::ostringstream s;s<<*p;return s.str(); }
    if(auto p=std::get_if<bool>(&v)) return *p?"true":"false";
    return "null";
}
double numberValue(const ZsValue& v) {
    if(auto p=std::get_if<double>(&v)) return *p;
    if(auto p=std::get_if<bool>(&v)) return *p?1.0:0.0;
    return 0.0;
}
bool execute(const Module& m,std::uint32_t index,const std::vector<ZsValue>& args,const ZsNativeCall& native,int depth,ZsValue& result,std::string& error) {
    if(depth>128||index>=m.functions.size()){error="ZSC call depth or function index invalid.";return false;}
    const auto& f=m.functions[index]; if(args.size()!=f.params.size()){error="Function argument count mismatch.";return false;}
    std::vector<ZsValue> locals(f.locals),stack; for(std::size_t i=0;i<args.size();++i)locals[i]=args[i];
    std::size_t ip=0; auto pop=[&](ZsValue& v){if(stack.empty())return false;v=std::move(stack.back());stack.pop_back();return true;};
    while(ip<f.code.size()) {
        const auto ins=f.code[ip++]; ZsValue a,b;
        switch(ins.op) {
            case Op::Constant:
                if(ins.b==1){if(ins.a<0||static_cast<std::size_t>(ins.a)>=m.strings.size()){error="String constant out of range.";return false;}stack.emplace_back(m.strings[ins.a]);}
                else {if(ins.a<0||static_cast<std::size_t>(ins.a)>=m.numbers.size()){error="Number constant out of range.";return false;}stack.emplace_back(m.numbers[ins.a]);} break;
            case Op::True: stack.emplace_back(true); break; case Op::False: stack.emplace_back(false); break; case Op::Null: stack.emplace_back(std::monostate{}); break;
            case Op::LoadLocal: if(ins.a<0||static_cast<std::size_t>(ins.a)>=locals.size()){error="Local index out of range.";return false;} stack.push_back(locals[ins.a]); break;
            case Op::StoreLocal: if(ins.a<0||static_cast<std::size_t>(ins.a)>=locals.size()||!pop(a)){error="Invalid local store.";return false;} locals[ins.a]=std::move(a); break;
            case Op::Pop: if(!pop(a)){error="Stack underflow.";return false;} break;
            case Op::Negate: if(!pop(a)){error="Stack underflow.";return false;} stack.emplace_back(-numberValue(a)); break;
            case Op::Not: if(!pop(a)){error="Stack underflow.";return false;} stack.emplace_back(!truthy(a)); break;
            case Op::Add: if(!pop(b)||!pop(a)){error="Stack underflow.";return false;} if(std::holds_alternative<std::string>(a)||std::holds_alternative<std::string>(b))stack.emplace_back(valueString(a)+valueString(b));else stack.emplace_back(numberValue(a)+numberValue(b)); break;
            case Op::Sub: case Op::Mul: case Op::Div: case Op::Mod: case Op::Less: case Op::LessEqual: case Op::Greater: case Op::GreaterEqual:
                if(!pop(b)||!pop(a)){error="Stack underflow.";return false;}
                if(ins.op==Op::Sub)stack.emplace_back(numberValue(a)-numberValue(b));
                else if(ins.op==Op::Mul)stack.emplace_back(numberValue(a)*numberValue(b));
                else if(ins.op==Op::Div){if(numberValue(b)==0){error="Division by zero.";return false;}stack.emplace_back(numberValue(a)/numberValue(b));}
                else if(ins.op==Op::Mod){if(numberValue(b)==0){error="Modulo by zero.";return false;}stack.emplace_back(std::fmod(numberValue(a),numberValue(b)));}
                else if(ins.op==Op::Less)stack.emplace_back(numberValue(a)<numberValue(b));
                else if(ins.op==Op::LessEqual)stack.emplace_back(numberValue(a)<=numberValue(b));
                else if(ins.op==Op::Greater)stack.emplace_back(numberValue(a)>numberValue(b));
                else stack.emplace_back(numberValue(a)>=numberValue(b)); break;
            case Op::Equal: case Op::NotEqual:
                if(!pop(b)||!pop(a)){error="Stack underflow.";return false;}
                {bool eq=a.index()==b.index()&&valueString(a)==valueString(b);stack.emplace_back(ins.op==Op::Equal?eq:!eq);} break;
            case Op::Jump: if(ins.a<0||static_cast<std::size_t>(ins.a)>f.code.size()){error="Invalid jump.";return false;} ip=static_cast<std::size_t>(ins.a); break;
            case Op::JumpFalse: if(!pop(a)){error="Stack underflow.";return false;} if(!truthy(a)){if(ins.a<0||static_cast<std::size_t>(ins.a)>f.code.size()){error="Invalid conditional jump.";return false;}ip=static_cast<std::size_t>(ins.a);} break;
            case Op::CallFunction: {
                if(ins.a<0||static_cast<std::size_t>(ins.a)>=m.functions.size()||ins.b<0||static_cast<std::size_t>(ins.b)>stack.size()){error="Invalid function call.";return false;}
                std::vector<ZsValue> callArgs(static_cast<std::size_t>(ins.b));for(int i=ins.b-1;i>=0;--i)pop(callArgs[static_cast<std::size_t>(i)]);
                ZsValue returned;if(!execute(m,static_cast<std::uint32_t>(ins.a),callArgs,native,depth+1,returned,error))return false;stack.push_back(std::move(returned));break;
            }
            case Op::CallNative: {
                if(ins.a<0||static_cast<std::size_t>(ins.a)>=m.strings.size()||ins.b<0||static_cast<std::size_t>(ins.b)>stack.size()){error="Invalid native call.";return false;}
                std::vector<ZsValue> callArgs(static_cast<std::size_t>(ins.b));for(int i=ins.b-1;i>=0;--i)pop(callArgs[static_cast<std::size_t>(i)]);
                if(!native){error="No native API provider registered.";return false;}stack.push_back(native(m.strings[ins.a],callArgs));break;
            }
            case Op::Return: if(!pop(result))result=std::monostate{};return true;
        }
        if(stack.size()>1000000){error="ZSC stack limit exceeded.";return false;}
    }
    result=std::monostate{};return true;
}
}

bool ZsCompiler::compile(const std::filesystem::path& input,const std::filesystem::path& output,std::string& error) {
    std::ifstream file(input,std::ios::binary); if(!file){error="Cannot open ZS source.";return false;}
    std::ostringstream buffer;buffer<<file.rdbuf();std::vector<Token> tokens;
    if(!Lexer(buffer.str()).scan(tokens,error))return false;
    Parser parser(std::move(tokens));std::vector<Function> functions;std::vector<std::string> strings;
    if(!parser.parse(functions,strings,error))return false;
    std::random_device rd;std::mt19937_64 rng((static_cast<std::uint64_t>(rd())<<32)^rd());
    static constexpr char alphabet[]="abcdefghijklmnopqrstuvwxyz0123456789";std::unordered_map<std::string,bool> used;
    for(auto& f:functions){do{f.symbol.clear();for(int i=0;i<3;++i)f.symbol+=alphabet[rng()%36];}while(used.contains(f.symbol));used[f.symbol]=true;}
    std::ofstream out(output,std::ios::binary);if(!out){error="Cannot create ZSC output.";return false;}
    const std::array<char,8> magic{'Z','S','C','V','M','0','0','1'};out.write(magic.data(),magic.size());
    const auto ns=static_cast<std::uint32_t>(strings.size());const auto nn=static_cast<std::uint32_t>(parser.numbers().size());const auto nf=static_cast<std::uint32_t>(functions.size());
    writeRaw(out,ns);writeRaw(out,nn);writeRaw(out,nf);
    for(const auto& s:strings)writeString(out,s);
    for(const auto n:parser.numbers())writeRaw(out,n);
    for(const auto& f:functions){writeString(out,f.symbol);const auto p=static_cast<std::uint32_t>(f.params.size());writeRaw(out,p);writeRaw(out,f.locals);const auto c=static_cast<std::uint32_t>(f.code.size());writeRaw(out,c);for(const auto& ins:f.code){const auto op=static_cast<std::uint8_t>(ins.op);writeRaw(out,op);writeRaw(out,ins.a);writeRaw(out,ins.b);}}
    if(!out){error="Failed writing ZSC module.";return false;}return true;
}

bool ZsVm::run(const std::filesystem::path& bytecode,const ZsNativeCall& nativeCall,std::string& error) {
    Module module;if(!loadModule(bytecode,module,error))return false;
    if(module.functions.empty()){error="ZSC module has no entry function.";return false;}
    std::uint32_t entry=0;for(std::uint32_t i=0;i<module.functions.size();++i)if(module.functions[i].symbol.size()==3&&module.functions[i].name.empty()){entry=i;break;}
    ZsValue result;return execute(module,entry,{},nativeCall,0,result,error);
}
}
