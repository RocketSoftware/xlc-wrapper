#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

int main(int argc, char *argv[]) {
    /* malloc */
    char * mptr = malloc(0);
    assert(mptr != NULL);
    free(mptr);
    /* calloc */
    char * cptr1 = calloc(0, 0);
    char * cptr2 = calloc(1, 0);
    char * cptr3 = calloc(0, 1);
    assert(cptr1 != NULL);
    assert(cptr2 != NULL);
    assert(cptr3 != NULL);
    free(cptr1);
    free(cptr2);
    free(cptr3);
    /* realloc */
    /*
    char * rptr1 = malloc(1);
    char * retrptr1 = realloc(rptr, 0);
    */
    char * retrptr2 = realloc(NULL, 0);
    assert(retrptr2 != NULL);
    free(retrptr2);

    printf("Lookin' good.\n");
    return 0;
}
