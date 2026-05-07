#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "ad.h"

extern const char *tokenNames[];

const char *tkCodeName(int code){
    if(code >= 0 && code < NUM_TOKEN_TYPES)
        return tokenNames[code];
    return "UNKNOWN";
}

Token *iTk;		// the iterator in the tokens list
Token *consumedTk;		// the last consumed token
Symbol *owner = NULL;

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
    Token *startTk = iTk;  // save position for backtracking
    if(consume(STRUCT)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(consume(LACC)){
                Symbol *s = findSymbolInDomain(symTable, tkName->value.text);
                if(s)
                    tkerr("symbol redefinition: %s", tkName->value.text);
                s = addSymbolToDomain(symTable, newSymbol(tkName->value.text, SK_STRUCT));
                s->type.tb = TB_STRUCT;
				s->type.s = s;
				s->type.n = -1;
				pushDomain();
				owner = s;
               for(;;){
                if(varDef());
                else break;
               }
               if(consume(RACC)){
                if(consume(SEMICOLON))
                {
                    owner = NULL;
                    dropDomain();
                    return true;
                }
                else
                    tkerr("expected ';' after struct definition");
               }else
                    tkerr("expected '}' after struct definition or invalid expression in struct definition"); 
            }
        }
    }
    iTk = startTk;  
    return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON 
bool varDef(){
    Token *start = iTk;
    Type t;
    if(typeBase(&t)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(arrayDecl(&t)){
                if(t.n == 0)
                    tkerr("a vector variable must have a specified dimension");
            }
            if(consume(SEMICOLON)){
                Symbol *var = findSymbolInDomain(symTable, tkName->value.text);
				if (var)
					tkerr("symbol redefinition: %s", tkName->value.text);
				var = newSymbol(tkName->value.text, SK_VAR);
				var->type = t;
				var->owner = owner;
				addSymbolToDomain(symTable, var);
				if (owner)
				{
					switch (owner->kind)
					{
					case SK_FN:
						var->varIdx = symbolsLen(owner->fn.locals);
						addSymbolToList(&owner->fn.locals, dupSymbol(var));
						break;
					case SK_STRUCT:
						var->varIdx = typeSize(&owner->type);
						addSymbolToList(&owner->structMembers, dupSymbol(var));
						break;
					default:
						break;
					}
				}else{
                    var->varMem = safeAlloc(typeSize(&t));
                }
                return true;
            }
            else
                tkerr("missing ';' in variable declaration or invalid syntax/missing '(' for function");
        }else
            tkerr("invalid syntax: expected variable/function identifier or missing '{' for struct definition");
    }
    iTk = start;
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t){
    t->n = -1;
    Token *start = iTk;
	if(consume(TYPE_INT)){
        t->tb = TB_INT;
		return true;
	}
	if(consume(TYPE_DOUBLE)){
        t->tb = TB_DOUBLE;
		return true;
	}
	if(consume(TYPE_CHAR)){
        t->tb = TB_CHAR;
		return true;
	}
	if(consume(STRUCT)){
		if(consume(ID)){
			Token *tkName = consumedTk;
			t->tb = TB_STRUCT;
			t->s = findSymbol(tkName->value.text);
			if (!t->s)
			{
				tkerr("structure %s is not defined", tkName->value.text);
			}
			return true;
		}else{
            tkerr("expected struct name");
        }
	}
    iTk = start;
	return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t){
    Token *start = iTk;
    if(consume(LBRACKET)){
        if(consume(INT)){
            Token *tkSize = consumedTk;
            t->n = tkSize->value.i;
        }else{
            t->n = 0; 
        }
        if(consume(RBRACKET))
            return true;
        else
            tkerr("expected ']' after array declaration or invalid expression in array declaration");
    }
    iTk = start;
    return false;
}

