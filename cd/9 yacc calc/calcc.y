%{
#include <stdio.h>
#include <ctype.h>
int yylex();
void yyerror(char *s);
%}

%token NUMBER
%left '+' '-'
%left '*' '/'

%%

input:
    expression '\n' { printf("Result = %d\n", $1); }
    ;

expression:
      expression '+' expression { $$ = $1 + $3; }
    | expression '-' expression { $$ = $1 - $3; }
    | expression '*' expression { $$ = $1 * $3; }
    | expression '/' expression { $$ = $1 / $3; }
    | '(' expression ')'        { $$ = $2; }
    | NUMBER                    { $$ = $1; }
    ;

%%

int yylex()
{
    int c;

    while ((c = getchar()) == ' ' || c == '\t');

    if (isdigit(c))
    {
        ungetc(c, stdin);
        scanf("%d", &yylval);
        return NUMBER;
    }

    return c;
}

void yyerror(char *s)
{
    printf("Invalid expression\n");
}

int main()
{
    printf("Enter expression: ");
    yyparse();
    return 0;
}