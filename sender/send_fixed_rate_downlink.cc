#include <string>
#include <vector>
#include <poll.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "acker.hh"
#include "fixed_rate_sending.hh"

using namespace std;

void *send_data(void *saturatr_) {
   FixedRateSending * saturatr = (FixedRateSending*) saturatr_;
   while (1) {
    saturatr->send_data();
    usleep(saturatr->wait_time() / (float) 1e3);
   }
}

int main( int argc, char *argv[] )
{
  if ( argc != 3 && argc != 4 ) {
    fprintf( stderr, "Usage: %s [SERVER : PACKET_PER_SECOND PACKET_SIZE] [CLIENT: TEST_IP TEST_DEV SERVER_IP]\n",
	     argv[ 0 ]);
    exit( 1 );
  }

  Socket data_socket;
  bool server;

  int sender_id = getpid();
  int packet_per_second = -1;
  int packet_size = -1;

  Socket::Address remote_data_address( UNKNOWN );

  uint64_t ts=Socket::timestamp();
  if ( argc == 3 ) { /* server */
    server = true;
    data_socket.bind( Socket::Address( "0.0.0.0", 9001 ) );
    packet_per_second = atoi(argv[ 1 ]);
    packet_size = atoi(argv[2]);
  } else { /* client */
    server = false;

    const char *test_ip = argv[ 1 ];
    const char *test_dev = argv[ 2 ];
    const char *server_ip = argv[ 3 ];
    
    sender_id = ( (int)(ts/1e9) );

    data_socket.bind( Socket::Address( test_ip, 9003 ) );
    data_socket.bind_to_device( test_dev );
    remote_data_address = Socket::Address( server_ip, 9001 );
  }

  FILE* log_file;
  char log_file_name[50];

  sprintf(log_file_name,"%s-%s-%s-%d-%d", server ? "server" : "client" , "downlink", "fixedrate" , (int)(ts/1e9),sender_id);
  log_file=fopen(log_file_name,"w");

  if (server) {
    fprintf(log_file, "Command %s %s %s\n", argv[0], argv[1], argv[2]);
  } else {
    fprintf(log_file, "Command %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);
  }
  

  FixedRateSending saturatr( server ? "OUTGOING" : "INCOMING", log_file, data_socket, remote_data_address, server, sender_id, packet_per_second, packet_size );


  if (!server) { // Wait for data
    saturatr.set_remote(remote_data_address);
    
    while ( 1 ) {
      fflush( NULL );
      saturatr.ping();
      /* wait for incoming packet OR expiry of timer */
      struct pollfd poll_fds[ 1 ];
      poll_fds[ 0 ].fd = data_socket.get_sock();
      poll_fds[ 0 ].events = POLLIN;

      struct timespec timeout;
      uint64_t next_transmission_delay = 1000000000;

      timeout.tv_sec = next_transmission_delay / 1000000000;
      timeout.tv_nsec = next_transmission_delay % 1000000000;
      ppoll( poll_fds, 1, &timeout, NULL );

      if ( poll_fds[ 0 ].revents & POLLIN ) {
        saturatr.recv_data();
      }

    }
  } else {
    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_data, &saturatr);
    while ( 1 ) {
      fflush( NULL );
      
      /* wait for incoming packet OR expiry of timer */
      struct pollfd poll_fds[ 1 ];
      poll_fds[ 0 ].fd = data_socket.get_sock();
      poll_fds[ 0 ].events = POLLIN;

      struct timespec timeout;
      uint64_t next_transmission_delay = saturatr.wait_time();

      if ( next_transmission_delay == 0 ) {
        fprintf( stderr, "ZERO %ld \n", saturatr.wait_time() );
      }

      timeout.tv_sec = next_transmission_delay / 1000000000;
      timeout.tv_nsec = next_transmission_delay % 1000000000;
      ppoll( poll_fds, 1, &timeout, NULL );

      if ( poll_fds[ 0 ].revents & POLLIN ) {
        saturatr.recv_ack();
      }
    }
  }
    

}
