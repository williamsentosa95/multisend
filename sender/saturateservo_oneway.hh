#ifndef SATURATESERVO_ONEWAY_HH
#define SATURATESERVO_ONEWAY_HH

#include <fstream>
#include "socket.hh"

class SaturateServoOneWay
{
private:
  const std::string _name;
  FILE* _log_file;

  const Socket _socket;
  Socket::Address _remote;

  const bool _server;
  const int _send_id;


  uint64_t _next_transmission_time;

  static const int _transmission_interval = 1000 * 1000 * 1000;

  int _foreign_id;

  int _packets_sent, _max_ack_id;

  int _window;

  uint64_t _logging_time;

  uint64_t _packet_counter_interval;

  uint64_t _next_ping_time;  

  static const int LOWER_WINDOW = 20;
  static const int UPPER_WINDOW = 1500;

  static constexpr double LOWER_RTT = 0.75;
  static constexpr double UPPER_RTT = 3.0;

public:
  SaturateServoOneWay( const char * s_name,
                 FILE* log_file,
		 const Socket & s_socket,
		 const Socket::Address & s_remote,
		 const bool s_server,
		 const int s_send_id );

  void recv_data( void );
  void recv_ack( void );

  uint64_t wait_time( void ) const;

  void send_data( void );

  void ping( void );

  void set_remote( const Socket::Address & s_remote ) { _remote = s_remote; }

  SaturateServoOneWay( const SaturateServoOneWay & ) = delete;
  const SaturateServoOneWay & operator=( const SaturateServoOneWay & ) = delete;
};

#endif
