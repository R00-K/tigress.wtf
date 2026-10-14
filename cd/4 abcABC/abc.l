%{
#include <stdio.h>
%}
%%
abc     { printf("ABC"); }
.       { printf("%s", yytext); }
\n      { printf("\n"); }
%%
int yywrap(){
    return 1;
}
int main(){
    char filename[100];
    FILE *file;

    printf("Enter the name of the input file: ");
    scanf("%s", filename);
    file = fopen(filename, "r");

    if(!file){
        printf("Could not open file %s\n", filename);
        return 1;
    }

    yyin = file;

    printf("\nConverted output:\n");
    yylex();

    fclose(file);
    return 0;
}