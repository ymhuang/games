#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define _DEBUG 1

int main(void)
{
    int i = 0;
    int j = 0;
    int A = 0;
    int B = 0;
    int counter = 0;
    char gennum;
    char answer[5] = { '\0', '\0', '\0', '\0', '\0'};
    char guess[5] = { '\0', '\0', '\0', '\0', '\0'};

    srand(time(NULL));

    for( i = 0; i < 4; i++) {
regen:
        gennum = (rand() % 10) + '0';
#if (_DEBUG == 1) 
        printf("random number is %c\n", gennum);
#endif
        if (i != 0) {
            for ( j = 0; j < i; j++ ) {
                if ( gennum == answer[j] ) {
                    goto regen;
                }
            }
            answer[i] = gennum;
        } else {
            answer[i] = gennum;
        }
    }
#if (_DEBUG == 1) 
    printf("answer is %s\n", answer);
#endif

    printf("?A?B Game on\n");
    do {
        counter++;
        printf("Enter 4 digits(%d time):\n", counter );
        scanf("%4s", guess);

        for( i = 0, A = 0, B = 0; i < 4; i++ ) {
            for( j = 0; j < 4; j++ ) {
                if ( answer[i] == guess[j] ) {
                    if (i == j) {
                        A++;
                    } else {
                        B++;
                    }
                }
            }
        }

        printf("%d A %d B\n", A, B);
        if ( 4 == A ) {
            printf("You Win!\n");
            break;
        } else if (counter < 8) {
            printf("Try again\n");
        } else {
            printf("You Loss! Answer is %s.\n", answer);
        }
    } while (counter < 8);
}

