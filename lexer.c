#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

typedef enum {
    T_NUMBER, T_STRING, T_CHAR, T_COMMENT,
    T_KEYWORD, T_IDENT, T_ANNOTATION,
    T_OP, T_DELIM,
    T_ERROR
} TokenType;

const char* token_name(TokenType t){
    switch(t){
        case T_NUMBER: return "NUMBER";
        case T_STRING: return "STRING";
        case T_CHAR: return "CHAR";
        case T_COMMENT:return "COMMENT";
        case T_KEYWORD:return "KEYWORD";
        case T_IDENT:  return "IDENT";
        case T_ANNOTATION:return "ANNOTATION";
        case T_OP:     return "OP";
        case T_DELIM:  return "DELIM";
        default:       return "ERROR";
    }
}

static const char* KEYWORDS[] = {
 "abstract","assert","boolean","break","byte","case","catch","char","class","const",
 "continue","default","do","double","else","enum","extends","final","finally","float",
 "for","goto","if","implements","import","instanceof","int","interface","long","native",
 "new","package","private","protected","public","return","short","static","strictfp",
 "super","switch","synchronized","this","throw","throws","transient","try","void",
 "volatile","while","record","sealed","permits","var","yield","non-sealed","module",
 "open","opens","exports","requires","transitive","uses","provides","with","to"
};
static const size_t KEYWORDS_N = sizeof(KEYWORDS)/sizeof(KEYWORDS[0]);

int is_keyword(const char* s, size_t len){
    for(size_t i=0;i<KEYWORDS_N;i++){
        size_t klen = strlen(KEYWORDS[i]);
        if(klen==len && strncmp(s, KEYWORDS[i], len)==0) return 1;
    }
    return 0;
}

char* read_file(const char* path, size_t* out_len){
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(sz < 0){ fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if(!buf){ fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    if(out_len) *out_len = n;
    return buf;
}

#define ULINE "\x1b[4m"
#define RESET "\x1b[0m"

void emit(const char* start, size_t len, TokenType t){
    char* tmp = malloc(len+1);
    memcpy(tmp, start, len);
    tmp[len] = '\0';
    printf(ULINE"%s"RESET" <%s>\n", tmp, token_name(t));
    free(tmp);
}

size_t scan_string(const char* p, const char* end){
    if(*p != '\"') return 0;
    const char* i = p+1;
    while(i<end){
        if(*i=='\\'){ i++; if(i<end) i++; }
        else if(*i=='\"'){ return (size_t)(i - p + 1); }
        else i++;
    }
    return (size_t)(end - p);
}

size_t scan_charlit(const char* p, const char* end){
    if(*p != '\'') return 0;
    const char* i = p+1;
    if(i>=end) return (size_t)(end - p);
    if(*i=='\\'){ i++; if(i<end) i++; }
    else i++;
    if(i<end && *i=='\'') return (size_t)(i - p + 1);
    return (size_t)(end - p);
}

size_t scan_comment(const char* p, const char* end){
    if(p+1>=end) return 0;
    if(p[0]=='/' && p[1]=='/'){
        const char* i = p+2;
        while(i<end && *i!='\n') i++;
        return (size_t)(i - p);
    }
    if(p[0]=='/' && p[1]=='*'){
        const char* i = p+2;
        while(i<end-1){
            if(i[0]=='*' && i[1]=='/') return (size_t)(i - p + 2);
            i++;
        }
        return (size_t)(end - p);
    }
    return 0;
}

typedef struct {
    const char* pattern;
    TokenType type;
    regex_t re;
    int compiled;
} Rule;

Rule RULES[] = {
    {"^[ \t\r\n]+", T_ERROR, {}, 0},
    {"^@([A-Za-z_$][A-Za-z0-9_$]*)(\\.[A-Za-z_$][A-Za-z0-9_$]*)*", T_ANNOTATION, {}, 0},
    {"^[A-Za-z_$][A-Za-z0-9_$]*", T_IDENT, {}, 0},
    {"^([0-9](_?[0-9])*)?\\.[0-9](_?[0-9])*(?:[eE][+-]?[0-9](_?[0-9])*)?[FfDd]?|"
     "^[0-9](_?[0-9])*\\.[0-9](_?[0-9])*(?:[eE][+-]?[0-9](_?[0-9])*)?[FfDd]?|"
     "^[0-9](_?[0-9])*(?:[eE][+-]?[0-9](_?[0-9])*)[FfDd]?", T_NUMBER, {}, 0},
    {"^0[xX][0-9A-Fa-f](_?[0-9A-Fa-f])*[Ll]?", T_NUMBER, {}, 0},
    {"^[0-9](_?[0-9])*[Ll]?", T_NUMBER, {}, 0},
    {"^(>>>\\=|>>>|>>\\=|<<\\=|::|->|\\+\\+|--|\\+=|-=|\\*=|/=|%=|&=|\\|=|\\^=|>>|<<|>=|<=|==|!=|&&|\\|\\||\\?|:|~|!|\\+|-|\\*|/|%|=|&|\\||\\^|>|<)", T_OP, {}, 0},
    {"^(\\.\\.\\.|\\(|\\)|\\[|\\]|\\{|\\}|,|;|\\.|@)", T_DELIM, {}, 0}
};


void compile_rules(){
    for(size_t i=0;i<sizeof(RULES)/sizeof(RULES[0]);i++){
        int rc = regcomp(&RULES[i].re, RULES[i].pattern, REG_EXTENDED);
        if(rc!=0){ 
            char buf[256]; 
            regerror(rc, &RULES[i].re, buf, sizeof(buf)); 
            fprintf(stderr, "%s\n", buf); 
            exit(1); 
        }
        RULES[i].compiled = 1;
    }
}

int main(int argc, char** argv){
    if(argc<2) return 1;
    size_t n=0;
    char* src = read_file(argv[1], &n);
    if(!src) return 1;
    compile_rules();
    const char* p = src;
    const char* end = src + n;
    while(p < end){
        size_t m = scan_comment(p, end);
        if(m>0){ emit(p,m,T_COMMENT); p+=m; continue; }
        m = scan_string(p, end);
        if(m>0){ emit(p,m,(p[m-1]=='\"')?T_STRING:T_ERROR); p+=m; continue; }
        m = scan_charlit(p, end);
        if(m>0){ emit(p,m,(p[m-1]=='\'')?T_CHAR:T_ERROR); p+=m; continue; }
        regmatch_t pm;
        if(RULES[0].compiled && regexec(&RULES[0].re, p, 1, &pm, 0)==0 && pm.rm_so==0){
            if(pm.rm_eo>0){ p+=pm.rm_eo; continue; }
        }
        size_t best_len=0; int best_rule=-1; regmatch_t best_pm={0};
        for(int i=1;i<(int)(sizeof(RULES)/sizeof(RULES[0]));i++){
            regmatch_t mpm;
            if(regexec(&RULES[i].re, p, 1, &mpm, 0)==0 && mpm.rm_so==0){
                size_t len = (size_t)mpm.rm_eo;
                if(len>best_len){ best_len=len; best_rule=i; best_pm=mpm; }
            }
        }
        if(best_rule>=0){
            TokenType t = RULES[best_rule].type;
            const char* s = p; size_t len = best_len;
            if(t==T_IDENT){ if(is_keyword(s,len)) t=T_KEYWORD; }
            emit(s,len,t);
            p+=len; continue;
        }
        emit(p,1,T_ERROR);
        p++;
    }
    free(src);
    return 0;
}
