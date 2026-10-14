%{
#include <stdio.h>

int vowels, consonants;
%}

%%
[AEIOUaeiou]    { vowels++; }
[a-zA-Z]        { consonants++; }
\n              {
                    printf("Vowels = %d\n", vowels);
                    printf("Consonants = %d\n\n", consonants);

                    vowels = 0;
                    consonants = 0;

                
                }
.               ;
%%

int yywrap()
{
    return 1;
}

int main()
{
    printf("Enter a line:\n");
    yylex();
    return 0;
}

