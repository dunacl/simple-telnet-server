#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "server.h"


/**
 Authentication
*/

int authentication(int newsockfd, char buffer[256]) {
  int username, password = 0;
  char userbuffer[256];
  char passbuffer[256];

  write(newsockfd, PROMPT_USERNAME, sizeof(PROMPT_USERNAME));
  username = read( newsockfd, buffer, 256);
  strcpy(userbuffer, buffer);
  write(newsockfd, PROMPT_PASSWORD, sizeof(PROMPT_PASSWORD));
  password = read( newsockfd, buffer, 256);
  strcpy(passbuffer, buffer);

  if ((strncmp(USERNAME, userbuffer, username - 2) == 0) && (strncmp(PASSWORD, passbuffer, password - 2) == 0)){
    return 1;
  }
  return 0;
}

/**
 * Method to send help docs
*/
void sendHelpScreen(int newsockfd) {
  char str1[16] = "Commands list\n\n";
  char str2[26] = "help        Display help\n";
  char str3[25] = "led on      Turn led on\n";
  char str4[26] = "led off     Turn led off\n";
  char str5[26] = "clear       Clear screen\n";
  char str6[28] = "exit        Exit session\n\r\n";
  write(newsockfd, str1, sizeof(str1));
  write(newsockfd, str2, sizeof(str2));
  write(newsockfd, str3, sizeof(str3));
  write(newsockfd, str4, sizeof(str4));
  write(newsockfd, str5, sizeof(str5));
  write(newsockfd, str6, sizeof(str6));
}

/**
 * Method used to print the welcome screen of our shell
 */
void welcomeScreen(int new_socket){
  write(new_socket, WELCOME_MESSAGE, sizeof(WELCOME_MESSAGE));
  sendHelpScreen(new_socket);
  send(new_socket, DEFAULTPROMPT, sizeof(DEFAULTPROMPT), 0);
}

int main(int argc , char *argv[])
{
  int opt = TRUE;
  int authenticated = FALSE;
  int login_errors = 1;
  int master_socket , addrlen , new_socket , client_socket[30] , max_clients = 30 , activity, i , valread , sd;
  int max_sd;
  struct sockaddr_in address;

  char buffer[256];

  //set of socket descriptors
  fd_set readfds;

  //initialise all client_socket[] to 0 so not checked
  for (i = 0; i < max_clients; i++)
  {
    client_socket[i] = 0;
  }

  //create a master socket
  if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  //set master socket to allow multiple connections ,
  //this is just a good habit, it will work without this
  if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
  {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  //type of socket created
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(LISTEN_IP);
  address.sin_port = htons( PORT );

  //bind the socket
  if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  printf("Listener at %s on port %d \n", LISTEN_IP, PORT);

  //try to specify maximum of 3 pending connections for the master socket
  if (listen(master_socket, 3) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  //accept the incoming connection
  addrlen = sizeof(address);
  //puts("Waiting for connections ...");

  while(TRUE)
  {
    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    //add child sockets to set
    for ( i = 0 ; i < max_clients ; i++)
    {
      //socket descriptor
      sd = client_socket[i];

      //if valid socket descriptor then add to read list
      if(sd > 0)
        FD_SET( sd , &readfds);

      //highest file descriptor number, need it for the select function
      if(sd > max_sd)
        max_sd = sd;
    }

    //wait for an activity on one of the sockets , timeout is NULL ,
    //so wait indefinitely
    activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

    if ((activity < 0) && (errno!=EINTR))
    {
      printf("select activity error");
    }

    //If something happened on the master socket ,
    //then its an incoming connection
    if (FD_ISSET(master_socket, &readfds))
    {
      if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
      {
        perror("accept error");
        exit(EXIT_FAILURE);
      }

      //inform user of socket number - used in send and receive commands
      printf("New connection , socket fd is %d , ip is : %s , port : %d\n" , new_socket ,inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
      //puts("Welcome message sent successfully");

      //add new socket to array of sockets
      for (i = 0; i < max_clients; i++)
      {
        //if position is empty
        if( client_socket[i] == 0 )
        {
          client_socket[i] = new_socket;
          printf("Adding to list of sockets as %d\n" , i);
          break;
        }
      }
    }

    // Do Auth
    while(authenticated == 0){
      if(login_errors > 3) {
        close( new_socket );
        client_socket[i] = 0;
        printf("Fatal Auth, ip %s , port %d \n" ,
          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        break;
      } else {
        authenticated = authentication(new_socket, buffer);
        login_errors++;
      }
    }
    login_errors = 1;
    welcomeScreen(new_socket);

    //else its some IO operation on some other socket
    for (i = 0; i < max_clients; i++)
    {
      sd = client_socket[i];

      if (FD_ISSET( sd , &readfds))
      {

        //Check if it was for closing , and also read the
        //incoming message
        if ((valread = read( sd , buffer, 256)) == 0)
        {
          //Somebody disconnected , get his details and print
          getpeername(sd , (struct sockaddr*)&address ,(socklen_t*)&addrlen);
          printf("Host disconnected , ip %s , port %d \n" ,
          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

          //Close the socket and mark as 0 in list for reuse
          close( sd );
          client_socket[i] = 0;
        }
          //Echo back the message that came in
        else
        {
          //set the string terminating NULL byte on the end
          //of the data read
          buffer[valread] = '\0';

          // write help
          if (strncmp("help", buffer, 4) == 0) {
            sendHelpScreen(sd);
          }
          // led on
          else if (strncmp("led on", buffer, 6) == 0) {
            send(sd, "Turning LED ON\n", 15, 0);
            send(sd, buffer, 6,0);
          }
          // led off
          else if (strncmp("led off", buffer, 7) == 0) {
            send(sd, "Turning LED OFF\n", 16, 0);
          }
          // clear
          else if (strncmp("clear", buffer, 4) == 0){
            for(i = 0; i < 100; i = i +1) {
              send(sd,"\n",1,0);
            }
          }
          // exit
          else if (strncmp("exit", buffer, 4) == 0) {
            send(sd, "Good bye\n", 9, 0);
            printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
            close( sd );
            client_socket[i] = 0;
          }
          else {
            send(sd, COMANDO_INVALIDO, sizeof(COMANDO_INVALIDO), 0);
          }
          //send(sd , buffer , strlen(buffer) , 0 );
          send(sd, DEFAULTPROMPT, sizeof(DEFAULTPROMPT), 0);
        }
      }
    }
  }

  return 0;
}