#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"

int main(){
    char *inBuffer = loadFile("tests/testparser.c");

    Token *tokens = tokenize(inBuffer);
    free(inBuffer);

    showTokens(tokens);
    printf("\nParsing...\n");
    parse(tokens);

    free(tokens);
    return 0;
}