#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int g_var1 = 0;
pthread_mutex_t mutexA;

void *inc_gv(){

    int i,j;
    for (i=0;i<10;i++){
        pthread_mutex_lock(&mutexA);
        g_var1++;
        for (j=0;j<5000000;j++);
            printf(" %d", g_var1);
            fflush(stdout);
            printf(", Thread ID: %lu\n", (unsigned long)pthread_self());
        pthread_mutex_unlock(&mutexA);
    }

}

int main(){

    pthread_t TA, TB;
    int TAret, TBret;

    pthread_mutex_init(&mutexA, NULL);

    TAret = pthread_create(&TA, NULL, inc_gv, NULL);
    TBret = pthread_create(&TB, NULL, inc_gv, NULL);

    pthread_join(TA, NULL);
    pthread_join(TB, NULL);

    //printf("TA & TB return: %d, %d\n", TAret, TBret);
    printf("\n pthread2 completed \n");
    return 0;

}
