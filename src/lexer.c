#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

Token *tokens = NULL;	// single linked list of tokens
Token *lastTk = NULL;		// the last token in list

int line=1;		// the current line in the input file

const char *tokenNames[] = {
    "ID", "TYPE_CHAR", "TYPE_DOUBLE", "ELSE", "IF", "TYPE_INT", "RETURN", "STRUCT", "VOID", "WHILE",
    "INT", "DOUBLE", "CHAR", "STRING", "COMMA", "SEMICOLON", "LPAR", "RPAR", "LBRACKET", "RBRACKET",
    "LACC", "RACC", "END", "ADD", "SUB", "MUL", "DIV", "DOT", "AND", "OR", "NOT", "ASSIGN", "EQUAL",
    "NOTEQ", "LESS", "LESSEQ", "GREATER", "GREATEREQ"
};

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code){
	Token *tk=safeAlloc(sizeof(Token));
	tk->code=code;
	tk->line=line;
	tk->next=NULL;
	if(lastTk){
		lastTk->next=tk;
		}else{
		tokens=tk;
		}
	lastTk=tk;
	return tk;
	}

char *extract(const char *begin,const char *end){  // [start,end)
	size_t len = end - begin;
	char *str = safeAlloc(len + 1);
	strncpy(str, begin, len);
	str[len] = '\0';
	return str;
}

char *extractString(const char *begin, const char *end) {
	size_t maxLen = end - begin;
	char *str = safeAlloc(maxLen + 1);
	size_t j = 0;
	for (const char *p = begin; p < end; p++) {
		if (*p == '\\' && p + 1 < end) {
			p++;
			switch (*p) {
				case 'a': str[j++] = '\a'; break;
				case 'b': str[j++] = '\b'; break;
				case 'f': str[j++] = '\f'; break;
				case 'n': str[j++] = '\n'; break;
				case 'r': str[j++] = '\r'; break;
				case 't': str[j++] = '\t'; break;
				case 'v': str[j++] = '\v'; break;
				case '\\': str[j++] = '\\'; break;
				case '\'': str[j++] = '\''; break;
				case '"': str[j++] = '"'; break;
				case '0': str[j++] = '\0'; break;
				default: str[j++] = *p; break;
			}
		} else {
			str[j++] = *p;
		}
	}
	str[j] = '\0';
	return str;
}

