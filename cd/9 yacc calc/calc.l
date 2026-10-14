%{
#include "y.tab.h"
#include <stdlib.h>
%}

digit [0-9]+(\.[0-9]+)?

%%

{digit}     {
                yylval.value = atof(yytext);
                return NUMBER;
            }

"+"         { return ADD; }
"-"         { return SUB; }
"*"         { return MUL; }
"/"         { return DIV; }

"("         { return LPAREN; }
")"         { return RPAREN; }

[ \t]+      ;

\n          { return EOL; }

.           {
                printf("Unknown character: %s\n", yytext);
            }

%%

int yywrap()
{
    return 1;
}
