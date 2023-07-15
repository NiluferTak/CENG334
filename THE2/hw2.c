#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include "hw2_output.c"



struct CalculateParams {
    int n1;
    int n2;
    int n3;
    int n4;
    int m1;
    int m2;
    int m3;
    int m4;
    unsigned **A;
    unsigned **B;
    unsigned **C;
    unsigned **D;
    unsigned **AB;
    unsigned **CD;
    unsigned **result;
    sem_t *sem_AB;
    sem_t *sem_CD;
    sem_t *sem_result;
    
};
struct CalculateParams *params;


void *addition(void *args) {
    int *parameters = (int *) args;
    int row_index = parameters[0];
    int is_AB = parameters[1];
    // Calculate row result for matrices A+B or C+D, depending on the value of is_AB
    if (parameters[1] == 0) {
        //for one row only
        for (int j = 0; j < params->m1; j++) {
            params->AB[row_index][j] = params->A[row_index][j] + params->B[row_index][j];
            hw2_write_output(0, row_index+1, j+1, params->AB[row_index][j]);
        
        }
        sem_post(&params->sem_AB[row_index]);
    } else {
        for (int j = 0; j < params->m3; j++) {
            params->CD[row_index][j] = params->C[row_index][j] + params->D[row_index][j];
            
            //printf("%d is posted\n",params->CD[3][3]);
            hw2_write_output(1, row_index+1, j+1, params->CD[row_index][j]);
            sem_post(&params->sem_CD[params->n3*j+row_index]);
        }
        //sem_post(&params->sem_CD[row_index]);
    }
    pthread_exit(NULL);
}

void *multiply(void *args){
    int *parameters = (int *) args;
    int row_index = parameters[0];
    int is_multiplication = parameters[1];//no need for this
    
    //wait until a row in AB is calculated
    sem_wait(&params->sem_AB[row_index]); 
    for (int i = 0; i < params->m3; i++)
    {
        for (int j = 0; j < params->m1; j++)
        {
            int wait_for_CD_cell = j*params->m3 + i;
            sem_wait(&params->sem_CD[wait_for_CD_cell]);

            params->result[row_index][i] += params->AB[row_index][j] * params->CD[j][i];
            sem_post(&params->sem_CD[wait_for_CD_cell]);
            
        }
        hw2_write_output(2, row_index+1, i+1, params->result[row_index][i]);
            
            
    }
    
    
    
    pthread_exit(NULL);


}

