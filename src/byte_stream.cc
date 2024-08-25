#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return this->is_close_;
}

void Writer::push( string data )
{
  if ( this->available_capacity() < data.size() ) {
    data = data.substr( 0, this->available_capacity() );
  }
  this->bytes_pushed_ += data.size();
  this->buf_.append( data );
}

void Writer::close()
{
  this->is_close_ = true;
}

uint64_t Writer::available_capacity() const
{
  return this->capacity_ - this->buf_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return this->bytes_pushed_;
}

bool Reader::is_finished() const
{
  return !this->buf_.size() && this->is_close_;
}

uint64_t Reader::bytes_popped() const
{
  return this->bytes_popped_;
}

string_view Reader::peek() const
{
  return string_view( this->buf_.data(), min( static_cast<uint64_t>( 2048 ), this->bytes_buffered() ) );
}

void Reader::pop( uint64_t len )
{
  this->buf_.erase( 0, len );
  this->bytes_popped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return this->buf_.size();
}
