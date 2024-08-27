#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

class RouteTableEntry;

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};
  std::vector<RouteTableEntry> _route_table {};
  std::vector<InternetDatagram> _need_to_send {};
};

class RouteTableEntry
{
public:
  uint32_t route_prefix { 0 };
  uint8_t prefix_length { 0 };
  std::optional<Address> next_hop {};
  size_t interface_num { 0 };

  RouteTableEntry() = default;

  RouteTableEntry( uint32_t route_prefix_,
                   uint8_t prefix_length_,
                   std::optional<Address> next_hop_,
                   size_t interface_num_ )
    : route_prefix( route_prefix_ )
    , prefix_length( prefix_length_ )
    , next_hop( next_hop_ )
    , interface_num( interface_num_ )
  {}

  std::pair<bool, uint32_t> matche_len( uint32_t address ) const
  {
    if(route_prefix == 0) {
      return  {true, 0};
    }
    if ( prefix_length != 0 ) {
      uint32_t mask = 0xFFFFFFFF << ( 32 - prefix_length );
      address &= mask;
    }
    return { route_prefix == address, prefix_length };
  }
};