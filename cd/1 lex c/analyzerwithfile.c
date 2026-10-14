#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int isKeyword(char buffer[])
{
    char keywords[32][10] = {
        "auto", "break", "case", "char", "const",
        "continue", "default",
        "do", "double", "else", "enum", "extern",
        "float", "for", "goto",
        "if", "int", "long", "register", "return",
        "short", "signed",
        "sizeof", "static", "struct", "switch",
        "typedef", "union",
        "unsigned", "void", "volatile", "while"
    };

    for (int i = 0; i < 32; i++)
    {
        if (strcmp(keywords[i], buffer) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int main()
{
    FILE *fp;
    char ch;
    char buffer[100];
    char ops[] = "*-/+=%";
    int i, j = 0;

    fp = fopen("test.c", "r");

    if (fp == NULL)
    {
        printf("Error opening the file.\n");
        return 1;
    }

    while ((ch = fgetc(fp)) != EOF)
    {
        /* Check for operators */
        for (i = 0; i < 6; i++)
        {
            if (ch == ops[i])
            {
                printf("%c is an operator\n", ch);
            }
        }

        /* Build identifiers/keywords */
        if (isalnum(ch) || ch == '_')
        {
            buffer[j++] = ch;
        }
        else
        {
            if (j != 0)
            {
                buffer[j] = '\0';
                j = 0;

                if (isKeyword(buffer))
                {
                    printf("%s is a Keyword\n", buffer);
                }
                else
                {
                    printf("%s is an Identifier\n", buffer);
                }
            }
        }
    }

    /* Handle last word if file ends without whitespace */
    if (j != 0)
    {
        buffer[j] = '\0';

        if (isKeyword(buffer))
        {
            printf("%s is a Keyword\n", buffer);
        }
        else
        {
            printf("%s is an Identifier\n", buffer);
        }
    }

    fclose(fp);

    return 0;
}
