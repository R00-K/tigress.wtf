%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
void yyerror(const char *s);
%}

%token ID

%left '+'
%left '*'

%%

expr:
      ID
    | expr '+' expr
    | expr '*' expr
    | '(' expr ')'
    ;

%%

void yyerror(const char *s)
{
}

int main()
{
    printf("Enter the expression: ");

    if (yyparse() == 0)
        printf("valid expression\n");
    else
        printf("invalid expression\n");

    return 0;
}
