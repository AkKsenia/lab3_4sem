#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE* mf;
    char str[50];
    char* estr;

    mf = fopen("file_for_saving.txt", "r");
    if (mf == NULL) {
        printf("error\n");
        exit(EXIT_FAILURE);
    }

    estr = fgets(str, sizeof(str), mf);
    if (estr == NULL) {
        if (feof(mf) != 0) {
            printf("reading the file is finished\n");
            exit(EXIT_FAILURE);
        }
        else {
            printf("file reading error\n");
            exit(EXIT_FAILURE);
        }
    }

    char* string = strtok(str, " ");

    if ((strcmp(string, "4") == 0) || (strcmp(string, "5") == 0) || (strcmp(string, "6") == 0))
        printf("test passed!\n");
    else printf("test failed!\n");

    if (fclose(mf) == EOF) printf("error\n");

    exit(EXIT_SUCCESS);
}
