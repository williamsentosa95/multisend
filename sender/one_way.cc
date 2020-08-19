#include <string>
#include <vector>
#include <poll.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#include "acker.hh"
#include "saturateservo_oneway.hh"

using namespace std;

int main( int argc, char *argv[] )
{
  if ( argc != 2 && argc != 5 ) {
    fprintf( stderr, "Usage: %s DIRECTION [TEST_IP TEST_DEV SERVER_IP]\n",
	     argv[ 0 ]);
    exit( 1 );
  }

  Socket data_socket;
  bool server;

  int sender_id = getpid();

  Socket::Address remote_data_address( UNKNOWN );

  uint64_t ts=Socket::timestamp();
  if ( argc == 2 ) { /* server */
    server = true;
    data_socket.bind( Socket::Address( "0.0.0.0", 9001 ) );
  } else { /* client */
    server = false;

    const char *test_ip = argv[ 2 ];
    const char *test_dev = argv[ 3 ];

    const char *server_ip = argv[ 4 ];

    sender_id = ( (int)(ts/1e9) );

    data_socket.bind( Socket::Address( test_ip, 9003 ) );
    data_socket.bind_to_device( test_dev );
    remote_data_address = Socket::Address( server_ip, 9001 );
  }

  FILE* log_file;
  char log_file_name[50];
  const char * direction = argv[1];
  bool uplink = (strcmp(direction, "up") == 0);

  bool is_send_data = (server && !uplink) || (!server && uplink);

  sprintf(log_file_name,"%s-%s-%s-%d-%d", server ? "server" : "client", uplink ? "up" : "down", is_send_data ? "ack" : "data", (int)(ts/1e9),sender_id);
  log_file=fopen(log_file_name,"w");

  SaturateServoOneWay saturatr( is_send_data ? "OUTGOING" : "INCOMING", log_file, data_socket, remote_data_address, server, sender_id );

  if (!server) {
    saturatr.set_remote(remote_data_address);
  }

  if (is_send_data) { // Send data
      while ( 1 ) {
        fflush( NULL );

        /* possibly send packet */
        saturatr.send_data();
        
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
  } else {
    while ( 1 ) {
        fflush( NULL );

        if (!server) {
          saturatr.ping();
        }

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
  }

}