// fnDef: (typeBase | VOID) ID LPAR (fnParam (COMMA fnParam)*)? RPAR stmCompound
bool fnDef(){
    Token *startTk = iTk;  
    Type t;
    bool consumedVoidTk = false;
    if(typeBase(&t) || (consumedVoidTk = consume(VOID))){
        if(consumedVoidTk)
            t.tb = TB_VOID;
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(consume(LPAR)){
                Symbol *fn = findSymbolInDomain(symTable, tkName->value.text);
				if (fn)
					tkerr("symbol redefinition: %s", tkName->value.text);
				fn = newSymbol(tkName->value.text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
                if(fnParam()){
                    while(consume(COMMA)){
                        if(!fnParam())
                            tkerr("expected parameter after ',' in function definition");
                    }
                }
                if(consume(RPAR)){
                    if(stmCompound(false)){
                        dropDomain();
                        owner = NULL;
                        return true;
                    }
                    else
                        tkerr("expected '{' at the beginning of function body or invalid expression in function definition");
                }
                else
                    tkerr("expected ')' after function parameters or invalid expression in function definition");
            }
        }/*else{
            if(consume(LPAR))
                tkerr("missing function identifier");
        }*/
    }
    iTk = startTk; 
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(){
    Token *start = iTk;
    Type t;
    if(typeBase(&t)){
        if(consume(ID)){
            Token *tkName = consumedTk;
             if(arrayDecl(&t)){
                t.n = 0;
            }
            Symbol *param = findSymbolInDomain(symTable, tkName->value.text);
			if (param)
				tkerr("symbol redefinition: %s", tkName->value.text);
			param = newSymbol(tkName->value.text, SK_PARAM);
			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			addSymbolToDomain(symTable, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
            return true;
        }else
            tkerr("expected parameter name");
    }
    iTk = start;
    return false;
}

// stm: stmCompound | IF LPAR expr RPAR stm (ELSE stm)? | WHILE LPAR expr RPAR stm | RETURN expr? SEMICOLON | expr? SEMICOLON
bool stm(){
    Token *start = iTk;
    if(stmCompound(true)){
        return true;
    }
    if(consume(IF)){
        if(consume(LPAR)){
            if(expr()){
                if(consume(RPAR)){
                    if(stm()){
                        if(consume(ELSE)){
                            if(stm())
                                return true;
                            else
                                tkerr("expected statement after 'else' or invalid expression in statement");
                            return true;
                        }
                    }else{
                        tkerr("expected statement after 'if' condition or invalid expression in statement");
                    }
                }else{
                    tkerr("expected ')' after 'if' condition or invalid expression in statement");
                }
            }else{
                tkerr("missing condition for 'if' statement");
            }
        }else{
            tkerr("expected '(' after 'if'");
        }
    }
    if(consume(WHILE)){
        if(consume(LPAR)){
            if(expr()){
                if(consume(RPAR)){
                    if(stm()){
                        return true;
                    }else{
                        tkerr("missing statement after 'while' condition");
                    }
                }else{
                    tkerr("expected ')' after 'while' condition or invalid expression in statement");
                }
            }else{
                tkerr("missing condition for 'while' statement");
            }
        }else{
            tkerr("expected '(' after 'while'");
        }
    }
    if(consume(RETURN)){
        if(expr()){}
        if(consume(SEMICOLON)){
            return true;
        }else{
            tkerr("expected ';' after return statement");
        }
    }
    if(expr()){
        if(consume(SEMICOLON)){
            return true;
        }else{
            tkerr("expected ';' after expression statement");
        }
    }
    if(consume(SEMICOLON)){
        return true; // empty statement
    }
    iTk = start;
    return false;
}

// stmCompound: LACC (varDef | stm)* RACC
bool stmCompound(bool newDomain){
    Token *start = iTk;
    if(consume(LACC)){
        if(newDomain)
            pushDomain();
        for(;;){
            if(varDef()){}
            else if(stm()){}
            else break;
        }
        if(consume(RACC)){
            if(newDomain)
                dropDomain();
            return true;
        }else{
            tkerr("expected '}' after compound statement or invalid expression in compound statement");
        }
    }
    iTk = start;
    return false;
}

// expr: exprAssign
bool expr(){
    if(exprAssign()){
        return true;
    }
    return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(){
    Token *start = iTk;
    if(exprUnary()){
        if(consume(ASSIGN)){
            if(exprAssign()){
                return true;
            }else{
                tkerr("invalid or missing expression after '='");
            }
        }
        iTk = start;
    }
    if(exprOr()){
        return true;
    }
    iTk = start;
    return false;
}

// exprOr: exprOr OR exprAnd | exprAnd   =>  exprOr: exprAnd exprOrPrim 
bool exprOr(){
    Token *start = iTk;
    if(exprAnd()){
        if(exprOrPrim()){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | ε
bool exprOrPrim(){
    Token *start = iTk;
    if(consume(OR)){
        if(exprAnd()){
            if(exprOrPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after '||'");
            }
        }else{
            tkerr("invalid or missing expression after '||'");
        }
    }
    iTk = start;
    return true;
}

// exprAnd: exprAnd AND exprEq | exprEq  =>  exprAnd: exprEq exprAndPrim
bool exprAnd(){
    Token *start = iTk;
    if(exprEq()){
        if(exprAndPrim()){
            return true;
        }
    }
    iTk = start;   
    return false;
}

// exprAndPrim: AND exprEq exprAndPrim | ε
bool exprAndPrim(){
    Token *start = iTk;
    if(consume(AND)){
        if(exprEq()){
            if(exprAndPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after '&&'");
            }
        }else{
            tkerr("invalid or missing expression after '&&'");
        }
    }
    iTk = start;
    return true;
}

// exprEq: exprEq (EQUAL | NOTEQ) exprRel | exprRel  =>  exprEq: exprRel exprEqPrim
bool exprEq(){
    Token *start = iTk;
    if(exprRel()){
        if(exprEqPrim()){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprEqPrim: (EQUAL | NOTEQ) exprRel exprEqPrim | ε
bool exprEqPrim(){
    Token *start = iTk;
    if(consume(EQUAL) || consume(NOTEQ)){
        if(exprRel()){
            if(exprEqPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after '==' or '!='");
            }
        }else{
            tkerr("invalid or missing expression after '==' or '!='");
        }
    }
    iTk = start;
    return true;
}

// exprRel: exprRel (LESS | LESSEQ | GREATER | GREATEREQ) exprAdd | exprAdd  =>  exprRel: exprAdd exprRelPrim
bool exprRel(){
    Token *start = iTk;
    if(exprAdd()){
        if(exprRelPrim()){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprRelPrim: (LESS | LESSEQ | GREATER | GREATEREQ) exprAdd exprRelPrim | ε
bool exprRelPrim(){
    Token *start = iTk;
    if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)){
        if(exprAdd()){
            if(exprRelPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after relational operator");
            }
        }else{
            tkerr("invalid or missing expression after relational operator");
        }
    }
    iTk = start;
    return true;
}

// exprAdd: exprAdd (ADD | SUB) exprMul | exprMul  =>  exprAdd: exprMul exprAddPrim
bool exprAdd(){
    Token *start = iTk;
    if(exprMul()){
        if(exprAddPrim()){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprAddPrim: (ADD | SUB) exprMul exprAddPrim | ε
bool exprAddPrim(){
    //Token *start = iTk;
    if(consume(ADD) || consume(SUB)){
        if(exprMul()){
            if(exprAddPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after '+' or '-'");
            }
        }else{
            tkerr("invalid or missing expression after '+' or '-'");
        }
    }
    //iTk = start;
    return true;
}

// exprMul: exprMul (MUL | DIV) exprCast | exprCast  =>  exprMul: exprCast exprMulPrim
bool exprMul(){
    if(exprCast()){
        if(exprMulPrim()){
            return true;
        }
    }
    return false;
}

// exprMulPrim: (MUL | DIV) exprCast exprMulPrim | ε
bool exprMulPrim(){
    if(consume(MUL) || consume(DIV)){
        if(exprCast()){
            if(exprMulPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after '*' or '/'");
            }
        }else{
            tkerr("invalid or missing expression after '*' or '/'");
        }
    }
    return true;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(){
    Token *start = iTk;
    if(consume(LPAR)){
        Type t;
        if(typeBase(&t)){
            if(arrayDecl(&t)){}
            if(consume(RPAR)){
                if(exprCast()){
                    return true;
                }else{
                    tkerr("invalid or missing expression after cast");
                }
            }else{
                tkerr("expected ')' after cast type or invalid expression in cast");
            }
        }
        // Daca LPAR a fost gasit dar n-am dat de un tip (typeBase e false),
        // ignoram si lasam sa se intoarca la start pentru a incerca exprUnary.
    }
    
    iTk = start;
    if(exprUnary()){
        return true;
    }
    
    iTk = start;
    return false;
}

// exprUnary: (SUB | NOT) exprUnary | exprPostfix
bool exprUnary(){
    Token *start = iTk;
    if(consume(SUB) || consume(NOT)){
        if(exprUnary()){
            return true;
        }else{
            tkerr("invalid or missing expression after unary operator");
        }
    }iTk = start;
        if(exprPostfix()){
            return true;
        }
    
    iTk = start;
    return false;
}

// exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary   =>  exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(){
    Token *start = iTk;
    if(exprPrimary()){
        if(exprPostfixPrim()){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | ε
bool exprPostfixPrim(){
    Token *start = iTk;
    if(consume(LBRACKET)){
        if(expr()){
            if(consume(RBRACKET)){
                if(exprPostfixPrim()){
                    return true;
                }else{
                    tkerr("invalid or missing expression after array access");
                }
            }else{
                tkerr("expected ']' after array access or invalid expression in array access");
            }
        }else{
            tkerr("invalid or missing expression after '[' in array access");
        }
    }iTk = start;
    if(consume(DOT)){
        if(consume(ID)){
            if(exprPostfixPrim()){
                return true;
            }else{
                tkerr("invalid or missing expression after member access");
            }
        }else{
            tkerr("expected member name after '.' in member access");
        }
    }
    iTk = start;
    return true;
}

// exprPrimary: ID (LPAR (expr (COMMA expr)*)? RPAR)? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(){
    Token *start = iTk;
    if(consume(ID)){
        if(consume(LPAR)){
            if(expr()){
                while(consume(COMMA)){
                    if(!expr())
                        tkerr("expected expression after ',' in function call");
                }
            }
            if(consume(RPAR)){
                return true;
            }else{
                tkerr("expected ')' after function call arguments or invalid expression in function call");
            }
        }
        return true; // variable or function name without arguments
    }
    if(consume(INT) || consume(DOUBLE) || consume(CHAR) || consume(STRING)){
        return true;
    }
    if(consume(LPAR)){
        if(expr()){
            if(consume(RPAR)){
                return true;
            }else{
                tkerr("expected ')' after expression or invalid expression in parentheses");
            }
        }else{
            tkerr("invalid or missing expression after '('");
        }
    }
    iTk = start;
    return false;
}


void parse(Token *tokens){    
	iTk=tokens;
	if(!unit())
        tkerr("syntax error: unexpected token at global scope. Expected 'struct', a type or 'void'");
}
