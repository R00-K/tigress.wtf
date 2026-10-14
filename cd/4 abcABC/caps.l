%{
#include <stdio.h>
#include <ctype.h>
%}

%%
[a-z]      { putchar(toupper(yytext[0])); }
.|\n       { putchar(yytext[0]); }
%%

int main(int argc, char *argv[])
{
    if (argc > 1)
        yyin = fopen(argv[1], "r");
    else
        yyin = stdin;

    if (yyin == NULL)
    {
        printf("Cannot open file\n");
        return 1;
    }

    yylex();

    if (yyin != stdin)
        fclose(yyin);

    return 0;
}

int yywrap()
{
    return 1;
}

