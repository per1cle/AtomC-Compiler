#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"

int main(){
    char *inBuffer = loadFile("tests/testat.c");

    Token *tokens = tokenize(inBuffer);
    free(inBuffer);

    showTokens(tokens);
    printf("\n\n");
    pushDomain(); 
    parse(tokens);
    printf("\n\n");
    showDomain(symTable,"global"); 
    dropDomain();

    free(tokens);
    return 0;
}