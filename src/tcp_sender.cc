#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  Reader& reader = input_.reader();

  // cout << "ii  " << last_seqno_.raw_value() << endl;
  if ( !first_fin_ || !first_win_size_zero_ ) {
    // cout << "finished" << endl;
    return;
  }
  if ( current_window_size_ == 0 && first_win_size_zero_ == 2 ) {
    first_win_size_zero_ = 0;
    current_window_size_ = 1;
  }
  TCPSenderMessage msg;
  msg.RST = input_.has_error();
  if ( first_msg_ ) {
    msg.SYN = true;
    msg.seqno = isn_;
    first_msg_ = false;
  } else {
    if ( current_window_size_ == 0 ) {
      cout << "current_window_size_ == 0" << endl;
      return;
    } else {
      while ( current_window_size_ > 0 || reader.is_finished() ) {
        auto need_read_bytes
          = current_window_size_ > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE : current_window_size_;
        string data;
        while ( need_read_bytes > 0 ) {
          string_view peeked = reader.peek();
          if ( peeked.empty() ) {
            if ( data.empty() && !input_.writer().is_closed() ) {
              // cout << "empty" << endl;
              return;
            }
            break;
          }
          peeked = peeked.substr( 0, min( need_read_bytes, (uint64_t)peeked.size() ) );
          data.append( peeked );
          need_read_bytes -= peeked.size();
          reader.pop( peeked.size() );
        }
        msg.seqno = last_seqno_;
        msg.payload = std::move( data );
        if ( current_window_size_ == msg.sequence_length() || reader.is_finished() ) {
          break;
        }
        transmit( msg );
        current_window_size_ -= msg.sequence_length();
        last_seqno_ = last_seqno_ + msg.sequence_length();
        in_flight_ += msg.sequence_length();
        remaining_data_.push( msg );
      }
      // cout << msg.payload << " " << data << endl;
    }
  }
  // cout << msg.SYN << " " << msg.FIN << " " << current_window_size_ << " " << msg.sequence_length() << endl;
  // if ( current_window_size_ < msg.sequence_length() && !msg.SYN && !msg.FIN ) {
  //   first_fin_ = true;
  //   last_seqno_ = last_seqno_ + current_window_size_;
  //   return;
  // }
  // cout << msg.SYN << " " << msg.FIN << endl;
  msg.FIN = input_.writer().is_closed() && reader.is_finished();
  // cout << current_window_size_ << " " << msg.sequence_length() << endl;
  if ( !msg.SYN && msg.sequence_length() > current_window_size_ )
    msg.FIN = false;
  // if ( !first_fin_ ) {
  //   msg.FIN = false;
  // }
  // cout << "hh  " << msg.sequence_length() << " " << current_window_size_ << endl;
  transmit( msg );

  last_seqno_ = last_seqno_ + msg.sequence_length();
  in_flight_ += msg.sequence_length();
  current_window_size_ -= msg.sequence_length();
  remaining_data_.push( msg );
  if ( msg.FIN ) {
    first_fin_ = false;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = last_seqno_;
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
  }
  if ( msg.ackno.has_value() && msg.ackno.value().raw_value() >= recv_sqno_.raw_value() ) {
    Wrap32 ackno = msg.ackno.value();
    if ( ackno.raw_value() > last_seqno_.raw_value() ) {
      return;
    }
    if ( !remaining_data_.empty()
         && remaining_data_.front().seqno.raw_value() + remaining_data_.front().sequence_length()
              <= ackno.raw_value() ) {
      initial_RTO_ms_ = initial_RTO_ms_origin_;
      consecutive_retransmissions_ = 0;
      last_tick_ = 0;
    }
    while ( !remaining_data_.empty()
            && remaining_data_.front().seqno.raw_value() + remaining_data_.front().sequence_length()
                 <= ackno.raw_value() ) {
      in_flight_ -= remaining_data_.front().sequence_length();
      remaining_data_.pop();
    }
    uint64_t temp = ackno.raw_value() + msg.window_size;
    if ( last_seqno_.raw_value() < temp ) {
      current_window_size_ = msg.window_size;
    } else {
      current_window_size_ = last_seqno_.raw_value() - temp;
    }
    // current_window_size_ = msg.window_size;
    first_win_size_zero_ = msg.window_size == 0 ? 2 : 1;
    recv_sqno_ = ackno;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  last_tick_ += ms_since_last_tick;
  // cout << last_tick_ << " " << ms_since_last_tick << " " <<  initial_RTO_ms_ << endl;
  if ( remaining_data_.empty() || last_tick_ < initial_RTO_ms_ ) {
    return;
  }
  last_tick_ -= initial_RTO_ms_;
  if ( first_win_size_zero_ != 0 )
    initial_RTO_ms_ <<= 1;
  auto data = remaining_data_.front();
  transmit( data );
  consecutive_retransmissions_++;
}
