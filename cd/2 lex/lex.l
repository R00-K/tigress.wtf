%{
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
%}

letter[a-zA-Z]
digit[0-9]
%%
"int"|"char"|"float"|"if"|"printf"  {printf("%s is a keyword\n",yytext);}
"+"|"-"|"*"|"/"|"^"|"="     	    {printf("%s is a operator\n",yytext);}
"'"|";"|"{"|"}"|"("|")"  	    {printf("%s is a special character\n",yytext);}
[0-9]+				    {printf("%s is a number\n",yytext);}
{letter}({letter}|{digit})*         {printf("%s is a identifier\n",yytext);}
%%
void main()
{
 printf("Enter a string");
 yylex();
 }
 
 int yywrap()
 {
 return 1;
 }
