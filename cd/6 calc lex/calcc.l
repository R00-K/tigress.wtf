%{
#include <stdio.h>
#include <stdlib.h>
int a = 0, b = 0;
char op;
%}

%%
[0-9]+      {
                if(a == 0)
                    a = atoi(yytext);
                else
                    b = atoi(yytext);
            }
[-+*/]      {
                op = yytext[0];
            }
\n          {
                switch(op)
                {
                    case '+':
                        printf("Result = %d\n", a + b);
                        break;

                    case '-':
                        printf("Result = %d\n", a - b);
                        break;

                    case '*':
                        printf("Result = %d\n", a * b);
                        break;

                    case '/':
                        if(b != 0)
                            printf("Result = %d\n", a / b);
                        else
                            printf("Division by zero\n");
                        break;
                }

                a = 0;
                b = 0;
                op = 0;
                printf("Enter expression: ");
            }
[ \t]       ;
.           { printf("Invalid input\n"); }
%%

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