Token *tokenize(const char *pch){
	const char *start;
	Token *tk;
	for(;;){
		switch(*pch){
			case ' ': case '\t': pch++; break;
			case '\r':		// handles different kinds of newlines (Windows: \r\n, Linux: \n, MacOS, OS X: \r or \n)
				if(pch[1]=='\n') pch++;
				// fallthrough to \n
			case '\n':
				line++;
				pch++;
				break;
			case '\0':addTk(END); return tokens;
			case ',':addTk(COMMA); pch++; break;
			case ';': addTk(SEMICOLON); pch++; break;
			case '(': addTk(LPAR); pch++; break;
			case ')': addTk(RPAR); pch++; break;
			case '[': addTk(LBRACKET); pch++; break;
			case ']': addTk(RBRACKET); pch++; break;
			case '{': addTk(LACC); pch++; break;
			case '}': addTk(RACC); pch++; break;
			case '+': addTk(ADD); pch++; break;
			case '-': addTk(SUB); pch++; break;
			case '*': addTk(MUL); pch++; break;
			case '/':
				if(pch[1] == '/'){
					while(*pch && *pch != '\n'){
						pch++;
					}
				}  
				else{
					addTk(DIV);
					pch++;
				}
				break;
			case '.': addTk(DOT); pch++; break;                                                             
			case '=':
				if(pch[1]=='='){
					addTk(EQUAL);
					pch+=2;
				}else{
					addTk(ASSIGN);
					pch++;
				}
				break;
			case '!':
				if(pch[1] == '='){
					addTk(NOTEQ);
					pch+=2;
				}else{
					addTk(NOT);
					pch++;
				}
				break;
			case '<':
				if(pch[1] == '='){
					addTk(LESSEQ);
					pch+=2;
				}else{
					addTk(LESS);
					pch++;
				}
				break;
			case '>':
				if(pch[1] == '='){
					addTk(GREATEREQ);
					pch+=2;
				}else{
					addTk(GREATER);
					pch++;
				}
				break;
			case '&': 
				if(pch[1] == '&'){
					addTk(AND); 
					pch+=2; 
				}
				else {
					err("Missing the second & symbol");
				}
				break;			
			case '|': 
				if(pch[1] == '|'){
					addTk(OR); 
					pch+=2; 
				}
				else {
					err("Missing the second | symbol");
				}
				break;
			case '\'':
                if (pch[1] == '\\' && pch[2] && pch[3] == '\'') {
                    char escapeChar;
                    switch(pch[2]) {
                        case 'a': escapeChar = '\a'; break;
                        case 'b': escapeChar = '\b'; break;
                        case 'f': escapeChar = '\f'; break;
                        case 'n': escapeChar = '\n'; break;
                        case 'r': escapeChar = '\r'; break;
                        case 't': escapeChar = '\t'; break;
                        case 'v': escapeChar = '\v'; break;
                        case '\\': escapeChar = '\\'; break;
                        case '\'': escapeChar = '\''; break;
                        case '"': escapeChar = '"'; break;
                        case '0': escapeChar = '\0'; break;
                        default: err("Invalid escape sequence in CHAR");
                    }
                    tk = addTk(CHAR);
                    tk->value.c = escapeChar;
                    pch += 4;
                } else if (pch[1] && pch[1] != '\\' && pch[1] != '\'' && pch[2] == '\'') {
                    // Regular character: 'c'
                    tk = addTk(CHAR);
                    tk->value.c = pch[1];
                    pch += 3;
                } else {
                    err("Invalid CHAR syntax");
                }
                break;
			case '"':
                start = ++pch;
                while (*pch && *pch != '"') {
                    if (*pch == '\\') {
                        if (pch[1] == 'a' || pch[1] == 'b' || pch[1] == 'f' || 
                            pch[1] == 'n' || pch[1] == 'r' || pch[1] == 't' || 
                            pch[1] == 'v' || pch[1] == '\\' || pch[1] == '\'' || 
                            pch[1] == '"' || pch[1] == '0') {
                            pch += 2;
                        } else {
                            err("Invalid escape sequence in STRING");
                        }
                    } else {
                        pch++;
                    }
                }
                if (*pch == '"') {
                    tk = addTk(STRING);
                    tk->value.text = extractString(start, pch);
                    pch++;
                } else {
                    err("Missing STRING's ending");
                }
                break;
			default:
				if(isalpha(*pch)||*pch=='_'){
					for(start=pch++;isalnum(*pch)||*pch=='_';pch++){}
					char *text=extract(start,pch);
					if(strcmp(text,"char")==0) addTk(TYPE_CHAR);
					else if(strcmp(text,"double")==0) addTk(TYPE_DOUBLE);
					else if(strcmp(text,"else")==0) addTk(ELSE);
					else if(strcmp(text,"if")==0) addTk(IF);
					else if(strcmp(text,"int")==0) addTk(TYPE_INT);
					else if(strcmp(text,"return")==0) addTk(RETURN);
					else if(strcmp(text,"struct")==0) addTk(STRUCT);
					else if(strcmp(text,"void")==0) addTk(VOID);
					else if(strcmp(text,"while")==0) addTk(WHILE);
					else{
						tk = addTk(ID);
						tk->value.text = text;
						}
				}
				else if(isdigit(*pch)){
					start = pch;
					int isDouble = 0;

					while(isdigit(*pch)) pch++;

					if(*pch == '.'){
						isDouble = 1;
						pch++;
						if(!isdigit(*pch)) err("Lipsa parte zecimala");
						while(isdigit(*pch)) pch++;
					}
					if(*pch == 'e' || *pch == 'E'){
						isDouble = 1;
						pch++;
						if(*pch == '+' || *pch == '-') pch++;
						if(!isdigit(*pch)) err("Lipsa parte numerica exponent");
						while (isdigit(*pch)) pch++;
					}

					if(isDouble){
						tk = addTk(DOUBLE);
						tk->value.d = strtod(start, NULL);
					}else{
						tk = addTk(INT);
						tk->value.i = strtol(start, NULL, 10);
					}
				}
				else err("invalid char: %c (%d)",*pch,*pch);
			}
		}
	}

void showTokens(const Token *tokens){   
	for(const Token *tk=tokens;tk;tk=tk->next){
		printf("%d\t%s", tk->line, tokenNames[tk->code]);
        if (tk->code == ID || tk->code == STRING) printf(":%s", tk->value.text);
        else if (tk->code == INT) printf(":%d", tk->value.i);
        else if (tk->code == DOUBLE) printf(":%.2f", tk->value.d);
        else if (tk->code == CHAR) printf(":%c", tk->value.c);
        printf("\n");
	}
}
