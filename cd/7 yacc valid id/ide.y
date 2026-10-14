%{
#include <stdio.h>
#include <string.h>
int yylex(void);
int yyerror(char *s);
%}
%token LETTER DIGIT
%%
S : LETTER R
  ;
R : LETTER R
  | DIGIT R
  |
  ;
%%
int main(){

    printf("Enter an identifier: ");

    if (yyparse() == 0)
        printf("Valid Identifier\n");
    return 0;
}
int yyerror(char *s){

    printf("Invalid Identifier\n");
    return 0;
}