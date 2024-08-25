#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    this->reassembler_.reader().set_error();
  }
  if ( message.SYN ) {
    ISN = message.seqno;
    ISN.set_flag();
  }
  if ( ISN.get_flag() ) {
    if ( message.FIN ) {
      FIN = true;
    }
    if ( !message.SYN && message.seqno.raw_value() == ISN.raw_value() ) {
      return;
    }
    auto start_idx = message.seqno.unwrap( ISN, this->reassembler_.get_first_unassmebled_idx() );
    if ( start_idx > 0 )
      start_idx--;
    // if(FIN && this->reassembler_.bytes_pending() != 0) start_idx--;
    this->reassembler_.insert( start_idx, message.payload, message.FIN );
    current_idx += message.sequence_length();
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage res = TCPReceiverMessage();
  if ( ISN.get_flag() ) {
    res.ackno = Wrap32( ( FIN && this->reassembler_.bytes_pending() == 0 ) + 1
                        + this->reassembler_.get_first_unassmebled_idx() + ISN.raw_value() );
  }
  uint64_t window_size = this->reassembler_.writer().available_capacity();
  res.window_size = window_size < UINT16_MAX ? window_size : UINT16_MAX;
  res.RST = this->reassembler_.reader().has_error();
  return res;
}
