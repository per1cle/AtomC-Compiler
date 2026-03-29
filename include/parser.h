#pragma once

#include <stdbool.h>
#include "lexer.h"

const char *tkCodeName(int code);
void tkerr(const char *fmt,...);
bool consume(int code);
void parse(Token *tokens);

bool unit();
bool structDef();
bool varDef();
bool typeBase();
bool arrayDecl();
bool fnDef();