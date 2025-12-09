#include <ctype.h>
#include <stdio.h>

#define CORE_IMPLEMENTATION
#include "core.h"

#define streql core_streql
#define QUIT CORE_FATAL_ERROR

typedef struct SrcInfo SrcInfo;
struct SrcInfo {
    SrcInfo * parent;
    const char * file;
    long line;
    long col;
};

#define DO_TOKENS(x)            \
    x(TOK_IDENTIFIER)           \
                                \
    /*Keywords*/                \
    x(TOK_INT)                  \
    x(TOK_RETURN)               \
                                \
    /*Syntactic elements*/      \
    x(TOK_OPEN_PARENS)          \
    x(TOK_CLOSE_PARENS)         \
    x(TOK_OPEN_BRACE)           \
    x(TOK_CLOSE_BRACE)          \
    x(TOK_PLUS)                 \
    x(TOK_SEMICOLON)            \
    x(TOK_COMMA)                

#define ENUM_MEMBER(a) a,
#define ENUM_NAME(a) #a,

typedef enum {
    DO_TOKENS(ENUM_MEMBER)
    TOK_COUNT
} TokenTag;

const char * token_tag_names[] = {
    DO_TOKENS(ENUM_NAME)
    NULL
};

typedef struct {
    TokenTag tag;
    const char * identifier;

    SrcInfo src; /*For reporting error messages about where the error came from*/
} Token;

typedef core_Vec(Token) Tokens;

void token_fprint(FILE * fp, Token tok) {
    switch(tok.tag) {

    case TOK_IDENTIFIER: fprintf(fp, "TOK_IDENTIFIER(%s)", tok.identifier); break;

    /* Keywords */
    case TOK_INT: fprintf(fp, "TOK_INT"); break;
    case TOK_RETURN: fprintf(fp, "TOK_RETURN"); break;

    /* Syntactic elements */
    case TOK_OPEN_PARENS: fprintf(fp, "TOK_OPEN_PARENS"); break;
    case TOK_CLOSE_PARENS: fprintf(fp, "TOK_CLOSE_PARENS"); break;
    case TOK_OPEN_BRACE: fprintf(fp, "TOK_OPEN_BRACE"); break;
    case TOK_CLOSE_BRACE: fprintf(fp, "TOK_CLOSE_BRACE"); break;
    case TOK_PLUS: fprintf(fp, "TOK_PLUS"); break;
    case TOK_SEMICOLON: fprintf(fp, "TOK_SEMICOLON"); break;
    case TOK_COMMA: fprintf(fp, "TOK_COMMA"); break;

    default: fprintf(fp, "TOK_<UNKNOWN:%d>", tok.tag); break;
    }
}

void token_print(Token tok) {token_fprint(stdout, tok);}

core_Bool lex_token(core_Arena * a, FILE * fp, Token * result, SrcInfo * src) {
    char ch = 0;

    while(isspace(core_peek(fp))) {
        if(core_peek(fp) == '\n') {
            ++src->line;
            src->col = 1;
        } else {
            ++src->col;
        }
        fgetc(fp);
    }

    result->src = *src;

    if(feof(fp)) return CORE_FALSE;
    ch = fgetc(fp);
    ++src->col;
    
    if(ch == '(') {
        result->tag = TOK_OPEN_PARENS;
    } else if (ch == ')') {
        result->tag = TOK_CLOSE_PARENS;
    } else if(ch == '{') {
        result->tag = TOK_OPEN_BRACE;
    } else if(ch == '}') {
        result->tag = TOK_CLOSE_BRACE;
    } else if(ch == '+') {
        result->tag = TOK_PLUS;
    } else if(ch == ';') {
        result->tag = TOK_SEMICOLON;
    } else if(ch == ',') {
        result->tag = TOK_COMMA;
    } else if(isalpha(ch)) {
        char buf[1024];
        unsigned long i = 0;
        while(core_isidentifier(ch)) {
            assert(sizeof(buf) > i + 1);
            buf[i] = ch;
            buf[i+1] = 0;
            ++i;
            ch = fgetc(fp);
            ++src->col;
        }
        ungetc(ch, fp);
        if(streql(buf, "int")) {
            result->tag = TOK_INT;
        } else if(streql(buf, "return")) {
            result->tag = TOK_RETURN;
        } else {
            result->tag = TOK_IDENTIFIER;
            result->identifier = core_arena_strdup(a, buf);
        }
    } else {
        fprintf(stderr, "Invalid Token: %c\n", ch);
        return CORE_FALSE;
    }
    return CORE_TRUE;
}

Tokens tokenize_file(core_Arena * a, const char * path) {
   FILE * fp = NULL;
   Tokens t = {0};
   SrcInfo src = {0};

   src.file = core_arena_strdup(a, path);

   fp = fopen(path, "r");
   if(!fp) {
       fprintf(stderr, "Failed to open file: '%s'\n", path);
       return t;
   }

   while(!feof(fp)) {
       Token tok = {0};
       if(lex_token(a, fp, &tok, &src)) {
           core_vec_append(&t, a, tok);
       }
   }
   
   fclose(fp);
   return t;
}

typedef struct {
    Tokens t;
    unsigned long i;
} TokenStream;


Token * ts_get(TokenStream * s) {
    if(s->i > s->t.len) return NULL;
    return &s->t.items[s->i];
}


