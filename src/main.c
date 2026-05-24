#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "utils.h"
#include "parser.h"
#include "ad.h"

int main(){
    char *inBuffer = loadFile("tests/testgc.c");

    Token *tokens = tokenize(inBuffer);
    free(inBuffer);

    showTokens(tokens);

    printf("\n\n");

    pushDomain(); 
    vmInit();

    parse(tokens);
    printf("\n\n");

    Instr *test = genTestProgram2();
    run(test);
    /*
    Symbol *symMain=findSymbolInDomain(symTable,"main");
    if(!symMain)err("missing main function");
    Instr *entryCode=NULL;
    addInstr(&entryCode,OP_CALL)->arg.instr=symMain->fn.instr;
    addInstr(&entryCode,OP_HALT);
    run(entryCode);
    */

    //showDomain(symTable,"global"); 
    dropDomain();

    free(tokens);
    return 0;
}