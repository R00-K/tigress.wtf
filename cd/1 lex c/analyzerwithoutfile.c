
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int isKeyword(char word[])
{
    char keywords[][10] = {
        "int", "char", "float", "if", "printf"
    };

    for (int i = 0; i < 5; i++)
    {
        if (strcmp(word, keywords[i]) == 0)
            return 1;
    }

    return 0;
}

void checkWord(char word[])
{
    if (word[0] == '\0')
        return;

    if (isKeyword(word))
        printf("%s is a keyword\n", word);
    else if (isdigit(word[0]))
        printf("%s is a number\n", word);
    else
        printf("%s is a identifier\n", word);
}

int main()
{
    char ch;
    char word[100];
    int i = 0;

    printf("Enter a string:\n");

    while ((ch = getchar()) != '\n')
    {
        /* If it is a letter or digit, build the word */
        if (isalnum(ch))
        {
            word[i++] = ch;
        }

        /* Operator */
        else if (ch == '+' || ch == '-' || ch == '*' ||
                 ch == '/' || ch == '^' || ch == '=')
        {
            word[i] = '\0';
            checkWord(word);
            i = 0;

            printf("%c is a operator\n", ch);
        }

        /* Special character */
        else if (ch == '\'' || ch == ';' || ch == '{' ||
                 ch == '}' || ch == '(' || ch == ')')
        {
            word[i] = '\0';
            checkWord(word);
            i = 0;

            printf("%c is a special character\n", ch);
        }

        /* Space */
        else
        {
            word[i] = '\0';
            checkWord(word);
            i = 0;
        }
    }

    /* Check the last word */
    word[i] = '\0';
    checkWord(word);

    return 0;
}

