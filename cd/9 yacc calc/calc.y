%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
void yyerror(const char *s);
%}

%union {
    float value;
}

%token <value> NUMBER
%token ADD SUB MUL DIV
%token LPAREN RPAREN
%token EOL

%left ADD SUB
%left MUL DIV

%type <value> expression

%%

input:
      /* empty */
    | input line
    ;

line:
      expression EOL
      {
          printf("Result: %f\n", $1);
          printf("Enter expression:\n");
      }
    ;

expression:
      expression ADD expression
      {
          $$ = $1 + $3;
      }

    | expression SUB expression
      {
          $$ = $1 - $3;
      }

    | expression MUL expression
      {
          $$ = $1 * $3;
      }

    | expression DIV expression
      {
          if ($3 != 0)
              $$ = $1 / $3;
          else
          {
              printf("Error: division by zero\n");
              $$ = 0;
          }
      }

    | LPAREN expression RPAREN
      {
          $$ = $2;
      }

    | NUMBER
      {
          $$ = $1;
      }
    ;

%%

void yyerror(const char *s)
{
    printf("Error: syntax error\n");
}

int main()
{
    printf("Enter expression:\n");
    yyparse();

    return 0;
}
