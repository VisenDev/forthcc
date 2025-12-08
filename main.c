#include <ctype.h>
#include <stdio.h>

#define CORE_IMPLEMENTATION
#include "core.h"

#define streql core_streql
#define QUIT CORE_FATAL_ERROR

typedef enum {
    TOK_IDENTIFIER,

    /*Keywords*/
    TOK_INT,
    TOK_RETURN,

    /*Syntactic elements*/
    TOK_OPEN_PARENS,
    TOK_CLOSE_PARENS,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_PLUS,
    TOK_SEMICOLON,
    TOK_COMMA
    
} TokenTag;

typedef struct {
    TokenTag tag;
    const char * identifier;
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

core_Bool lex_token(core_Arena * a, FILE * fp, Token * result) {
    char ch = 0;
    core_skip_whitespace(fp);
    if(feof(fp)) return CORE_FALSE;
    ch = fgetc(fp);
    
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
   FILE * fp = fopen(path, "r");
   Tokens t = {0};
   if(!fp) {
       fprintf(stderr, "Failed to open file: '%s'\n", path);
       return t;
   }

   while(!feof(fp)) {
       Token tok = {0};
       if(lex_token(a, fp, &tok)) {
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


Token * tokenstream_get(TokenStream * s) {
    if(s->i > s->t.len) return NULL;
    return &s->t.items[s->i];
}


Token * tokenstream_peek(TokenStream * s) {
    unsigned long point = s->i;
    Token * tok = tokenstream_get(s);
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
            Expression * rhs
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
    ToplevelTag tag;
    union {
        struct {
            const char * name;
            TypeSpecifier return_type;
            FunctionParameters parameters;
            Statements body;
        } function_definition;
    } as;
} Toplevel;

typedef core_Vec(Toplevel) Toplevels;

core_Bool parse_type_specifier(TokenStream * s, core_Arena * a, TypeSpecifier * out) {
    Token * tok = tokenstream_get(s);
    if(tok->tag == TOK_INT) {
        out->tag = TYPE_INT;
    } else {
        CORE_TODO("Support parsing other types");
    }
    return CORE_TRUE;
}

core_Bool parse_function_parameters();

core_Bool parse_function_definition(TokenStream * s, core_Arena * a, FunctionDefinition * out) {
    TypeSpecifier return_type = {0};
    return_type.tag = TYPE_INT;

    out->return_type = return_type;
    out->name = core_arena_strdup(a, ident->identifier);
}

core_Bool parser_should_parse_declaration(TokenStream * s) {
    /*TODO: make this function more robust*/
    
    Token * tok = tokenstream_peek(s);
    assert(tok);
    if(tok->tag == TOK_INT) return CORE_TRUE;
    return CORE_FALSE;
}

core_Bool parse_declaration(TokenStream * s, core_Arena * a, Toplevel * out) {
    long save_point = s->i;
    TypeSpecifier type = {0};
    if(!parse_type_specifier(s, a, &type)) QUIT("Failed to parse declaration type");
    Token * name = tokenstream_get(s);
    Token * third = tokenstream_get(s);
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
