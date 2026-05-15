#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "ad.h"
#include "at.h"

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
    //printf("consume(%s)", tkCodeName(code));
	if(iTk->code==code){
		consumedTk=iTk;
		iTk=iTk->next;
        //printf(" => consumed\n");
		return true;
	}
    //printf(" => found %s\n", tkCodeName(iTk->code));
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
    Ret rCond, rExpr;
    if(stmCompound(true)){
        return true;
    }
    if(consume(IF)){
        if(consume(LPAR)){
            if(expr(&rCond)){
                if(!canBeScalar(&rCond))
                    tkerr("the if condition must be a scalar value");
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
            if(expr(&rCond)){
                if(!canBeScalar(&rCond))
                    tkerr("the while condition must be a scalar value");
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
        if(expr(&rExpr)){
            if(owner->type.tb == TB_VOID)
                tkerr("a void function cannot return a value");
            if(!canBeScalar(&rExpr))
                tkerr("the return value must be a scalar value");
            if(!convTo(&rExpr.type, &owner->type))
                tkerr("cannot convert the return expression type to the function return type");
        }else{
            if(owner->type.tb != TB_VOID)
                tkerr("a non-void function must return a value");
        }
        if(consume(SEMICOLON)){
            return true;
        }else{
            tkerr("expected ';' after return statement");
        }
    }
    if(expr(&rExpr)){
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
bool expr(Ret *r){
    if(exprAssign(r)){
        return true;
    }
    return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r){
    Token *start = iTk;
    Ret rDst;
    if(exprUnary(&rDst)){
        if(consume(ASSIGN)){
            if(exprAssign(r)){
                if (!rDst.lval)
					tkerr("the assign destination must be a left-value");
				if (rDst.ct)
					tkerr("the assign destination cannot be constant");
				if (!canBeScalar(&rDst))
					tkerr("the assign destination must be scalar");
				if (!canBeScalar(r))
					tkerr("the assign source must be scalar");
				if (!convTo(&r->type, &rDst.type))
					tkerr("the assign source cannot be converted to the destination");
				r->lval = false;
				r->ct = true;
                return true;
            }else{
                tkerr("invalid or missing expression after '='");
            }
        }
        iTk = start;
    }
    if(exprOr(r)){
        return true;
    }
    iTk = start;
    return false;
}

// exprOr: exprOr OR exprAnd | exprAnd   =>  exprOr: exprAnd exprOrPrim 
bool exprOr(Ret *r){
    Token *start = iTk;
    if(exprAnd(r)){
        if(exprOrPrim(r)){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | ε
bool exprOrPrim(Ret *r){
    Token *start = iTk;
    if(consume(OR)){
        Ret right;
        if(exprAnd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst))
                tkerr("invalid operand type for '||'");
            *r = (Ret){{TB_INT,NULL,-1},false,true};
            if(exprOrPrim(r)){
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
bool exprAnd(Ret *r){
    Token *start = iTk;
    if(exprEq(r)){
        if(exprAndPrim(r)){
            return true;
        }
    }
    iTk = start;   
    return false;
}

// exprAndPrim: AND exprEq exprAndPrim | ε
bool exprAndPrim(Ret *r){
    Token *start = iTk;
    if(consume(AND)){
        Ret right;
        if(exprEq(&right)){
            Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '&&' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprAndPrim(r)){
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
bool exprEq(Ret *r){
    Token *start = iTk;
    if(exprRel(r)){
        if(exprEqPrim(r)){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprEqPrim: (EQUAL | NOTEQ) exprRel exprEqPrim | ε
bool exprEqPrim(Ret *r)
{
	Token *start = iTk;
	if (consume(EQUAL))
	{
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '==' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprEqPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '==' operator");
		}
	}
	if (consume(NOTEQ))
	{
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '!=' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprEqPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '!=' operator");
		}
	}
	iTk = start;
	return true;
}

// exprRel: exprRel (LESS | LESSEQ | GREATER | GREATEREQ) exprAdd | exprAdd  =>  exprRel: exprAdd exprRelPrim
bool exprRel(Ret *r){
    Token *start = iTk;
    if(exprAdd(r)){
        if(exprRelPrim(r)){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprRelPrim: (LESS | LESSEQ | GREATER | GREATEREQ) exprAdd exprRelPrim | ε
bool exprRelPrim(Ret *r)
{
	if (consume(LESS))
	{
		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '<' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '<' operator");
		}
	}
	if (consume(LESSEQ))
	{
		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '<=' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '<=' operator");
		}
	}
	if (consume(GREATER))
	{
		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '>' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '>' operator");
		}
	}
	if (consume(GREATEREQ))
	{
		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '>=' operator");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '>=' operator");
		}
	}
	return true;
}

// exprAdd: exprAdd (ADD | SUB) exprMul | exprMul  =>  exprAdd: exprMul exprAddPrim
bool exprAdd(Ret *r){
    Token *start = iTk;
    if(exprMul(r)){
        if(exprAddPrim(r)){
            return true;
        }
    }
    iTk = start;
    return false;
}

// exprAddPrim: (ADD | SUB) exprMul exprAddPrim | ε
bool exprAddPrim(Ret *r)
{
	if (consume(ADD))
	{
		Ret right;
		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '+' operator");
			*r = (Ret){tDst, false, true};
			if (exprAddPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '+' operator");
		}
	}
	if (consume(SUB))
	{
		Ret right;
		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '-' operator");
			*r = (Ret){tDst, false, true};
			if (exprAddPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	return true;
}

// exprMul: exprMul (MUL | DIV) exprCast | exprCast  =>  exprMul: exprCast exprMulPrim
bool exprMul(Ret *r){
    if(exprCast(r)){
        if(exprMulPrim(r)){
            return true;
        }
    }
    return false;
}

// exprMulPrim: (MUL | DIV) exprCast exprMulPrim | ε
bool exprMulPrim(Ret *r)
{
	if (consume(MUL))
	{
		Ret right;
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '*' operator");
			*r = (Ret){tDst, false, true};
			if (exprMulPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '*' operator");
		}
	}
	if (consume(DIV))
	{
		Ret right;
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))
				tkerr("invalid operand type for '/' operator");
			*r = (Ret){tDst, false, true};
			if (exprMulPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("missing or invalid expression after '/' operator");
		}
	}
	return true;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret *r){
    Token *start = iTk;
    if(consume(LPAR)){
        Type t;
        Ret op;
        if(typeBase(&t)){
            if(arrayDecl(&t)){}
            if(consume(RPAR)){
                if(exprCast(&op)){
                    if (t.tb == TB_STRUCT)
						tkerr("cannot convert to a struct type");
					if (op.type.tb == TB_STRUCT)
						tkerr("cannot convert a struct");
					if (op.type.n >= 0 && t.n < 0)
						tkerr("an array can only be converted to another array");
					if (op.type.n < 0 && t.n >= 0)
						tkerr("a scalar can only be converted to another scalar");
					*r = (Ret){t, false, true};
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
    if(exprUnary(r)){
        return true;
    }
    
    iTk = start;
    return false;
}

// exprUnary: (SUB | NOT) exprUnary | exprPostfix
bool exprUnary(Ret *r)
{
	Token *start = iTk;
	if (consume(SUB))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r))
				tkerr("unary '-' operator must have a scalar operand");
			r->lval = false;
			r->ct = true;
			return true;
		}
		else
		{
			tkerr("missing or invalid expression after '-' operator");
		}
	}
	if (consume(NOT))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r))
				tkerr("unary '!' operator must have a scalar operand");
			r->lval = false;
			r->ct = true;
			return true;
		}
		else
		{
			tkerr("missing or invalid expression after '!' operator");
		}
	}
	iTk = start;
	if (exprPostfix(r))
	{
		return true;
	}
	iTk = start;
	return false;
}

// exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary   =>  exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(Ret *r)
{
	Token *start = iTk;
	if (exprPrimary(r))
	{
		if (exprPostfixPrim(r))
		{
			return true;
		}
	}
	iTk = start;
	return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | ε
bool exprPostfixPrim(Ret *r)
{
	Token *start = iTk;
	if (consume(LBRACKET))
	{
		Ret idx;
		if (expr(&idx))
		{
			if (consume(RBRACKET))
			{
				if (r->type.n < 0)
					tkerr("only an array can be indexed");
				Type tInt = {TB_INT, NULL, -1};
				if (!convTo(&idx.type, &tInt))
					tkerr("the array index is not convertible to int");
				r->type.n = -1;
				r->lval = true;
				r->ct = false;
				if (exprPostfixPrim(r))
				{
					return true;
				}
			}
			else
			{
				tkerr("invalid expression between [...] or missing ']'");
			}
		}
	}
	iTk = start;
	if (consume(DOT))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;
			if (r->type.tb != TB_STRUCT)
				tkerr("a field can only be selected from a struct");
			Symbol *s = findSymbolInList(r->type.s->structMembers, tkName->value.text);
			if (!s)
				tkerr("the struct %s does not have a field %s", r->type.s->name, tkName->value.text);
			*r = (Ret){s->type, true, s->type.n >= 0};
			if (exprPostfixPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("invalid or missing identifier after '.'");
		}
	}
	iTk = start;
	return true;
}

// exprPrimary: ID (LPAR (expr (COMMA expr)*)? RPAR)? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r){
    Token *start = iTk;
    if(consume(ID)){
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->value.text);
        if(!s)
            tkerr("undefined id: %s", tkName->value.text);
        if(consume(LPAR)){
            if(s->kind != SK_FN) 
                tkerr("only a function can be called");
            Ret rArg;
            Symbol *param = s->fn.params;
            if(expr(&rArg)){
                if (!param)
                    tkerr("too many arguments in function call");
                if (!convTo(&rArg.type, &param->type))
					tkerr("cannot convert the argument type to the parameter type during function call");
                param=param->next;
                while(consume(COMMA)){
                    if(expr(&rArg)){
                        if (!param)
                            tkerr("too many arguments in function call");
                        if (!convTo(&rArg.type, &param->type))
                            tkerr("cannot convert the argument type to the parameter type during function call");
                        param=param->next;
                    }
                    else
                        tkerr("invalid or missing expression after ','");    
                }
            }
            if(consume(RPAR)){
                if(param)
                    tkerr("too few arguments in function call");
                *r = (Ret){s->type, false, true};
                return true;
            }else{
                tkerr("expected ')' after function call arguments or invalid expression in function call");
            }
        }
        else{
            if(s->kind == SK_FN)
                tkerr("A function can only be called");
            *r = (Ret){s->type, true, s->type.n >= 0};
        }
        return true; // variable or function name without arguments
    }
   if (consume(INT))
	{
		*r = (Ret){{TB_INT, NULL, -1}, false, true};
		return true;
	}
	if (consume(DOUBLE))
	{
		*r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
		return true;
	}
	if (consume(CHAR))
	{
		*r = (Ret){{TB_CHAR, NULL, -1}, false, true};
		return true;
	}
	if (consume(STRING))
	{
		*r = (Ret){{TB_CHAR, NULL, 0}, false, true};
		return true;
	}
	if (consume(LPAR))
	{
		if (expr(r))
		{
			if (consume(RPAR))
			{
				return true;
			}
			else
			{
				tkerr("missing or invalid expression between \"(...)\" or missing ')'");
			}
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
