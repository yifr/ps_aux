#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char word[] = "123299 jasdf l 1209823 asd ";
    char del[2] = " ";
    char *tok = strtok(word, del);

    while(tok != NULL) {
        printf("%s\n", tok);
        tok = strtok(NULL, del);
    }
}
