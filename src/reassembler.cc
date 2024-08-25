#include "reassembler.hh"
#include <iomanip>
#include <iostream>
using namespace std;

bool Reassembler::overlapping( std::string data, uint64_t idx )
{
  if ( idx + data.size() < next_idx )
    return false;
  if ( idx < this->pre_node_.first_index ) {
    return idx + data.size() >= next_idx;
  }
  uint64_t idxx = idx - this->pre_node_.first_index;
  return !this->pre_node_.data.empty() &&
         // data != this->pre_node_.data &&
         data.starts_with( this->pre_node_.data.substr( idxx, this->pre_node_.data.size() ) );
  //  && idx == this->pre_node_.first_index;
}

void Reassembler::insert_operation( uint64_t first_index, std::string& data )
{
  // cout << "insert_operation: " << data << endl;
  output_.writer().push( data );
  this->next_idx += data.size();
  this->pre_node_.first_index = first_index;
  this->pre_node_.data = data;
}

void Reassembler::insert_next()
{
  vector<PreNode> toErase;
  for ( auto it = this->pending_data.begin(); it != this->pending_data.end(); it++ ) {
    // cout << it->first_index << " " << it->data << endl;
    if ( it->first_index > next_idx )
      break;
    uint64_t first_index = it->first_index;
    string data = it->data;
    toErase.push_back( *it );
    if ( first_index < next_idx && this->overlapping( data, first_index ) ) {
      data = data.substr( next_idx - first_index, data.size() );
      // data = data.substr( 0, min( data.size(), writer().available_capacity() ) );
      if ( data.size() != 0 )
        this->insert_operation( next_idx, data );
    } else if ( first_index == next_idx ) {
      this->insert_operation( next_idx, data );
    }
  }
  for ( auto it = toErase.begin(); it != toErase.end(); it++ ) {
    this->pending_data.erase( *it );
  }
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring )
    this->last_is_occurence = true;
  this->last_idx = max( first_index + data.size(), this->last_idx );
  Writer& writer = output_.writer();
  if ( first_index == next_idx ) {
    // cout << data.size() << writer.available_capacity() << endl;
    data = data.substr( 0, min( data.size(), writer.available_capacity() ) );
    this->insert_operation( first_index, data );
    this->insert_next();
  } else if ( next_idx < first_index ) {
    data = data.substr( 0, min( data.size(), writer.available_capacity() + next_idx - first_index ) );
    if ( data.size() != 0 ) {
      PreNode first, last;
      uint64_t start_idx = 0, end_idx = data.size();
      vector<PreNode> related_data;
      for ( auto it = this->pending_data.begin(); it != this->pending_data.end(); it++ ) {
        auto sizea = first_index + data.size();
        auto sizeb = it->first_index + it->data.size();
        if ( it->first_index <= first_index && sizeb >= sizea )
          return;
        if ( sizea <= it->first_index )
          break;
        if ( sizeb <= first_index )
          continue;
        if ( it->first_index >= first_index && sizeb <= sizea ) {
          related_data.push_back( *it );
        } else if ( it->first_index <= first_index && sizeb >= first_index && sizeb < sizea ) {
          first = *it;
        } else if ( it->first_index > first_index && it->first_index <= sizea && sizeb >= sizea ) {
          last = *it;
        }
      }
      if ( first.first_index != 0 ) {
        start_idx = first.first_index + first.data.size() - first_index;
        first_index = first.first_index + first.data.size();
      }
      if ( last.first_index != 0 ) {
        end_idx = last.first_index - first_index;
      }
      // cout << data << endl;
      data = data.substr( start_idx, end_idx );
      // cout << first_index << " " << data << endl;
      for ( auto it = related_data.begin(); it != related_data.end(); it++ ) {
        this->pending_data.erase( *it );
      }
      this->pending_data.insert( PreNode( first_index, data ) );
    }
  } else if ( next_idx > first_index ) {
    if ( !this->overlapping( data, first_index ) )
      return;
    data = data.substr( next_idx - first_index, data.size() );
    data = data.substr( 0, min( data.size(), writer.available_capacity() ) );

    if ( data.size() != 0 ) {
      this->insert_operation( next_idx, data );
      this->insert_next();
    }
  }
  if ( this->last_is_occurence && this->pending_data.empty() && next_idx == last_idx ) {
    writer.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // 统计pending_data中的各个字符串的长度之和
  uint64_t sum = 0;
  for ( auto it = pending_data.begin(); it != pending_data.end(); it++ ) {
    sum += it->data.size();
  }
  return sum;
}