Token * ts_peek(TokenStream * s) {
    unsigned long point = s->i;
    Token * tok = ts_get(s);
    s->i = point;
    return tok;
}

/**** PARSER ****/

typedef enum {
    TOPLEVEL_FUNCTION_DEFINITION
} ToplevelTag;

typedef enum {
    STATEMENT_RETURN
} StatementTag;

typedef enum {
    EXPRESSION_PLUS,
    EXPRESSION_IDENTIFIER
} ExpressionTag;

typedef struct Expression Expression;
struct Expression {
    ExpressionTag tag;
    union {
        struct {
            Expression * lhs;
            Expression * rhs;
        } plus;
        const char * identifier;
    } as;
};

typedef struct {
    StatementTag tag;
    union {
        struct {
            Expression value;
        } return_;
    } as;
} Statement;
typedef core_Vec(Statement) Statements;

typedef enum {
    TYPE_INT
} TypeSpecifierTag;

typedef struct TypeSpecifier TypeSpecifier;
struct TypeSpecifier {
    TypeSpecifierTag tag;
    /*union {} as;*/
};

typedef struct {
    TypeSpecifier type;
    const char * name;
} FunctionParameter;

typedef core_Vec(FunctionParameter) FunctionParameters;

typedef struct {
    const char * name;
    TypeSpecifier return_type;
    FunctionParameters parameters;
} FunctionPrototype;

typedef struct {
    FunctionPrototype prototype;
    Statements * body;
} FunctionDefinition;

typedef struct {
    ToplevelTag tag;
    union {
        FunctionDefinition function_definition;
    } as;
} Toplevel;

typedef core_Vec(Toplevel) Toplevels;

core_Bool parse_type_specifier(TokenStream * s, core_Arena * a, TypeSpecifier * out) {
    Token * tok = ts_get(s);
    (void)a;
    if(tok->tag == TOK_INT) {
        out->tag = TYPE_INT;
    } else {
        CORE_TODO("Support parsing other types");
    }
    return CORE_TRUE;
}

core_Bool parse_statement(TokenStream * s, core_Arena * a, Statement * out) {
    Token * first = ts_get(s);
    if(!first) QUIT("Expected statement");
    
}

core_Bool parse_function_definition_or_prototype(TokenStream * s, core_Arena * a, FunctionDefinition * out) {
    Token * name = NULL;
    Token * parens;
    core_Bool more_parameters = CORE_TRUE;
    if(!parse_type_specifier(s, a, &out->prototype.return_type)) return CORE_FALSE;
    name = ts_get(s);
    if(!name || name->tag != TOK_IDENTIFIER) CORE_FATAL_ERROR("Expected identifier");
    out->prototype.name = core_arena_strdup(a, name->identifier);
    parens = ts_get(s);
    if(parens->tag != TOK_OPEN_PARENS) CORE_FATAL_ERROR("Expected '('");
    while(more_parameters) {
        FunctionParameter param = {0};
        if(!parse_type_specifier(s, a, &param.type)) CORE_FATAL_ERROR("Expected type specifier");
        name = ts_get(s);
        if(!name || name->tag != TOK_IDENTIFIER) CORE_FATAL_ERROR("Expected identifier");
        param.name = core_arena_strdup(a, name->identifier);
        core_vec_append(&out->prototype.parameters, a, param);
        if(ts_peek(s) && ts_peek(s)->tag == TOK_COMMA) 
            more_parameters = CORE_TRUE; 
        else more_parameters = CORE_FALSE;;
    }
    if(!ts_peek(s)) CORE_FATAL_ERROR("Unexpected EOF");
    if(ts_get(s)->tag != TOK_CLOSE_PARENS) CORE_FATAL_ERROR("Expected ')'");

    if(ts_peek(s) && ts_peek(s)->tag == TOK_SEMICOLON) {
        out->body = NULL;
        return CORE_TRUE;
    }

    if(!ts_peek(s) || ts_get(s)->tag != TOK_OPEN_BRACE) CORE_FATAL_ERROR("Expected '{'");
    
    
}

core_Bool parser_should_parse_declaration(TokenStream * s) {
    /*TODO: make this function more robust*/
    
    Token * tok = ts_peek(s);
    assert(tok);
    if(tok->tag == TOK_INT) return CORE_TRUE;
    return CORE_FALSE;
}

core_Bool parse_declaration(TokenStream * s, core_Arena * a, Toplevel * out) {
    long save_point = s->i;
    TypeSpecifier type = {0};
    if(!parse_type_specifier(s, a, &type)) QUIT("Failed to parse declaration type");
    Token * name = ts_get(s);
    Token * third = ts_get(s);
    if(third->tag == TOK_OPEN_PARENS) {
        s->i = save_point;
        
    }
}

core_Bool parse_toplevel(TokenStream * s, core_Arena * a, Toplevel * out) {
    if(parser_should_parse_declaration(s)) {
        return parse_declaration(s, a, out);
    } else {
        CORE_TODO("Parse other toplevel forms");
    }
    return CORE_TRUE;
}

int main(void) {
    unsigned long i = 0;
    core_Arena a = {0};
    Tokens t = tokenize_file(&a, "test-cases/001.c");
    for(i = 0; i < t.len; ++i) {
        token_print(t.items[i]);
        puts("");
    }

    core_arena_free(&a);
    
}
