#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/socket.h>
#include "message.h"
#include "logging.h"
#include "limits.h"
#include <poll.h>
// #include "logging.c"
// #include "message.c"
//define max bomb count
#define MAX_BOMB_COUNT 1024

#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

typedef struct bomber_data {
    coordinate position;
    int arg_count;
    char *executable_path;
    int *args;
    pid_t killer_pid;
    
    
} bmb;

typedef struct radius_data {
    pid_t pid;
    unsigned int bomb_radius;
    coordinate bomb_position;
} rad;

int main(){
    setbuf(stdout, NULL);
    //get input
    int map_width, map_height , obstacle_count,bomber_count;
    scanf("%d %d %d %d ",&map_width, &map_height , &obstacle_count,&bomber_count);
    
    obsd obstacles[obstacle_count];
    bmb *bombers= (bmb*)malloc(bomber_count * sizeof(bmb));
    for(int i = 0 ; i< obstacle_count ; i++){
        obsd obstacle;
        scanf("%d %d %d",&obstacle.position.x,&obstacle.position.y,&obstacle.remaining_durability);
        obstacles[i]= obstacle;
    }

    for(int y = 0 ; y<bomber_count ; y++){
        scanf("%d %d %d",&bombers[y].position.x,&bombers[y].position.y,&bombers[y].arg_count);
        bombers[y].args = (int*)malloc((bombers[y].arg_count-1) * sizeof(int));
        //scanf("%s", bombers[y].executable_path);
        // Allocate memory for the executable path
        char path_buffer[256]; // buffer to hold the executable path
        scanf("%s", path_buffer);
        bombers[y].executable_path = malloc(strlen(path_buffer) + 1);
        strcpy(bombers[y].executable_path, path_buffer);
        //printf("%s\n", bombers[y].executable_path);
        for (int z = 0; z < bombers[y].arg_count-1; z++) {
            scanf("%d", &bombers[y].args[z]);
            //printf("%d\n ", bombers[y].args[z]);
            
        }

    }
    
    

    //create pipes
    int bomberpipes[bomber_count][2];
    for (int i = 0; i < bomber_count; i++)
    {
        if (PIPE(bomberpipes[i]) == -1)
        {

            perror("pipe");
            exit(EXIT_FAILURE);
        }
        //printf("pipe successfully created\n");
    }
    
    //create child processes
    pid_t pids[bomber_count];
    for (int i = 0; i < bomber_count; i++)
    {
        pid_t pid = fork();
        
        if (pid == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        //printf("fork successfully created\n");
        if (pid == 0)
        {
            //printf("child process created\n");
            
            //child process
            //close read end of pipe
            close(bomberpipes[i][0]);
            
            //redirect stdout to write end of pipe
            dup2(bomberpipes[i][1], STDOUT_FILENO);
            dup2(bomberpipes[i][1], STDIN_FILENO);
            // printf("dup2 successfully created\n");
            //close write end of pipe
            //close(bomberpipes[i][1]);
            
            //printf("execv successfully created\n");
            char *args[bombers[i].arg_count];
            args[0] = bombers[i].executable_path;
            
            for (int y = 0; y < bombers[i].arg_count-1; y++)
            {
                char *arg = (char*)malloc(10 * sizeof(char));
                sprintf(arg, "%d", bombers[i].args[y]);
                args[y+1] = arg;
                
                
                
            }
           
            args[bombers[i].arg_count] = NULL;
            //printf("%p\n", args[bombers[i].arg_count]);
            
            
            //printf("i am here\n");
            execv(bombers[i].executable_path, args);
            
            
            // exit(EXIT_SUCCESS);
        }
        else
        {
            //parent process
            //close write end of pipe
            //printf("parent process created\n");
            close(bomberpipes[i][1]);
            pids[i] = pid;
            //printf("pid: %d\n", pid);
        }
    }
    // printf("i am out\n");
    int bomb_count = 0;
    //create pollfd array

    //create 2 dimensional array for keeping the maze information for bombs and bombers and obstacles.If maze is empty, it is 0.
    int mazebombers[map_width][map_height];
    int mazebombs[map_width][map_height];
    int mazeobstacles[map_width][map_height];
    //initialize the arrays

    for (int i = 0; i < map_width; i++)
    {
        for (int j = 0; j < map_height; j++)
        {
            mazebombers[i][j] = 0;
            mazebombs[i][j] = 0;
            mazeobstacles[i][j] = 0;
        }
    }

    //initialize the arrays with obstacles
    for (int i = 0; i < obstacle_count; i++)
    {
        mazeobstacles[obstacles[i].position.x][obstacles[i].position.y] = obstacles[i].remaining_durability;
    }
    //create pipes for bombs
    int bombpipes[MAX_BOMB_COUNT][2];
    int bomb_iterator = 0;
    pid_t lasttwobombers[2];
    pid_t winning_bomber;
    pid_t lastbombers[bomber_count];
    pid_t bombpids[MAX_BOMB_COUNT];
    pid_t deadbombers[bomber_count];
    bmb winner;
    int deadpeople =0;
    int bomber_count_initial = bomber_count;
    // printf("i am here\n");
    rad bomb_radius_array[MAX_BOMB_COUNT];
    //initialize all bomber's killer_pid to 0
    for (int i = 0; i < bomber_count; i++)
    {
        bombers[i].killer_pid = 0;
    }
    //initialize winner position to -1
    winner.position.x = -1;
    winner.position.y = -1;
    //initialize winner's arg_count to 0
    winner.arg_count = 0;
    //initialize winner's args to NULL
    winner.args = NULL;
    //initialize winner's executable_path to NULL
    winner.executable_path = NULL;
    //initialize winner's killer_pid to 0
    winner.killer_pid = -1;


    while (bomber_count != 0)
    {
        
        
        
        //printf("i am in\n");
        //create pipe for bombs
        //int bombpipe[bomb_count][2];
        if(bomb_count != 0){//if there are bombs I have to read its message first
            //int bombcount = bomb_count;
            
            for (int i = 0; i < bomb_count; i++)
            {
                struct pollfd pollfds;
                pollfds.fd = bombpipes[i][0];
                pollfds.events = POLLIN;
                //poll
                int poll_result = poll(&pollfds,1, 0);
                if (poll_result == -1)
                {
                    perror("poll");
                    exit(EXIT_FAILURE);
                }
                //allocate memory for im message
                im message ;
                if (pollfds.revents & POLLIN)
                {
                    // printf("read_data ya geldi");
                    
                    int read_result = read_data(bombpipes[i][0],&message);
                    if (read_result == -1)
                    {
                        perror("read");
                        exit(EXIT_FAILURE);
                    }
                    

                }
                if (message.type == BOMB_EXPLODE)
                {
                    bomb_count--;
                    //allocate memory for om message
                    om outmessage;
                    omp printoutput; 

                    //allocate space for imp struct
                    imp printinput; 
                    printinput.pid = bombpids[i];
                    printinput.m = &message;
                    outmessage.type = BOMB_EXPLODE;
                    
                    // send_message(bombpipes[i][0], &outmessage);
                    print_output(&printinput,NULL,NULL,NULL);

                    
                    // printf("is it hereeee");
                    //get the radius of the bomb from its pid
                    unsigned int bomb_radius = bomb_radius_array[i].bomb_radius;
                    unsigned int xcoord = bomb_radius_array[i].bomb_position.x;
                    unsigned int ycoord = bomb_radius_array[i].bomb_position.y;
                    //delete the bomb from the maze
                    mazebombs[xcoord][ycoord] = 0;

                    //check if there is a bomber in the radius of bomb. If there is, bomber dies.
                    for (int b = 0; b < bomber_count; b++)
                    {
                        //bomb position i hep   0 0 oluyor nedennnnn
                        // printf("bomber count %d\n", bomber_count);
                        // printf("radius %d\n",bomb_radius);
                        // // printf("DIEEEEEE");
                        // printf("bomber position: %d %d bomb position: %d %d\n", bombers[b].position.x, bombers[b].position.y, xcoord, ycoord);

                        //if bomber is in the radius of bomb, bomber dies
                        if ((bombers[b].position.x == xcoord || bombers[b].position.y == ycoord) && (abs(bombers[b].position.x - xcoord) + abs(bombers[b].position.y - ycoord)))
                        {
                            // printf("wtffffffffffffffffffffffffffffffffffffff");
                            // fflush(stdout);
                            //check if there is an obstacle in the radius of the bomb
                            for (int o = 0; o < obstacle_count; o++)
                            {
                                // printf("looooooolllll");
                                if (obstacles[o].position.x >= xcoord - bomb_radius && obstacles[o].position.x <= xcoord + bomb_radius && obstacles[o].position.y >= ycoord - bomb_radius && obstacles[o].position.y <= ycoord + bomb_radius)
                                {
                                    // printf("ssssssssss");
                                    //check if  the obstacle between the bomber and the bomb

                                    if (bombers[b].position.x >= obstacles[o].position.x - bomb_radius && bombers[b].position.x <= obstacles[o].position.x + bomb_radius && bombers[b].position.y >= obstacles[o].position.y - bomb_radius && bombers[b].position.y <= obstacles[o].position.y + bomb_radius)
                                    {
                                        //if bomber is behind the obstacle, bomber does not die
  
                                        //decrease the obstacle durability by 1 if it is not -1
                                        if (obstacles[o].remaining_durability != -1){
                                            obstacles[o].remaining_durability -= 1;
                                            // printf("sjgllllllllllllllddkdk");
                                            print_output(NULL,NULL,&obstacles[o],NULL);

                                        }

                                            
                                        //if obstacle durability is 0, delete the obstacle
                                        if (obstacles[o].remaining_durability == 0)
                                        {
                                            print_output(NULL,NULL,&obstacles[o],NULL);
                                            //delete the obstacle from the maze
                                            mazeobstacles[obstacles[o].position.x][obstacles[o].position.y] = 0;
                                            //delete the obstacle from the array
                                            for (int d = o; d < obstacle_count - 1; d++)
                                            {
                                                obstacles[d] = obstacles[d + 1];

                                            }
                                            obstacle_count -= 1;
                                        }
                                            
                                        
                                        
                                        //if bomber is not behind the obstacle, bomber does not die
                                        
                                    }
                                    
                                }
                                
                            }

                            // printf("segggggggggffffff\n");
                            bombers[b].killer_pid = bombpids[i];
                            // printf("dieddd : %d %d\n",bombers[b].position.x,bombers[b].position.y);
                            deadpeople++;
                            // printf("%d\n",deadpeople);
                            
                            // printf("%d\n",bombers[b].killer_pid);
                            // printf("bomber died\n");
                            

                        }
                    }

                    //BURALARDA SORUN VARR DUZELTTT
                    // only one remains
                    // 1 kisi kaldi,patlamada olmedi,sondan bir onceki request atmadi ama onemi yok.
                    if (deadpeople == bomber_count_initial - 1)
                    {

                        winner.position.x = bombers[0].position.x;
                        winner.position.y = bombers[0].position.y;
                        
                        // printf("%d\n",winner.position.x);
                        // printf("%d\n",winner.position.y);
                        // /* code */
                    }
                    // kimse kalmadi ama son iki kisi request atamadan farkli patlamalarda olduler.digerleri coktan oldu request atip gittiler
                    //en son olen kazandi.
                    if (deadpeople == bomber_count_initial && bombers[0].killer_pid != bombers[1].killer_pid)
                    {
                        // printf("bomber count is what : %d\n",bomber_count);
                        winner.position.x = bombers[1].position.x;
                        winner.position.y = bombers[1].position.y;
                        
                        // printf("%d\n",winner.position.x);
                        // printf("%d\n",winner.position.y);
                        // /* code */
                    }
                    
                    
                    // geri kalan herkes ayni patlamada oldu ve kimse request atmadı
                    if(deadpeople == bomber_count_initial && bombers[0].killer_pid == bombers[1].killer_pid){
                        // if all dead bomber's killer pid is same, winner is the bomber that is farthest from the bomb position
                        
                        //get the one bomber that furthest away from the bomb position.
                        int maxdistance = 0;
                        int maxdistanceindex = 0;
                        for (int b = 0; b < bomber_count; b++)
                        {
                            int distance = abs(bombers[b].position.x - xcoord) + abs(bombers[b].position.y - ycoord);
                            if (distance > maxdistance)
                            {
                                maxdistance = distance;
                                maxdistanceindex = b;
                            }
                        }
                        winner.position.x = bombers[maxdistanceindex].position.x;
                        winner.position.y = bombers[maxdistanceindex].position.y;
                        
                        // printf("%d\n",winner.position.x);
                        // printf("%d\n",winner.position.y);
                        

                    }
                    
                    



                    

                }
            }

        }
        for (int i = 0; i < bomber_count; i++)
            {
                struct pollfd pollfds;
                pollfds.fd = bomberpipes[i][0];
                pollfds.events = POLLIN;
                //poll
                int poll_result = poll(&pollfds,1, 0);
                if (poll_result == -1)
                {
                    perror("poll");
                    exit(EXIT_FAILURE);
                }
                //allocate memory for im message
                im message ;
                if (pollfds.revents & POLLIN)
                {
                    // printf("read_data ya geldi");
                    
                    int read_result = read_data(bomberpipes[i][0],&message);
                    
                if (read_result == -1)
                {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                //if message type is BOMBER_START
                if (message.type == BOMBER_START)
                {
                    
                    //allocate memory for om message
                    om outmessage;
                    omp printoutput; 

                    //allocate space for imp struct
                    imp printinput; 
                    printinput.pid = pids[i];
                    printinput.m = &message;
                    print_output(&printinput,NULL,NULL,NULL);
                    // if the bomber is dead, send BOMBER_DEAD message
                    if (bombers[i].killer_pid != 0 && winner.position.x != bombers[i].position.x && winner.position.y != bombers[i].position.y)
                    {
                        // printf("finallllyyy\n");
                        outmessage.type = BOMBER_DIE;
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        for (int d = i; d < bomber_count - 1; d++)
                        {
                            bombers[d] = bombers[d + 1];
                        }
                        bomber_count -= 1;



                        continue;
                    }
                    // printf("%d %d\n",bombers[i].position.x,bombers[i].position.y);
                    // printf("%d %d\n",winner.position.x,winner.position.y);
                    //if the bomber is the winner , send BOMBER_WIN message
                    if (bombers[i].position.x == winner.position.x && bombers[i].position.y == winner.position.y && bomber_count == 1)
                    {
                        // outmessage.type = BOMBER_WIN;
                        // printoutput.pid = pids[i];
                        // printoutput.m = &outmessage;
                        omp winoutput;
                        // printf("kazandiiiii\n");
                        outmessage.type = BOMBER_WIN;
                        winoutput.pid = pids[i];
                        winoutput.m = &outmessage;

                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        // for (int d = i; d < bomber_count - 1; d++)
                        // {
                        //     bombers[d] = bombers[d + 1];
                        // }
                        bomber_count -= 1;

                        
                    }
                    if (bomber_count == 0)
                    {
                        continue;
                        /* code */
                    }
                    
                    

                    
                    
                    

                    //set message type
                    outmessage.type = BOMBER_LOCATION;
                    outmessage.data.new_position.x = bombers[i].position.x;
                    outmessage.data.new_position.y = bombers[i].position.y;
                    printoutput.pid = pids[i];
                    printoutput.m = &outmessage;

                    //modify the mazebomber array with bomber's position.
                    mazebombers[bombers[i].position.x][bombers[i].position.y] = pids[i];
                    //printf("MAZEBOMBERSSSSSS %d \n", mazebombers[bombers[i].position.x][bombers[i].position.y]);
                    //printf("Iam here\n");
                    
                    //send outmessage to bomber process
                    int write_result = send_message(bomberpipes[i][0], &outmessage);
                    if (write_result == -1)
                    {
                        perror("write");
                        break;
                        /* code */
                    }
                    print_output(NULL,&printoutput,NULL,NULL);
                    
                    
                    
                    //counter--;
                    
                }
                //if message type is BOMBER_MOVE
                else if (message.type == BOMBER_MOVE)
                {
                    //printf("I am in BOMBER_MOVE\n");
                    //allocate memory for om message
                    om outmessage;
                    omp printoutput; 

                    //allocate space for imp struct
                    imp printinput; 
                    printinput.pid = pids[i];
                    printinput.m = &message;
                    print_output(&printinput,NULL,NULL,NULL);
                    if (bombers[i].killer_pid != 0 && winner.position.x != bombers[i].position.x && winner.position.y != bombers[i].position.y)
                    {
                        // printf("finallllyyy\n");
                        outmessage.type = BOMBER_DIE;
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        for (int d = i; d < bomber_count - 1; d++)
                        {
                            bombers[d] = bombers[d + 1];
                        }
                        bomber_count -= 1;



                        continue;
                    }
                    // printf("%d %d\n",bombers[i].position.x,bombers[i].position.y);
                    // printf("%d %d\n",winner.position.x,winner.position.y);
                    //if the bomber is the winner , send BOMBER_WIN message
                    if (bombers[i].position.x == winner.position.x && bombers[i].position.y == winner.position.y && bomber_count == 1)
                    {
                        // outmessage.type = BOMBER_WIN;
                        // printoutput.pid = pids[i];
                        // printoutput.m = &outmessage;
                        omp winoutput;
                        // printf("kazandiiiii\n");
                        outmessage.type = BOMBER_WIN;
                        winoutput.pid = pids[i];
                        winoutput.m = &outmessage;

                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        // for (int d = i; d < bomber_count - 1; d++)
                        // {
                        //     bombers[d] = bombers[d + 1];
                        // }
                        bomber_count -= 1;

                        
                    }
                    if (bomber_count == 0)
                    {
                        continue;
                        /* code */
                    }
                       

                    
                    
                    //check if  First, the position should be only one step away from the bomber position. Second, it should be a horizontal or vertical move, a diagonal move is not accepted. Third,there should be no obstacles or bombers in the target position.Fourth he target should not be out of bounds.

                    if (((abs(message.data.target_position.x - bombers[i].position.x) == 1 && message.data.target_position.y == bombers[i].position.y) || (abs(message.data.target_position.y - bombers[i].position.y) == 1 && message.data.target_position.x == bombers[i].position.x)) 
                    && (mazebombers[message.data.target_position.x][message.data.target_position.y] == 0) &&(mazeobstacles[message.data.target_position.x][message.data.target_position.y] == 0)
                    &&(message.data.target_position.x < map_height && message.data.target_position.y < map_width))
                    {

                        
                        //set message type
                        outmessage.type = BOMBER_LOCATION;
                        outmessage.data.new_position.x = message.data.target_position.x;
                        
                        outmessage.data.new_position.y = message.data.target_position.y;

                        bombers[i].position.x = message.data.target_position.x;
                        bombers[i].position.y = message.data.target_position.y;

                        // printf("x %d\n",outmessage.data.new_position.x);
                        // printf("y %d\n",outmessage.data.new_position.y);
                        
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;

                        //modify the mazebomber array with bomber's position.
                        mazebombers[message.data.target_position.x][message.data.target_position.y] = pids[i];
                        mazebombers[bombers[i].position.x][bombers[i].position.y] = 0;

                        //send outmessage to bomber process
                        int write_result = send_message(bomberpipes[i][0], &outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);
                            
                            
                        
                    }
                    else{
                        //set message type
                        outmessage.type = BOMBER_LOCATION;
                        outmessage.data.new_position.x = bombers[i].position.x;
                        outmessage.data.new_position.y = bombers[i].position.y;
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        
                        // printf("WTFFFFFFFFFFFFFF\n");
                        // printf("x %d\n",outmessage.data.new_position.x);
                        // printf("y %d\n",outmessage.data.new_position.y);


                        //modify the mazebomber array with bomber's position.no need . nothing has changed
                        //mazebombers[bombers[i].position.x][bombers[i].position.y] = pids[i];

                        //send outmessage to bomber process
                        int write_result = send_message(bomberpipes[i][0], &outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);
                    }
                   
                }

                //if message type is BOMBER_SEE
                else if (message.type == BOMBER_SEE)
                {
                    //allocate memory for om message
                    om outmessage;
                    omp printoutput; 

                    //allocate space for imp struct
                    imp printinput; 
                    printinput.pid = pids[i];
                    printinput.m = &message;
                    print_output(&printinput,NULL,NULL,NULL);
                    
                    //if the bomber's pid is equal to the winning_bomber's pid, send BOMBER_WIN message to bomber process
                    
                    //make a object_data struct array of size 25
                    od objects[25]; 
                    // if the bomber is dead, send BOMBER_DEAD message
                    if (bombers[i].killer_pid != 0 && winner.position.x != bombers[i].position.x && winner.position.y != bombers[i].position.y)
                    {
                        // printf("finallllyyy\n");
                        outmessage.type = BOMBER_DIE;
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        for (int d = i; d < bomber_count - 1; d++)
                        {
                            bombers[d] = bombers[d + 1];
                        }
                        bomber_count -= 1;



                        continue;
                    }
                    // printf("mywinnerbomb :%d %d\n",bombers[i].position.x,bombers[i].position.y);
                    // printf("%d %d\n",winner.position.x,winner.position.y);
                    //if the bomber is the winner , send BOMBER_WIN message
                    if (bombers[i].position.x == winner.position.x && bombers[i].position.y == winner.position.y && bomber_count == 1)
                    {
                        // printf("kazandiiiiiii\n");
                        omp winoutput;
                        outmessage.type = BOMBER_WIN;
                        winoutput.pid = pids[i];
                        winoutput.m = &outmessage;

                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&winoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        // for (int d = i; d < bomber_count - 1; d++)
                        // {
                        //     bombers[d] = bombers[d + 1];
                        // }
                        bomber_count -= 1;

                        
                    }
                    if (bomber_count == 0)
                    {
                        continue;
                        /* code */
                    }
                    //check the bomber's position up to 3 steps in all four directions(not diagonally) and count the objects if there are any.If there is a block in the way , the bomber cannot see  any objects beyond that block.
                    int counter = 0;
                    unsigned int x = bombers[i].position.x;
                    unsigned int y = bombers[i].position.y;
                    // printf("%d\n",x);
                    // printf("%d\n",y);
                    // printf("%d\n",bombers[i].position.x);
                    // printf("%d\n",bombers[i].position.y);
                    int iterator = 0;
                    //up
                    for (int j = 1; j < 4; j++)
                    {
                        if (x-j >= 0)
                        {
                            if (mazeobstacles[x-j][y] != 0)
                            {
                                counter++;
                                objects[iterator].type = OBSTACLE;
                                objects[iterator].position.x = x-j;
                                objects[iterator].position.y = y;
                                iterator++;
                                break;
                            }
                            if(mazebombers[x-j][y] != 0)
                            {
                                objects[iterator].type = BOMBER;
                                objects[iterator].position.x = x-j;
                                objects[iterator].position.y = y;
                                iterator++;
                                counter++;
                            }
                            if(mazebombs[x-j][y] == 1)
                            {
                                objects[iterator].type = BOMB;
                                objects[iterator].position.x = x-j;
                                objects[iterator].position.y = y;
                                iterator++;
                                counter++;
                            }
                            
                        }
                        
                    }
                    //down
                    for (int j = 1; j < 4; j++)
                    {
                        if (x+j < map_height)
                        {
                            if (mazeobstacles[x+j][y] != 0)
                            {
                                objects[iterator].type = OBSTACLE;
                                objects[iterator].position.x = x+j;
                                objects[iterator].position.y = y;
                                iterator++;
                                counter++;
                                break;
                            }
                            if (mazebombers[x+j][y] != 0)
                            {
                                objects[iterator].type = BOMBER;
                                objects[iterator].position.x = x+j;
                                objects[iterator].position.y = y;
                                // printf("%d\n",x+j);
                                // printf("%d\n",y);
                                // printf("%d\n",objects[iterator].position.x);
                                // printf("%d\n",objects[iterator].position.y);

                                iterator++;
                                counter++;
                                // printf("%d\n",counter);
                                // printf("%d\n",iterator);
                            }
                            if (mazebombs[x+j][y] == 1)
                            {
                                objects[iterator].type = BOMB;
                                objects[iterator].position.x = x+j;
                                objects[iterator].position.y = y;
                                iterator++;
                                counter++;
                            }
                            
                        }
                        
                    }
                    //left
                    for (int j = 1; j < 4; j++)
                    {
                        if (y-j >= 0)
                        {
                            if (mazeobstacles[x][y-j] != 0)
                            {
                                objects[iterator].type = OBSTACLE;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y-j;
                                iterator++;
                                counter++;
                                break;
                            }
                            if (mazebombers[x][y-j] != 0)
                            {
                                objects[iterator].type = BOMBER;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y-j;
                                iterator++;
                                counter++;
                            }
                            if (mazebombs[x][y-j] == 1)
                            {
                                objects[iterator].type = BOMB;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y-j;
                                iterator++;
                                counter++;
                            }
                            
                        }
                        
                    }
                    //right
                    for (int j = 1; j < 4; j++)
                    {
                        if (y+j < map_width)
                        {
                            if (mazeobstacles[x][y+j] != 0)
                            {
                                objects[iterator].type = OBSTACLE;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y+j;
                                iterator++;
                                counter++;
                                break;
                            }
                            if (mazebombers[x][y+j] != 0)
                            {
                                objects[iterator].type = BOMBER;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y+j;
                                iterator++;
                                counter++;
                            }
                            if (mazebombs[x][y+j] == 1)
                            {
                                objects[iterator].type = BOMB;
                                objects[iterator].position.x = x;
                                objects[iterator].position.y = y+j;
                                iterator++;
                                counter++;
                            }
                            
                        }
                        
                    }
                    //check if there is a bomb in the same position as the bomber
                    if (mazebombs[x][y] == 1)
                    {
                        objects[iterator].type = BOMB;
                        objects[iterator].position.x = x;
                        objects[iterator].position.y = y;
                        iterator++;
                        counter++;
                    }
                    // printf("%d\n",objects[iterator-1].position.x);
                    // printf("%d\n",objects[iterator-1].position.y);
                    // printf("%d\n",iterator);

                    //create od array of size iterator
                    od objectsarray[iterator];
                    for (int j = 0; j < iterator; j++)
                    {
                        objectsarray[j] = objects[j];
                    }
                    // for (int j = 0; j < iterator; j++)
                    // {
                    //     printf("%d\n",objectsarray[j].type);
                    //     printf("%d\n",objectsarray[j].position.x);
                    //     printf("%d\n",objectsarray[j].position.y);
                    // }




                    

                    
                    
                    //set message type
                    outmessage.type = BOMBER_VISION;
                    //set message data
                    outmessage.data.object_count = counter;
                    printoutput.pid = pids[i];
                    printoutput.m = &outmessage;

                        //send outmessage to bomber process
                        
                    int write_result = send_message(bomberpipes[i][0], &outmessage);
                    if(write_result == -1){
                        printf("Error in sending object data(counter) to bomber process");
                    }
                    // if(counter == 0){
                    //     print_output(NULL,&printoutput,NULL,NULL);
  
                    // }
                    int write_result2 = send_object_data(bomberpipes[i][0],counter,&objectsarray);
                        //printf("I send object data to bomber process\n");
                        //printf("%d\n",write_result);
                    if(write_result2 == -1){
                        printf("Error in sending object data to bomber process");
                    }
                    print_output(NULL,&printoutput,NULL,&objectsarray);
                    //print_output(NULL,&printoutput,NULL,NULL);
                    // else if(counter != 0){
                    //     //printf("heeey\n");
                    //     //send outmessage to bomber process
                    //     printf("%d\n",counter);
                    //     // printf("%d\n",objects[iterator].type);
                    //     // printf("%d\n",objects[iterator].position.x);
                    //     // printf("%d\n",objects[iterator].position.y);
                        
                        
                        
                    // }
                    
                    
                    
                    
                }
                //if bomber message is BOMBER_PLANT
                else if(message.type == BOMBER_PLANT){
                    //printf("I get BOMBER_PLANT message");
                    //allocate memory for om message
                    om outmessage;
                    om bomboutmessage;
                    omp printoutput; 

                    //allocate space for imp struct
                    imp printinput; 
                    printinput.pid = pids[i];
                    printinput.m = &message;
                    print_output(&printinput,NULL,NULL,NULL);
                    if (bombers[i].killer_pid != 0 && winner.position.x != bombers[i].position.x && winner.position.y != bombers[i].position.y)
                    {
                        // printf("finallllyyy\n");
                        outmessage.type = BOMBER_DIE;
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        for (int d = i; d < bomber_count - 1; d++)
                        {
                            bombers[d] = bombers[d + 1];
                        }
                        bomber_count -= 1;



                        continue;
                    }
                    // printf("%d %d\n",bombers[i].position.x,bombers[i].position.y);
                    // printf("%d %d\n",winner.position.x,winner.position.y);
                    //if the bomber is the winner , send BOMBER_WIN message
                    if (bombers[i].position.x == winner.position.x && bombers[i].position.y == winner.position.y && bomber_count == 1)
                    {
                        omp winoutput;
                        // printf("kazandiiiii\n");
                        outmessage.type = BOMBER_WIN;
                        winoutput.pid = pids[i];
                        winoutput.m = &outmessage;

                        send_message(bomberpipes[i][0],&outmessage);
                        print_output(NULL,&printoutput,NULL,NULL);

                        //reap the dead bomber process with waitpid
                        // int status;
                        // int waitpid_result = waitpid(pids[i],NULL,0);
                        // if (waitpid_result == -1)
                        // {
                        //     perror("waitpid");
                        //     exit(EXIT_FAILURE);
                        // }
                        //delete the bomber from the array
                        // for (int d = i; d < bomber_count - 1; d++)
                        // {
                        //     bombers[d] = bombers[d + 1];
                        // }
                        bomber_count -= 1;

                        
                    }
                    if (bomber_count == 0)
                    {
                        continue;
                        /* code */
                    }
                    

                    //check if there is a bomb in the same position as the bomber
                    if (mazebombs[bombers[i].position.x][bombers[i].position.y] == 1)
                    {
                        //set message type
                        outmessage.type = BOMBER_PLANT_RESULT;
                        //set message data
                        outmessage.data.planted = 0;
                        
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        int write_result = send_message(bomberpipes[i][0], &outmessage);
                        if(write_result == -1){
                            printf("Error in sending bomb message result to bomber process");
                        }
                        print_output(NULL,&printoutput,NULL,NULL);
                    }
                    else
                    {
                        //set message type
                        outmessage.type = BOMBER_PLANT_RESULT;
                        //set message data
                        outmessage.data.planted = 1;
                        
                        printoutput.pid = pids[i];
                        printoutput.m = &outmessage;
                        bomboutmessage.data.new_position.x = bombers[i].position.x;
                        bomboutmessage.data.new_position.y = bombers[i].position.y;
                        
                        //create pipe for the bomb process
                        int write_result = send_message(bomberpipes[i][0], &outmessage);
                        long bomb_interval = message.data.bomb_info.interval;
                        char arg[100];
                        // printf("HERE\n");
                        snprintf(arg, sizeof(arg), "%ld", bomb_interval);
                        // printf("hjshdkshosh\n");
                        //printf("%s\n",arg);
                        if(write_result == -1){
                            printf("Error in sending bomb message result to bomber process");
                        }
                        bomb_count++;
                        
                        if (PIPE(bombpipes[bomb_iterator]) == -1)
                        {

                            perror("pipe");
                            exit(EXIT_FAILURE);
                        }
                        //create bomb process
                        pid_t bombpid = fork();
                        //check if fork is successful
                        if(bombpid == -1){
                            printf("Error in creating bomb process");
                        }
                        //if bomb process is created
                        if(bombpid == 0){
                            // printf("%d\n",bomb_iterator);
                            // printf("HERE IS BOMB CHILD\n");
                            close(bombpipes[bomb_iterator][0]);
            
                            //redirect stdout to write end of pipe
                            dup2(bombpipes[bomb_iterator][1], STDOUT_FILENO);
                            dup2(bombpipes[bomb_iterator][1], STDIN_FILENO);
                            // close(bombpipes[bomb_iterator][1]);
                            // printf("HEReeeee\n");
                            
                            
                            execl("./bomb","./bomb", arg, NULL);
                            
                            
                        }
                        else{
                            close(bombpipes[bomb_iterator][1]);
                            bombpids[bomb_iterator] = bombpid;
                            bomb_radius_array[bomb_iterator].pid = bombpid;
                            bomb_radius_array[bomb_iterator].bomb_radius = message.data.bomb_info.radius;
                            bomb_radius_array[bomb_iterator].bomb_position.x = bombers[i].position.x;
                            bomb_radius_array[bomb_iterator].bomb_position.y = bombers[i].position.y;
                            
                            //printf("bomb process created with pid");
                        }
                        // send_message(bombpipes[bomb_iterator][0],&bomboutmessage);
                        bomb_iterator++;
                        
                        print_output(NULL,&printoutput,NULL,NULL);
                        //plant bomb
                        mazebombs[bombers[i].position.x][bombers[i].position.y] = 1;
                        // printf("%d\n",mazebombers[bombers[i].position.x][bombers[i].position.y]);
                        // printf("%d\n",bombers[i].position.x);
                        // printf("%d\n",bombers[i].position.y);
                        // printf("%d\n",mazebombs[bombers[i].position.x][bombers[i].position.y]);
                        // printf("%d\n",mazebombs[bombers[i].position.x][bombers[i].position.y]);

                    }
                    
                
                }


        
            
            }

        
            
            
            
            
            /* code */
        }
        
        
        usleep(1000); // Sleep for 1 millisecond
           
    
    }
    //Wait for remaining bombs to explode and reap them
    for (int i = 0; i < bomb_iterator; i++)
    {
        int status;
        int waitpid_result = waitpid(bombpids[i],NULL,0);
        if (waitpid_result == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
    }
    // reap all bomber processes
    for (int i = 0; i < bomber_count_initial; i++)
    {
        int status;
        int waitpid_result = waitpid(pids[i],NULL,0);
        if (waitpid_result == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
    }

    
    
    
    
    
    
    

}