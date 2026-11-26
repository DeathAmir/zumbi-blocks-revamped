#include "zs_compiler.hpp"
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <array>

namespace zumbi {
namespace {
enum class Op : std::uint8_t { ConstNum, ConstStr, True, False, Null, Load, Store, Pop, Add, Sub, Mul, Div, Mod, Neg, Not, Eq, Ne, Lt, Le, Gt, Ge, Jump, JumpFalse, Call, Native, Ret };
struct Ins { Op op{}; std::int32_t a{}, b{}; };
struct Fn { std::string symbol; std::vector<std::string> params; std::uint32_t locals{}; std::vector<Ins> code; };
struct Token { enum Kind { End, Id, Num, Str, Lp, Rp, Lb, Rb, Comma, Semi, Dot, Plus, Minus, Star, Slash, Percent, Bang, Assign, Eq, Ne, Lt, Le, Gt, Ge, And, Or } kind; std::string text; double number{}; std::size_t line{}; };

class Lexer {
public:
 explicit Lexer(std::string s):s_(std::move(s)){}
 bool run(std::vector<Token>& out,std::string& e) {
  while(p_<s_.size()) {
   char c=s_[p_];
   if(c==' '||c=='\t'||c=='\r'){++p_;continue;} if(c=='\n'){++p_;++line_;continue;}
   if(c=='/'&&p_+1<s_.size()&&s_[p_+1]=='/'){while(p_<s_.size()&&s_[p_]!='\n')++p_;continue;}
   auto ln=line_;
   if(std::isalpha((unsigned char)c)||c=='_'){std::string v;while(p_<s_.size()&&(std::isalnum((unsigned char)s_[p_])||s_[p_]=='_'))v+=s_[p_++];out.push_back({Token::Id,v,0,ln});continue;}
   if(std::isdigit((unsigned char)c)){std::string v;while(p_<s_.size()&&(std::isdigit((unsigned char)s_[p_])||s_[p_]=='.'))v+=s_[p_++];try{out.push_back({Token::Num,v,std::stod(v),ln});}catch(...){e="Line "+std::to_string(ln)+": invalid number.";return false;}continue;}
   ++p_;
   if(c=='"'){std::string v;bool closed=false;while(p_<s_.size()){char x=s_[p_++];if(x=='"'){closed=true;break;}if(x=='\n'){e="Line "+std::to_string(ln)+": newline in string.";return false;}if(x=='\\'){if(p_==s_.size())break;x=s_[p_++];if(x=='n')v+='\n';else if(x=='t')v+='\t';else if(x=='"'||x=='\\')v+=x;else{e="Line "+std::to_string(ln)+": invalid escape.";return false;}}else v+=x;}if(!closed){e="Line "+std::to_string(ln)+": unterminated string.";return false;}out.push_back({Token::Str,v,0,ln});continue;}
   Token::Kind k;
   switch(c){case '(':k=Token::Lp;break;case ')':k=Token::Rp;break;case '{':k=Token::Lb;break;case '}':k=Token::Rb;break;case ',':k=Token::Comma;break;case ';':k=Token::Semi;break;case '.':k=Token::Dot;break;case '+':k=Token::Plus;break;case '-':k=Token::Minus;break;case '*':k=Token::Star;break;case '/':k=Token::Slash;break;case '%':k=Token::Percent;break;case '!':k=take('=')?Token::Ne:Token::Bang;break;case '=':k=take('=')?Token::Eq:Token::Assign;break;case '<':k=take('=')?Token::Le:Token::Lt;break;case '>':k=take('=')?Token::Ge:Token::Gt;break;case '&':if(!take('&')){e="Line "+std::to_string(ln)+": expected &&.";return false;}k=Token::And;break;case '|':if(!take('|')){e="Line "+std::to_string(ln)+": expected ||.";return false;}k=Token::Or;break;default:e="Line "+std::to_string(ln)+": unexpected character.";return false;}
   out.push_back({k,std::string(1,c),0,ln});
  }
  out.push_back({Token::End,{},0,line_});return true;
 }
private: bool take(char c){if(p_<s_.size()&&s_[p_]==c){++p_;return true;}return false;} std::string s_;std::size_t p_{},line_{1};
};

class Compiler {
public:
 explicit Compiler(std::vector<Token> t):t_(std::move(t)){}
 bool run(std::vector<Fn>& f,std::vector<std::string>& ss,std::vector<double>& nn,std::string& e){
  err_=&e;
  for(std::size_t i=0;i+1<t_.size();++i)if(t_[i].kind==Token::Id&&t_[i].text=="fn"&&t_[i+1].kind==Token::Id){auto n=t_[i+1].text;if(ids_.contains(n))return fail(t_[i+1],"duplicate function.");ids_[n]=(std::uint32_t)names_.size();names_.push_back(n);}
  if(names_.empty())return fail(peek(),"no functions found.");
  f.resize(names_.size());
  for(std::size_t i=0;i<names_.size();++i){cur_=&f[i];vars_.clear();if(!word("fn")||!take(Token::Id,"expected function name.")||prev().text!=names_[i])return false;
   if(!take(Token::Lp,"expected '('."))return false;
   if(!is(Token::Rp)){do{if(!take(Token::Id,"expected parameter."))return false;auto n=prev().text;if(vars_.contains(n))return fail(prev(),"duplicate parameter.");vars_[n]=(std::uint32_t)vars_.size();cur_->params.push_back(n);}while(match(Token::Comma));}
   if(!take(Token::Rp,"expected ')'." )||!take(Token::Lb,"expected '{'."))return false;
   while(!is(Token::Rb)&&!is(Token::End))if(!stmt())return false;
   if(!take(Token::Rb,"expected '}'."))return false;
   emit(Op::Null);emit(Op::Ret);cur_->locals=(std::uint32_t)vars_.size();
  }
  if(!is(Token::End))return fail(peek(),"unexpected trailing token.");
  ss=strings_;nn=numbers_;return true;
 }
private:
 const Token& peek()const{return t_[p_];}const Token& prev()const{return t_[p_-1];}bool is(Token::Kind k)const{return peek().kind==k;}bool match(Token::Kind k){if(!is(k))return false;++p_;return true;}
 bool take(Token::Kind k,const char* msg){if(match(k))return true;return fail(peek(),msg);}bool word(const char*w){if(is(Token::Id)&&peek().text==w){++p_;return true;}return false;}
 bool fail(const Token&t,const std::string&m){*err_="Line "+std::to_string(t.line)+": "+m;return false;}void emit(Op o,int a=0,int b=0){cur_->code.push_back({o,a,b});}
 std::size_t jump(Op o){emit(o,-1);return cur_->code.size()-1;}void patch(std::size_t i){cur_->code[i].a=(int)cur_->code.size();}
 int str(const std::string&s){auto i=strIds_.find(s);if(i!=strIds_.end())return(int)i->second;auto n=(std::uint32_t)strings_.size();strings_.push_back(s);strIds_[s]=n;return(int)n;}
 int num(double n){auto i=(std::uint32_t)numbers_.size();numbers_.push_back(n);return(int)i;}
 bool stmt(){
  if(word("let")){if(!take(Token::Id,"expected variable name."))return false;auto n=prev().text;if(vars_.contains(n))return fail(prev(),"variable already declared.");auto id=(int)vars_.size();vars_[n]=(std::uint32_t)id;if(!take(Token::Assign,"expected '='.")||!expr()||!take(Token::Semi,"expected ';'."))return false;emit(Op::Store,id);return true;}
  if(word("return")){if(match(Token::Semi))emit(Op::Null);else if(!expr()||!take(Token::Semi,"expected ';'."))return false;emit(Op::Ret);return true;}
  if(word("if")){if(!take(Token::Lp,"expected '('")||!expr()||!take(Token::Rp,"expected ')'")||!take(Token::Lb,"expected '{'"))return false;auto jf=jump(Op::JumpFalse);while(!is(Token::Rb)&&!is(Token::End))if(!stmt())return false;if(!take(Token::Rb,"expected '}'"))return false;auto je=jump(Op::Jump);patch(jf);if(word("else")){if(!take(Token::Lb,"expected '{' after else"))return false;while(!is(Token::Rb)&&!is(Token::End))if(!stmt())return false;if(!take(Token::Rb,"expected '}'"))return false;}patch(je);return true;}
  if(word("while")){if(!take(Token::Lp,"expected '('"))return false;auto start=cur_->code.size();if(!expr()||!take(Token::Rp,"expected ')'")||!take(Token::Lb,"expected '{'"))return false;auto ex=jump(Op::JumpFalse);while(!is(Token::Rb)&&!is(Token::End))if(!stmt())return false;if(!take(Token::Rb,"expected '}'"))return false;emit(Op::Jump,(int)start);patch(ex);return true;}
  if(is(Token::Id)&&p_+1<t_.size()&&t_[p_+1].kind==Token::Assign){auto n=peek().text;p_+=2;auto i=vars_.find(n);if(i==vars_.end())return fail(prev(),"unknown variable.");if(!expr()||!take(Token::Semi,"expected ';'"))return false;emit(Op::Store,(int)i->second);return true;}
  if(!expr()||!take(Token::Semi,"expected ';' after expression."))return false;emit(Op::Pop);return true;
 }
 bool expr(){return orExpr();}
 bool orExpr(){if(!andExpr())return false;while(match(Token::Or)){auto jf=jump(Op::JumpFalse);emit(Op::True);auto je=jump(Op::Jump);patch(jf);if(!andExpr())return false;patch(je);}return true;}
 bool andExpr(){if(!equality())return false;while(match(Token::And)){auto jf=jump(Op::JumpFalse);if(!equality())return false;auto je=jump(Op::Jump);patch(jf);emit(Op::False);patch(je);}return true;}
 bool equality(){if(!compare())return false;while(match(Token::Eq)||match(Token::Ne)){auto k=prev().kind;if(!compare())return false;emit(k==Token::Eq?Op::Eq:Op::Ne);}return true;}
 bool compare(){if(!term())return false;while(match(Token::Lt)||match(Token::Le)||match(Token::Gt)||match(Token::Ge)){auto k=prev().kind;if(!term())return false;emit(k==Token::Lt?Op::Lt:k==Token::Le?Op::Le:k==Token::Gt?Op::Gt:Op::Ge);}return true;}
 bool term(){if(!factor())return false;while(match(Token::Plus)||match(Token::Minus)){auto k=prev().kind;if(!factor())return false;emit(k==Token::Plus?Op::Add:Op::Sub);}return true;}
 bool factor(){if(!unary())return false;while(match(Token::Star)||match(Token::Slash)||match(Token::Percent)){auto k=prev().kind;if(!unary())return false;emit(k==Token::Star?Op::Mul:k==Token::Slash?Op::Div:Op::Mod);}return true;}
 bool unary(){if(match(Token::Bang)){if(!unary())return false;emit(Op::Not);return true;}if(match(Token::Minus)){if(!unary())return false;emit(Op::Neg);return true;}return primary();}
 bool primary(){
  if(match(Token::Num)){emit(Op::ConstNum,num(prev().number));return true;}if(match(Token::Str)){emit(Op::ConstStr,str(prev().text));return true;}
  if(word("true")){emit(Op::True);return true;}if(word("false")){emit(Op::False);return true;}if(word("null")){emit(Op::Null);return true;}
  if(match(Token::Lp)){if(!expr()||!take(Token::Rp,"expected ')'"))return false;return true;}
  if(match(Token::Id)){std::string n=prev().text;while(match(Token::Dot)){if(!take(Token::Id,"expected name after dot."))return false;n+="."+prev().text;}
   if(match(Token::Lp)){int count=0;if(!is(Token::Rp)){do{if(!expr())return false;++count;}while(match(Token::Comma));}if(!take(Token::Rp,"expected ')' after args."))return false;auto f=ids_.find(n);if(f!=ids_.end())emit(Op::Call,(int)f->second,count);else emit(Op::Native,str(n),count);return true;}
   auto v=vars_.find(n);if(v==vars_.end())return fail(prev(),"unknown variable or missing call.");emit(Op::Load,(int)v->second);return true;
  }
  return fail(peek(),"expected expression.");
 }
 std::vector<Token> t_;std::size_t p_{};std::string*err_{};Fn*cur_{};std::vector<std::string>names_,strings_;std::vector<double>numbers_;std::unordered_map<std::string,std::uint32_t>ids_,vars_,strIds_;
};

template<class T>void wr(std::ostream&o,const T&v){o.write((const char*)&v,sizeof(v));}
template<class T>bool rd(std::istream&i,T&v){return(bool)i.read((char*)&v,sizeof(v));}
void wrs(std::ostream&o,const std::string&s){auto n=(std::uint32_t)s.size();wr(o,n);o.write(s.data(),n);}
bool rds(std::istream&i,std::string&s){std::uint32_t n;if(!rd(i,n)||n>16*1024*1024)return false;s.resize(n);return !n||(bool)i.read(s.data(),n);}
struct Module{std::vector<std::string> strings;std::vector<double> numbers;std::vector<Fn> functions;};
bool load(const std::filesystem::path&p,Module&m,std::string&e){
 std::ifstream f(p,std::ios::binary);std::array<char,8> magic{};if(!f||!f.read(magic.data(),8)||magic!=std::array<char,8>{'Z','S','C','V','M','0','0','1'}){e="Invalid ZSC module.";return false;}
 std::uint32_t ns,nn,nf;if(!rd(f,ns)||!rd(f,nn)||!rd(f,nf)||ns>100000||nn>100000||nf>10000){e="Invalid ZSC tables.";return false;}
 for(std::uint32_t i=0;i<ns;++i){std::string s;if(!rds(f,s)){e="Bad ZSC strings.";return false;}m.strings.push_back(std::move(s));}
 for(std::uint32_t i=0;i<nn;++i){double n;if(!rd(f,n)){e="Bad ZSC constants.";return false;}m.numbers.push_back(n);}
 for(std::uint32_t i=0;i<nf;++i){Fn fn;std::uint32_t np,nl,ni;if(!rds(f,fn.symbol)||!rd(f,np)||!rd(f,nl)||!rd(f,ni)||np>4096||nl>65536||ni>10000000){e="Bad ZSC function.";return false;}fn.locals=nl;fn.params.resize(np);for(auto&v:fn.params)if(!rds(f,v)){e="Bad ZSC params.";return false;}for(std::uint32_t j=0;j<ni;++j){std::uint8_t op;Ins x;if(!rd(f,op)||!rd(f,x.a)||!rd(f,x.b)||op>(std::uint8_t)Op::Ret){e="Bad ZSC opcode.";return false;}x.op=(Op)op;fn.code.push_back(x);}m.functions.push_back(std::move(fn));}
 return true;
}
bool truth(const ZsValue&v){if(std::holds_alternative<std::monostate>(v))return false;if(auto p=std::get_if<bool>(&v))return *p;if(auto p=std::get_if<double>(&v))return *p!=0;if(auto p=std::get_if<std::string>(&v))return !p->empty();return false;}
std::string strval(const ZsValue&v){if(auto p=std::get_if<std::string>(&v))return *p;if(auto p=std::get_if<double>(&v)){std::ostringstream s;s<<*p;return s.str();}if(auto p=std::get_if<bool>(&v))return *p?"true":"false";return "null";}
double numval(const ZsValue&v){if(auto p=std::get_if<double>(&v))return *p;if(auto p=std::get_if<bool>(&v))return *p?1:0;return 0;}
bool exec(const Module&m,std::uint32_t id,const std::vector<ZsValue>&args,const ZsNativeCall&native,int depth,std::uint64_t&budget,ZsValue&ret,std::string&e){
 if(depth>128||id>=m.functions.size()){e="Invalid ZSC call depth or function.";return false;}const auto&f=m.functions[id];if(args.size()!=f.params.size()){e="ZSC function argument count mismatch.";return false;}
 std::vector<ZsValue>locals(f.locals),stack;for(std::size_t i=0;i<args.size();++i)locals[i]=args[i];std::size_t ip=0;
 auto pop=[&](ZsValue&v){if(stack.empty())return false;v=std::move(stack.back());stack.pop_back();return true;};
 while(ip<f.code.size()){
  if(budget--==0){e="ZSC instruction budget exceeded.";return false;}auto x=f.code[ip++];ZsValue a,b;
  switch(x.op){
   case Op::ConstNum:if(x.a<0||(std::size_t)x.a>=m.numbers.size()){e="Bad number index.";return false;}stack.emplace_back(m.numbers[x.a]);break;
   case Op::ConstStr:if(x.a<0||(std::size_t)x.a>=m.strings.size()){e="Bad string index.";return false;}stack.emplace_back(m.strings[x.a]);break;
   case Op::True:stack.emplace_back(true);break;case Op::False:stack.emplace_back(false);break;case Op::Null:stack.emplace_back(std::monostate{});break;
   case Op::Load:if(x.a<0||(std::size_t)x.a>=locals.size()){e="Bad local index.";return false;}stack.push_back(locals[x.a]);break;
   case Op::Store:if(x.a<0||(std::size_t)x.a>=locals.size()||!pop(a)){e="Bad local store.";return false;}locals[x.a]=std::move(a);break;
   case Op::Pop:if(!pop(a)){e="Stack underflow.";return false;}break;
   case Op::Neg:if(!pop(a)){e="Stack underflow.";return false;}stack.emplace_back(-numval(a));break;
   case Op::Not:if(!pop(a)){e="Stack underflow.";return false;}stack.emplace_back(!truth(a));break;
   case Op::Add:if(!pop(b)||!pop(a)){e="Stack underflow.";return false;}if(std::holds_alternative<std::string>(a)||std::holds_alternative<std::string>(b))stack.emplace_back(strval(a)+strval(b));else stack.emplace_back(numval(a)+numval(b));break;
   case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:
    if(!pop(b)||!pop(a)){e="Stack underflow.";return false;}
    if(x.op==Op::Sub)stack.emplace_back(numval(a)-numval(b));else if(x.op==Op::Mul)stack.emplace_back(numval(a)*numval(b));else if(x.op==Op::Div||x.op==Op::Mod){if(numval(b)==0){e="Division by zero.";return false;}stack.emplace_back(x.op==Op::Div?numval(a)/numval(b):std::fmod(numval(a),numval(b)));}else if(x.op==Op::Lt)stack.emplace_back(numval(a)<numval(b));else if(x.op==Op::Le)stack.emplace_back(numval(a)<=numval(b));else if(x.op==Op::Gt)stack.emplace_back(numval(a)>numval(b));else stack.emplace_back(numval(a)>=numval(b));break;
   case Op::Eq:case Op::Ne:if(!pop(b)||!pop(a)){e="Stack underflow.";return false;}{bool eq=a.index()==b.index()&&strval(a)==strval(b);stack.emplace_back(x.op==Op::Eq?eq:!eq);}break;
   case Op::Jump:if(x.a<0||(std::size_t)x.a>f.code.size()){e="Bad jump.";return false;}ip=x.a;break;
   case Op::JumpFalse:if(!pop(a)){e="Stack underflow.";return false;}if(!truth(a)){if(x.a<0||(std::size_t)x.a>f.code.size()){e="Bad conditional jump.";return false;}ip=x.a;}break;
   case Op::Call:case Op::Native:{if(x.b<0||(std::size_t)x.b>stack.size()){e="Bad call arguments.";return false;}std::vector<ZsValue> av(x.b);for(int j=x.b-1;j>=0;--j)pop(av[(std::size_t)j]);if(x.op==Op::Call){ZsValue rv;if(x.a<0||!exec(m,(std::uint32_t)x.a,av,native,depth+1,budget,rv,e))return false;stack.push_back(std::move(rv));}else{if(x.a<0||(std::size_t)x.a>=m.strings.size()||!native){e="Native API unavailable.";return false;}stack.push_back(native(m.strings[x.a],av));}break;}
   case Op::Ret:if(!pop(ret))ret=std::monostate{};return true;
  }
  if(stack.size()>1000000){e="ZSC stack limit exceeded.";return false;}
 }
 ret=std::monostate{};return true;
}
}

bool ZsCompiler::compile(const std::filesystem::path&input,const std::filesystem::path&output,std::string&error){
 std::ifstream f(input,std::ios::binary);if(!f){error="Cannot open ZS source.";return false;}std::ostringstream src;src<<f.rdbuf();std::vector<Token>tokens;if(!Lexer(src.str()).run(tokens,error))return false;
 Compiler c(std::move(tokens));std::vector<Fn>functions;std::vector<std::string>strings;std::vector<double>numbers;if(!c.run(functions,strings,numbers,error))return false;
 std::random_device rd;std::mt19937 rng(rd());static constexpr char alphabet[]="abcdefghijklmnopqrstuvwxyz0123456789";std::unordered_map<std::string,bool>used;
 for(auto&fn:functions){do{fn.symbol.clear();for(int i=0;i<3;++i)fn.symbol+=alphabet[rng()%36];}while(used.contains(fn.symbol));used[fn.symbol]=true;}
 std::ofstream out(output,std::ios::binary);if(!out){error="Cannot create ZSC output.";return false;}const std::array<char,8>magic{'Z','S','C','V','M','0','0','1'};out.write(magic.data(),8);
 auto ns=(std::uint32_t)strings.size(),nn=(std::uint32_t)numbers.size(),nf=(std::uint32_t)functions.size();wr(out,ns);wr(out,nn);wr(out,nf);
 for(auto&s:strings)wrs(out,s);for(auto n:numbers)wr(out,n);
 for(auto&fn:functions){wrs(out,fn.symbol);auto np=(std::uint32_t)fn.params.size(),ni=(std::uint32_t)fn.code.size();wr(out,np);wr(out,fn.locals);wr(out,ni);for(auto&p:fn.params)wrs(out,p);for(auto&x:fn.code){auto op=(std::uint8_t)x.op;wr(out,op);wr(out,x.a);wr(out,x.b);}}
 if(!out){error="Failed writing ZSC.";return false;}return true;
}
bool ZsVm::run(const std::filesystem::path&bytecode,const ZsNativeCall&nativeCall,std::string&error){
 Module m;if(!load(bytecode,m,error))return false;if(m.functions.empty()){error="Empty ZSC module.";return false;}std::uint64_t budget=10000000;ZsValue ret;return exec(m,0,{},nativeCall,0,budget,ret,error);
}
}
