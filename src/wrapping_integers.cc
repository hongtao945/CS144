#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32( ( n + zero_point.raw_value_ ) % ( 1LL << 32 ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t a = static_cast<uint64_t>( this->raw_value_ );
  uint64_t b = static_cast<uint64_t>( zero_point.raw_value_ );
  uint64_t data = b > a ? ( 1UL << 32 ) - ( ( b - a ) % ( 1UL << 32 ) ) : ( ( a - b ) % ( 1UL << 32 ) );
  if ( data + ( 1UL << 31 ) < checkpoint ) {
    data += ( ( checkpoint - data + ( 1UL << 31 ) ) / ( 1UL << 32 ) ) * ( 1UL << 32 );
  }
  return data;
}