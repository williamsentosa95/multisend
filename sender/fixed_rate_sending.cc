#include <assert.h>
#include <string>
#include <iostream>

#include "fixed_rate_sending.hh"
#include "socket.hh"
#include "payload.hh"
#include "acker.hh"

#define PAYLOAD_SIZE 1428
#define LOGGING_INTERVAL_NS 1e9
#define PACKET_HEADER_SIZE 42

using namespace std;


FixedRateSending::FixedRateSending( const char * s_name,
                 FILE* log_file,
		 const Socket & s_socket,
		 const Socket::Address & s_remote,
		 const bool s_server,
		 const int s_send_id,
     const int s_pps,
     const int s_packet_size ) :
	_name(s_name),
	_log_file(log_file),
	_socket(s_socket),
	_remote(s_remote),
	_server(s_server),
	_send_id(s_send_id),
	_next_transmission_time ( Socket::timestamp() ),
	_foreign_id(-1),
	_packets_sent(0),
	_max_ack_id(-1),
	_window(LOWER_WINDOW),
	_logging_time( Socket::timestamp() ),
	_packet_counter_interval(0),
	_next_ping_time( Socket::timestamp() ),
  _pps ( s_pps ),
  _packet_size( s_packet_size )
{}


void FixedRateSending::recv_ack( void ) {
  /* get the ack packet */
  Socket::Packet incoming( _socket.recv() );
  SatPayload *contents = (SatPayload *) incoming.payload.data();
  contents->recv_timestamp = incoming.timestamp;

  if (contents->ack_number == -1 && contents->sequence_number == -1) { // Process ping heartbeat
  	set_remote(incoming.addr);
  	return;
  }

  if ( contents->sequence_number != -1 ) {
    /* not an ack */
    printf( "MARTIAN!\n" );
    return;
  }

  /* possibly roam */
  if ( _server ) {
      if ( (contents->sender_id > _foreign_id) ) {
	_foreign_id = contents->sender_id;
      }
  }

  /* process the ack */
  if ( contents->sender_id != _send_id ) {
    /* not from us */
    return;
  } else {
    if ( contents->ack_number > _max_ack_id ) {
      _max_ack_id = contents->ack_number;
    }

    /*    printf( "%s pid=%d ACK RECEIVED senderid=%d seq=%d, send_time=%ld, recv_time=%ld\n",
	  _name.c_str(), getpid(), contents->sender_id, contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp ); */

    int64_t rtt_ns = contents->recv_timestamp - contents->sent_timestamp;
    double rtt = rtt_ns / 1.e9;

     // printf("SaturateServo: %s ACK RECEIVED senderid=%d, seq=%d, send_time=%ld,  recv_time=%ld, rtt=%.4f, size=%lu\n",
     //   _name.c_str(),_server ? _foreign_id : contents->sender_id , contents->ack_number, contents->sent_timestamp, contents->recv_timestamp, (double)rtt,  incoming.payload.size() );

    fprintf( _log_file, "%s ACK RECEIVED senderid=%d, seq=%d, send_time=%ld,  recv_time=%ld, rtt=%.4f, %d => ",
       _name.c_str(),_server ? _foreign_id : contents->sender_id , contents->ack_number, contents->sent_timestamp, contents->recv_timestamp, (double)rtt,  _window );

    fprintf( _log_file, "%d\n", _window );
  }
}

void FixedRateSending::recv_data( void ) {
  /* get the data packet */
  Socket::Packet incoming( _socket.recv() );
  SatPayload *contents = (SatPayload *) incoming.payload.data();
  contents->recv_timestamp = incoming.timestamp;

  int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
  double oneway = oneway_ns / 1.e9;

  if ( _server ) {
      if ( contents->sender_id > _foreign_id ) {
		_foreign_id = contents->sender_id;
		set_remote( incoming.addr );
      }

    if ( _remote == UNKNOWN ) {
      return;
    }
  }

  assert( !(_remote == UNKNOWN) );

  Socket::Address fb_destination( _remote );

  /* send ack */
  SatPayload outgoing( *contents );
  outgoing.sequence_number = -1;
  outgoing.ack_number = contents->sequence_number;
  _socket.send( Socket::Packet( _remote, outgoing.str( incoming.payload.size() ) ) );

  if (Socket::timestamp() - _logging_time > LOGGING_INTERVAL_NS) {
      float received_data = _packet_counter_interval * incoming.payload.size() * 8 / (float) 1e6;
      printf("Received data rate %.2f Mbps, #packets = %lu, content size = %lu \n", received_data, _packet_counter_interval, incoming.payload.size());
      _packet_counter_interval = 0;
      _logging_time = Socket::timestamp(); 
  } else {
    _packet_counter_interval++;
  }
  

   fprintf( _log_file,"%s DATA RECEIVED senderid=%d, seq=%d, send_time=%ld, recv_time=%ld, 1delay=%.4f \n",
      _name.c_str(),  _server ? contents->sender_id : _send_id, contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,oneway ); 

}

uint64_t FixedRateSending::wait_time( void ) const {

  if ( _remote == UNKNOWN ) {
    return 1000000000;
  }

  return 1000000000 / _pps;
}


void FixedRateSending::send_data( void ) {
  if ( _remote == UNKNOWN ) {
    return;
  }

  SatPayload outgoing;
  outgoing.sequence_number = _packets_sent;
  outgoing.ack_number = -1;
  outgoing.sent_timestamp = Socket::timestamp();
  outgoing.recv_timestamp = 0;
  outgoing.sender_id = _send_id;
  string data_to_send = outgoing.str(_packet_size - PACKET_HEADER_SIZE);

  _socket.send( Socket::Packet( _remote, data_to_send ) );
  _packets_sent++;

}
