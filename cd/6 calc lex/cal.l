%{
#include <stdio.h>
#include <stdlib.h>

float a = 0, b = 0;
int op = 0;

void perform_operation();
%}

digit   [0-9]+(\.[0-9]+)?
add     "+"
sub     "-"
mul     "*"
div     "/"
eq      "="
ws      [ \t\n]+

%%

{digit} {
    if (op == 0)
        a = atof(yytext);
    else {
        b = atof(yytext);
        perform_operation();
    }
}

{add}   { op = 1; }
{sub}   { op = 2; }
{mul}   { op = 3; }
{div}   { op = 4; }

{eq} {
    printf("Result : %.2f\n", a);
    a = 0;
    b = 0;
    op = 0;
}

{ws}    { /* ignore whitespace */ }

. {
    printf("Unknown character: %s\n", yytext);
}

%%

void perform_operation()
{
    switch (op)
    {
        case 1:
            a = a + b;
            break;

        case 2:
            a = a - b;
            break;

        case 3:
            a = a * b;
            break;

        case 4:
            if (b != 0)
                a = a / b;
            else
                printf("Error: Division by zero\n");
            break;
    }

    b = 0;
}

int main()
{
    printf("Enter expression: ");
    yylex();
    return 0;
}

int yywrap()
{
    return 1;
}
