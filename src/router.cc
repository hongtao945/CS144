#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  _route_table.push_back( RouteTableEntry( route_prefix, prefix_length, next_hop, interface_num ) );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  for ( auto& it : _interfaces ) {
    InternetDatagram dgram;
    while ( !it->datagrams_received().empty() ) {
      _need_to_send.push_back( it->datagrams_received().front() );
      it->datagrams_received().pop();
    }
  }
  vector<InternetDatagram> no_match;
  for ( auto& dgram : _need_to_send ) {
    if ( dgram.header.ttl == 0 || dgram.header.ttl == 1 ) {
      continue;
    }
    uint32_t max_len = 0;
    bool can_send = false;
    RouteTableEntry rte;
    for ( auto& rt : _route_table ) {
      auto res = rt.matche_len( dgram.header.dst );
      if ( res.first ) {
        can_send = true;
      }
      if ( res.first && ( res.second > max_len || max_len == 0 ) ) {
        max_len = res.second;
        rte = rt;
      }
    }
    if ( !can_send ) {
      no_match.push_back( dgram );
      continue;
    }
    dgram.header.ttl -= 1;
    Address next_hop
      = rte.next_hop.has_value() ? rte.next_hop.value() : Address::from_ipv4_numeric( dgram.header.dst );
    dgram.header.compute_checksum();
    interface( rte.interface_num )->send_datagram( dgram, next_hop );
  }
  _need_to_send = move( no_match );
}
