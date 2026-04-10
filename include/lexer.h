#pragma once

typedef enum{   
	ID,     // [a-zA-Z_][a-zA-Z0-9_]*
	// keywords
	TYPE_CHAR,     // 'char'
	TYPE_DOUBLE,     // 'double'
	ELSE,     // 'else'    
	IF,     // 'if'
	TYPE_INT,     // 'int' 
	RETURN,     // 'reurn'
	STRUCT,     // 'struct'
	VOID,    // 'void'
	WHILE,     // 'while'
	// constants
	INT,     // [0-9]+
	DOUBLE,     // [0-9]+('.'[0-9]+([eE][+-]?[0-9]+)?|('.'[0-9]+)?[eE][+-]?[0-9]+)
	CHAR,     // [']([^'\\]|[\\][abfnrtv\\'"0])[']
	STRING,    // ["]([^"\\]|[\\][abfnrtv\\'"0])*["]
	// delimiters
	COMMA,     // ','
	SEMICOLON,     // ';'
	LPAR,     // '('
	RPAR,     // ')'
	LBRACKET,     // '['
	RBRACKET,     // ']'
	LACC,     // '{'
	RACC,     // '}'
	END,    // '\0' | EOF
	// operators
	ADD,     // '+'
	SUB,     // '-'
	MUL,     // '*'
	DIV,     // '/'
	DOT,     // '.'
	AND,     // '&&'
	OR,     // '||'
	NOT,     // '!'
	ASSIGN,     // '='
	EQUAL,     // '=='
	NOTEQ,     // '!='
	LESS,     // '<'
	LESSEQ,     // '<='
	GREATER,     // '>'
	GREATEREQ,    // '>='
	}TokenType;

#define NUM_TOKEN_TYPES 38  // Total number of token types

typedef struct Token{
	TokenType code;		// ID, TYPE_CHAR, ...
	int line;		// the line from the input file
	union{
		char *text;		// the chars for ID, STRING (dynamically allocated)
		int i;		// the value for INT
		char c;		// the value for CHAR
		double d;		// the value for DOUBLE
	}value;
	struct Token *next;		// next token in a simple linked list
}Token;

Token *tokenize(const char *pch);
void showTokens(const Token *tokens);