void *calculateRoutine(void *args){
    //printf("Thread %d created.\n",*(int *)args);

    //if the thread is not for multiplication operation,then call addition
    //else call multiply
    int *params = (int *) args;
    if (params[1] != 2) {
        addition(args);
    
    } else {
        multiply(args);
    }

}
int main(){

    
    params = malloc(sizeof(struct CalculateParams));
    //get the input
    int n1,m1,n2,m2,n3,m3,n4,m4;
    scanf("%d %d",&n1,&m1);

    unsigned** A = (unsigned**) malloc(n1 * sizeof(unsigned*));
    unsigned** B = (unsigned**) malloc(n1 * sizeof(unsigned*));
    for (int i = 0; i < n1; i++) {
        A[i] = (unsigned*) malloc(m1 * sizeof(unsigned));
    }
    for (int i = 0; i < n1; i++) {
        B[i] = (unsigned*) malloc(m1 * sizeof(unsigned));
    }
    
    for(int i=0;i<n1;i++){
        for(int j=0;j<m1;j++){
            scanf("%d",&A[i][j]);
        }
    }
    scanf("%d %d",&n2,&m2);
    for(int i=0;i<n1;i++){
        for(int j=0;j<m1;j++){
            scanf("%d",&B[i][j]);
        }
    }
    
    // get the input m2 and k
    scanf("%d %d",&n3,&m3);
    unsigned** C = (unsigned**) malloc(n3 * sizeof(unsigned*));
    unsigned **D = (unsigned**) malloc(n3 * sizeof(unsigned*));
    for (int i = 0; i < n3; i++) {
        C[i] = (unsigned*) malloc(m3 * sizeof(unsigned));
    }
    for (int i = 0; i < n3; i++) {
        D[i] = (unsigned*) malloc(m3 * sizeof(unsigned));
    }

    for(int i=0;i<n3;i++){
        for(int j=0;j<m3;j++){
            scanf("%d",&C[i][j]);
        }
    }
    scanf("%d %d",&n4,&m4);
    for(int i=0;i<n4;i++){
        for(int j=0;j<m4;j++){
            scanf("%d",&D[i][j]);
        }
    }

    //initialize AB and CD as 0
    unsigned **AB = (unsigned**) malloc(n1 * sizeof(unsigned*));
    unsigned **CD = (unsigned**) malloc(n3 * sizeof(unsigned*));
    for (int i = 0; i < n1; i++) {
        AB[i] = (unsigned*) malloc(m1 * sizeof(unsigned));
    }
    for (int i = 0; i < n3; i++) {
        CD[i] = (unsigned*) malloc(m3 * sizeof(unsigned));
    }
    for(int i=0;i<n1;i++){
        for(int j=0;j<m1;j++){
            AB[i][j]=0;
        }
    }
    for(int i=0;i<n3;i++){
        for(int j=0;j<m3;j++){
            CD[i][j]=0;
        }
    }
    //initialize result matrix
    unsigned **result = (unsigned**) malloc(n1 * sizeof(unsigned*));
    for (int i = 0; i < n1; i++) {
        result[i] = (unsigned*) malloc(m3 * sizeof(unsigned));
    }
    for(int i=0;i<n1;i++){
        for(int j=0;j<m3;j++){
            result[i][j]=0;
        }
    }

    

    //create semaphores
    sem_t sem_AB[n1];
    //for semaphore sem_CD we need element-wise semaphores

    sem_t sem_CD[n3*m3];
    for(int i=0;i<n1;i++){
        sem_init(&sem_AB[i],0,0);
    }
    
    for(int i=0;i<n3*m3;i++){
        
        sem_init(&sem_CD[i],0,0);
    }
    


    params->n1=n1;
    params->n2=n2;
    params->n3=n3;
    params->n4=n4;
    params->m1=m1;
    params->m2=m2;
    params->m3=m3;
    params->m4=m4;
    params->A=A;
    params->B=B;
    params->C=C;
    params->D=D;
    params->AB=AB;
    params->CD=CD;
    params->sem_AB=sem_AB;
    params->sem_CD=sem_CD;
    params->result=result;



    // print A matrix
    // for(int i=0;i<n1;i++){
    //     for(int j=0;j<m1;j++){
    //         printf("%d ",A[i][j]);
    //     }
    //     printf("\n");
    // }
    
    

    pthread_t calculateThreads[n1+m1+n1];

    hw2_init_output();
    
    //The first n1 threads will be responsible for the first addition operation.
    //The next m1 threads will be responsible for the second addition operation .
    //The last n1 threads will be responsible for the multiplicaton operation.
              
    int ids[n1+m1+n1];
    int thread_args[n1+m1+n1][2];
    for (int i = 0; i < n1+m1+n1; i++)
    {
        if (i < n1) {
            thread_args[i][0] = i;
            thread_args[i][1] = 0; // 1 indicates that this thread is for matrices A+B
        } else if (i >= n1 && i<n1+m1) {
            thread_args[i][0] = i - n1;
            thread_args[i][1] = 1; // 0 indicates that this thread is for matrices C+D
        }
        else if(i>=n1+m1){
            thread_args[i][0] = i - n1 - m1;
            thread_args[i][1] = 2; // 2 indicates that this thread is for matrices A*B
        }

        pthread_create(&calculateThreads[i], NULL, calculateRoutine, (void *) thread_args[i]);
        // ids[i]=i;
            
        // pthread_create(&calculateThreads[i],NULL, calculateRoutine , (void *)&ids[i]);
        
    }
    for (int i = 0; i < n1+m1+n1; i++)
    {
        pthread_join(calculateThreads[i],NULL);
    }

    // print result matrix
    for (int i = 0; i < params->n1; i++)
    {
        for (int j = 0; j < params->m3; j++)
        {
            printf("%d ",params->result[i][j]);
        }
        printf("\n");
        /* code */
    }
    
    return 0;



}


