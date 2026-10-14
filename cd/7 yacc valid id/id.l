%{
#include "y.tab.h"
%}
%%
"if"|"else"|"int"|"float" { return key; }
[a-zA-Z_][a-zA-Z0-9_]* { return letter; }
[0-9]+ { return digit; }
\n { return 0; }
. { return yytext[0]; }
%%
int yywrap()
{
return 1;
}
