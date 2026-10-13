#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int g_var1 = 0;

void *inc_gv(){

    int i,j;
    for (i=0;i<10;i++){
        g_var1++;
        for (j=0;j<5000000;j++);
            printf(" %d", g_var1);
            printf(", Thread ID: %lu\n", (unsigned long)pthread_self());
            fflush(stdout);
    }
}

int main(){

    pthread_t TA, TB;
    int TAret, TBret;

    TAret = pthread_create(&TA, NULL, inc_gv, NULL);
    TBret = pthread_create(&TB, NULL, inc_gv, NULL);

    pthread_join(TA, NULL);
    pthread_join(TB, NULL);

    //printf("TA & TB return: %d, %d\n", TAret, TBret);
    printf("\n pthread2 completed \n");
    return 0;

}
