%{
#include<stdio.h>

int no_lines = 0;
int no_words = 0;
int no_other_char = 0;
int no_chars = 0;
int total_char = 0;
%}

%%

\n                  { no_lines++; }

[A-Za-z0-9]+        { 
                        no_words++;
                        no_chars += yyleng;
                     }

[ \t]+              { no_other_char += yyleng; }

.                   { no_other_char++; }

%%

int main(int argc,char **argv)
{
    ++argv,--argc;

    if(argc > 0)
        yyin = fopen(argv[0],"r");
    else
        yyin = stdin;

    yylex();

    total_char = no_chars + no_other_char;

    printf("....result is .....\n");
    printf("number of lines=%d\n",no_lines);
    printf("number of words=%d\n",no_words);
    printf("number of alpha neumeric characters=%d\n",no_chars);
    printf("other characters=%d\n",no_other_char);
    printf("total no characters=%d\n",total_char);
}

int yywrap()
{
    return 1;
}
