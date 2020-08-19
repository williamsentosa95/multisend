#include <assert.h>
#include <string>
#include <iostream>

#include "saturateservo_oneway.hh"
#include "socket.hh"
#include "payload.hh"
#include "acker.hh"

#define PAYLOAD_SIZE 1428
#define LOGGING_INTERVAL_NS 1e9
#define DATA_PACKET_SIZE 1470


using namespace std;


SaturateServoOneWay::SaturateServoOneWay( const char * s_name,
                 FILE* log_file,
		 const Socket & s_socket,
		 const Socket::Address & s_remote,
		 const bool s_server,
		 const int s_send_id ) :
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
	_packet_counter_interval(0)
{}


void SaturateServoOneWay::recv_ack( void ) {
  /* get the ack packet */
  Socket::Packet incoming( _socket.recv() );
  SatPayload *contents = (SatPayload *) incoming.payload.data();
  contents->recv_timestamp = incoming.timestamp;

  if ( contents->sequence_number != -1 ) {
    /* not an ack */
    printf( "MARTIAN!\n" );
    return;
  }

  /* possibly roam */
  if ( _server ) {
      if ( (contents->sender_id > _foreign_id) && (contents->ack_number == -1) ) {
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
    /* increase-decrease rules */

    if ( (rtt < LOWER_RTT) && (_window < UPPER_WINDOW) ) {
      _window++;
    }

    if ( (rtt > UPPER_RTT) && (_window > LOWER_WINDOW + 10) ) {
      _window -= 20;
    }

    fprintf( _log_file, "%d\n", _window );
  }
}

void SaturateServoOneWay::recv_data( void ) {
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
  _socket.send( Socket::Packet( _remote, outgoing.str( sizeof( SatPayload ) ) ) );

  if (Socket::timestamp() - _logging_time > LOGGING_INTERVAL_NS) {
      float received_data = _packet_counter_interval * DATA_PACKET_SIZE * 8 / (float) 1e6;
      printf("Received data rate %.2f Mbps\n", received_data);
      _packet_counter_interval = 0;
      _logging_time = Socket::timestamp(); 
  } else {
    _packet_counter_interval++;
  }
  

   fprintf( _log_file,"%s DATA RECEIVED senderid=%d, seq=%d, send_time=%ld, recv_time=%ld, 1delay=%.4f \n",
      _name.c_str(),  _server ? contents->sender_id : _send_id, contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,oneway ); 

}

uint64_t SaturateServoOneWay::wait_time( void ) const {
  int num_outstanding = _packets_sent - _max_ack_id - 1;

  if ( _remote == UNKNOWN ) {
    return 1000000000;
  }

  if ( num_outstanding < _window ) {
    return 0;
  } else {
    int diff = _next_transmission_time - Socket::timestamp();
    if ( diff < 0 ) {
      diff = 0;
    }
    return diff;
  }
}

void SaturateServoOneWay::send_data( void ) {
  if ( _remote == UNKNOWN ) {
    return;
  }

  int num_outstanding = _packets_sent - _max_ack_id - 1;

  if ( num_outstanding < _window ) {
    /* send more packets */
    int amount_to_send = _window - num_outstanding;
    for ( int i = 0; i < amount_to_send; i++ ) {
      SatPayload outgoing;
      outgoing.sequence_number = _packets_sent;
      outgoing.ack_number = -1;
      outgoing.sent_timestamp = Socket::timestamp();
      outgoing.recv_timestamp = 0;
      outgoing.sender_id = _send_id;
      string data_to_send = outgoing.str(PAYLOAD_SIZE);

      _socket.send( Socket::Packet( _remote, data_to_send ) );

      // printf( "SaturateServo: %s pid=%d DATA SENT %d/%d senderid=%d seq=%d, send_time=%ld, recv_time=%ld, size=%lu\n",
      // _name.c_str(), _send_id, i+1, amount_to_send, outgoing.sender_id, outgoing.sequence_number, outgoing.sent_timestamp, outgoing.recv_timestamp, data_to_send.size() ); 

      // if (_server) {
      //   _send.send( Socket::Packet( _remote, data_to_send ) );

      //   printf( "SaturateServo: %s pid=%d DATA SENT %d/%d senderid=%d seq=%d, send_time=%ld, recv_time=%ld, size=%lu\n",
      //   _name.c_str(), _send_id, i+1, amount_to_send, outgoing.sender_id, outgoing.sequence_number, outgoing.sent_timestamp, outgoing.recv_timestamp, data_to_send.size() ); 
        
      // }
    
      _packets_sent++;
    }

    _next_transmission_time = Socket::timestamp() + _transmission_interval;
  }

  if ( _next_transmission_time < Socket::timestamp() ) {
    SatPayload outgoing;
    outgoing.sequence_number = _packets_sent;
    outgoing.ack_number = -1;
    outgoing.sent_timestamp = Socket::timestamp();
    outgoing.recv_timestamp = 0;
    outgoing.sender_id = _send_id;

    // if (_server) {
    //   _send.send( Socket::Packet( _remote, outgoing.str( 1400 ) ) );
    // }

    _socket.send( Socket::Packet( _remote, outgoing.str( PAYLOAD_SIZE ) ) );

    /*
    printf( "%s pid=%d DATA SENT senderid=%d seq=%d, send_time=%ld, recv_time=%ld\n",
    _name.c_str(), getpid(), outgoing.sender_id, outgoing.sequence_number, outgoing.sent_timestamp, outgoing.recv_timestamp ); */

    _packets_sent++;

    _next_transmission_time = Socket::timestamp() + _transmission_interval;
  }
}
