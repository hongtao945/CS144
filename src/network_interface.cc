#include "network_interface.hh"
#include "arp_message.hh"
#include "exception.hh"
#include <iostream>
#include <vector>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  uint32_t ip = next_hop.ipv4_numeric();
  EthernetFrame frame = EthernetFrame();
  Serializer serializer;
  frame.header = EthernetHeader();
  frame.header.src = ethernet_address_;
  // cout << "DEBUG: Sending datagram to " << ip << endl;
  // cout << already_known_address_.find( ip ) == already_known_address_.end() << endl;
  if ( already_known_address_.find( ip ) == already_known_address_.end() ) {
    EthernetFrame cache_frame = EthernetFrame();
    EthernetHeader cache_header = EthernetHeader();
    cache_header.src = ethernet_address_;
    cache_header.type = EthernetHeader::TYPE_IPv4;
    cache_frame.header = cache_header;
    Serializer s;
    dgram.serialize( s );
    cache_frame.payload = s.output();
    cache_data_.emplace( ip, cache_frame );
    if ( pending_requests_.find( ip ) != pending_requests_.end() ) {
      if ( pending_requests_[ip] > static_cast<uint64_t>( time_since_last_tick_ ) ) {
        return;
      }
      pending_requests_.erase( ip );
    }
    pending_requests_[ip] = static_cast<uint64_t>( time_since_last_tick_ ) + 5000;
    ARPMessage arp;
    arp.hardware_type = ARPMessage::TYPE_ETHERNET;
    arp.protocol_type = EthernetHeader::TYPE_IPv4;
    arp.hardware_address_size = sizeof( EthernetHeader::src );
    arp.protocol_address_size = sizeof( IPv4Header::src );
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = ethernet_address_;
    arp.sender_ip_address = ip_address_.ipv4_numeric();
    arp.target_ethernet_address = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    arp.target_ip_address = ip;
    arp.serialize( serializer );
    frame.payload = serializer.output();
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.dst = ETHERNET_BROADCAST;
  } else {
    dgram.serialize( serializer );
    frame.header.dst = already_known_address_[ip].ea;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serializer.output();
  }
  transmit( frame );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( !parse( dgram, frame.payload ) ) {
      return;
    }
    if ( dgram.header.src == ip_address_.ipv4_numeric() ) {
      return;
    }
    datagrams_received_.push( dgram );
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp;
    if ( !parse( arp, frame.payload ) ) {
      return;
    }
    EaAndTime eat( arp.sender_ethernet_address, static_cast<uint64_t>( time_since_last_tick_ ) + 30000 );
    already_known_address_[arp.sender_ip_address] = eat;
    if ( cache_data_.contains( arp.sender_ip_address ) ) {
      auto range = cache_data_.equal_range( arp.sender_ip_address );
      for ( auto i = range.first; i != range.second; i++ ) {
        i->second.header.dst = arp.sender_ethernet_address;
        transmit( i->second );
      }
      cache_data_.erase( range.first, range.second );
    }
    Serializer serializer;
    if ( arp.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage reply;
        reply.hardware_type = ARPMessage::TYPE_ETHERNET;
        reply.protocol_type = EthernetHeader::TYPE_IPv4;
        reply.hardware_address_size = sizeof( EthernetHeader::src );
        reply.protocol_address_size = sizeof( IPv4Header::src );
        reply.opcode = ARPMessage::OPCODE_REPLY;
        reply.sender_ethernet_address = ethernet_address_;
        reply.sender_ip_address = ip_address_.ipv4_numeric();
        reply.target_ethernet_address = arp.sender_ethernet_address;
        reply.target_ip_address = arp.sender_ip_address;
        reply.serialize( serializer );
        EthernetFrame reply_frame;
        reply_frame.header = EthernetHeader();
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.dst = arp.sender_ethernet_address;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;
        reply_frame.payload = serializer.output();
        transmit( reply_frame );
      }
    } 
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time_since_last_tick_ += ms_since_last_tick;
  vector<uint32_t> to_erase;
  // 遍历already_known_address_
  for ( auto it = already_known_address_.begin(); it != already_known_address_.end(); ++it ) {
    if ( it->second.time < static_cast<uint64_t>( time_since_last_tick_ ) ) {
      to_erase.push_back( it->first );
    }
  }
  for ( auto it = to_erase.begin(); it != to_erase.end(); ++it ) {
    already_known_address_.erase( *it );
  }
}
