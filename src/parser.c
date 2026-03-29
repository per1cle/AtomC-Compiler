#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "lexer.h"
#include "parser.h"
#include "utils.h"

const char *tokenNames[] = {
    "ID", "TYPE_CHAR", "TYPE_DOUBLE", "ELSE", "IF", "TYPE_INT", "RETURN", "STRUCT", "VOID", "WHILE",
    "INT", "DOUBLE", "CHAR", "STRING", "COMMA", "SEMICOLON", "LPAR", "RPAR", "LBRACKET", "RBRACKET",
    "LACC", "RACC", "END", "ADD", "SUB", "MUL", "DIV", "DOT", "AND", "OR", "NOT", "ASSIGN", "EQUAL",
    "NOTEQ", "LESS", "LESSEQ", "GREATER", "GREATEREQ"
};

const char *tkCodeName(int code){
    if(code >= 0 && code < sizeof(tokenNames)/sizeof(tokenNames[0]))
        return tokenNames[code];
    return "UNKNOWN";
}

Token *iTk;		// the iterator in the tokens list
Token *consumedTk;		// the last consumed token

void tkerr(const char *fmt,...){
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
}

bool consume(int code){
    printf("consume(%s)", tkCodeName(code));
	if(iTk->code==code){
		consumedTk=iTk;
		iTk=iTk->next;
        printf(" => consumed\n");
		return true;
	}
    printf(" => found %s\n", tkCodeName(iTk->code));
	return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit(){
    for(;;){
        if(structDef()) {}
        else if(fnDef()) {}
        else if(varDef()) {}
        else break;
    }
    if(consume(END))
        return true;
    return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
    if(consume(STRUCT)){
        if(consume(ID)){
            if(consume(LACC)){
               for(;;){
                if(varDef());
                else break;
               }
               if(consume(RACC)){
                if(consume(SEMICOLON))
                    return true;
                else
                    tkerr("expected ';' after struct definition");
               }else
                    tkerr("expected '}' after struct definition or invalid expression in struct definition"); 
            }else
                tkerr("expected '{' after struct name");
        }else{
            tkerr("expected struct name");
        }
    }
    return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON 
bool varDef(){
    if(typeBase()){
        if(consume(ID)){
            if(arrayDecl()){}
            if(consume(SEMICOLON))
                return true;
            else
                tkerr("expected ';' after variable definition");
        }else
            tkerr("expected variable name");
    }
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(){
	if(consume(TYPE_INT)){
		return true;
	}
	if(consume(TYPE_DOUBLE)){
		return true;
	}
	if(consume(TYPE_CHAR)){
		return true;
	}
	if(consume(STRUCT)){
		if(consume(ID)){
			return true;
		}else{
            tkerr("expected struct name");
        }
	}
	return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(){
    if(consume(LBRACKET)){
        if(consume(INT)){}
        if(consume(RBRACKET))
            return true;
        else
            tkerr("expected ']' after array declaration or invalid expression in array declaration");
    }
    return false;
}

// fnDef: (typeBase | VOID) ID LPAR (fnParam (COMMA fnParam)*)? RPAR stmCompound
bool fnDef(){
    if(typeBase() || consume(VOID)){
        if(consume(ID)){
            if(consume(LPAR)){
                if(fnParam()){
                    while(consume(COMMA)){
                        if(!fnParam())
                            tkerr("expected parameter after ',' in function definition");
                    }
                }
                if(consume(RPAR)){
                    if(stCompound())
                        return true;
                    else
                        tkerr("expected compound statement after function declaration or invalid expression in function definition");
                }
                else
                    tkerr("expected ')' after function parameters or invalid expression in function definition");
            }
        }else{
            if(consume(LPAR))
                tkerr("missing function identifier");
        }
    }
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(){
    if(typeBase()){
        if(consume(ID)){
            if(arrayDecl()){}
            return true;
        }else
            tkerr("expected parameter name");
    }
    return false;
}

void parse(Token *tokens){
	iTk=tokens;
	if(!unit())
        tkerr("syntax error");
}
