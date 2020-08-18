#include <assert.h>

#include "acker.hh"
#include "payload.hh"
#include "saturateservo.hh"

#define DATA_PACKET_SIZE 1470
#define LOGGING_INTERVAL_NS 1e9

Acker::Acker( const char *s_name, FILE* log_file_handle, const Socket & s_listen, const Socket & s_send, const Socket::Address & s_remote, const bool s_server, const int s_ack_id )
  : _name( s_name ), 
    _log_file(log_file_handle),
    _listen( s_listen ),
    _send( s_send ),
    _remote( s_remote ),
    _server( s_server ),
    _ack_id( s_ack_id ),
    _saturatr( NULL ),
    _next_ping_time( Socket::timestamp() ),
    _logging_time( Socket::timestamp() ),
    _foreign_id( -1 ),
    _packet_counter_interval(0)
{}

void Acker::recv( void )
{
  /* get the data packet */
  Socket::Packet incoming( _listen.recv() );
  SatPayload *contents = (SatPayload *) incoming.payload.data();
  contents->recv_timestamp = incoming.timestamp;

  int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
  double oneway = oneway_ns / 1.e9;

  printf("Received data packet 1rtt %.4f\n", oneway);

  if ( _server ) {
    if ( _saturatr ) {
      if ( contents->sender_id > _foreign_id ) {
	_foreign_id = contents->sender_id;
	_saturatr->set_remote( incoming.addr );
      }
    }

    if ( _remote == UNKNOWN ) {
      printf("FAILLLLLL!! Remote unknown!")
      return;
    }
  }

  assert( !(_remote == UNKNOWN) );

  Socket::Address fb_destination( _remote );

  /* send ack */
  SatPayload outgoing( *contents );
  outgoing.sequence_number = -1;
  outgoing.ack_number = contents->sequence_number;
  printf("--- Sending ACK ---\n");
  _send.send( Socket::Packet( _remote, outgoing.str( sizeof( SatPayload ) ) ) );

  if (Socket::timestamp() - _logging_time > LOGGING_INTERVAL_NS) {
      float received_data = _packet_counter_interval * DATA_PACKET_SIZE * 8 / (float) 1e6;
      printf("Received data rate %.2f Mbps\n", received_data);
      _packet_counter_interval = 0;
      _logging_time = Socket::timestamp(); 
  } else {
    _packet_counter_interval++;
  }
  

   fprintf( _log_file,"%s DATA RECEIVED senderid=%d, seq=%d, send_time=%ld, recv_time=%ld, 1delay=%.4f \n",
      _name.c_str(),  _server ? contents->sender_id : _ack_id, contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,oneway ); 
}

void Acker::tick( void )
{
  if ( _server ) {
    return;
  }

  /* send NAT heartbeats */
  if ( _remote == UNKNOWN ) {
    _next_ping_time = Socket::timestamp() + _ping_interval;
    return;
  }

  if ( _next_ping_time < Socket::timestamp() ) {
    SatPayload contents;
    contents.sequence_number = -1;
    contents.ack_number = -1;
    contents.sent_timestamp = Socket::timestamp();
    contents.recv_timestamp = 0;
    contents.sender_id = _ack_id;

    _send.send( Socket::Packet( _remote, contents.str( sizeof( SatPayload ) ) ) );

    _next_ping_time = Socket::timestamp() + _ping_interval;
  }
}

uint64_t Acker::wait_time( void ) const
{
  if ( _server ) {
    return 1000000000;
  }

  int diff = _next_ping_time - Socket::timestamp();
  if ( diff < 0 ) {
    diff = 0;
  }

  return diff;
}
