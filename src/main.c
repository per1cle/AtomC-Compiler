#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

int main(){
    char *inBuffer = loadFile("tests/testlex.c");

    Token *tokens = tokenize(inBuffer);
    free(inBuffer);

    showTokens(tokens);

    free(tokens);
    return 0;
}