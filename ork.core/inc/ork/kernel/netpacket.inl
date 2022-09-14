////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/atomic.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

namespace ork {

using zmq_socket_ptr_t = std::shared_ptr<::zmq::socket_t>;

template <size_t ksize> struct MessagePacket;
typedef MessagePacket<4096> NetworkMessage;

//! data read marker/iterator for a NetworkMessage
template <size_t ksize> struct MessagePacketIterator
{   ///////////////////////////////////////////////////
    // Constructor: NetworkMessageIterator
    //
    // construct a Network Message read Iterator
    //
    // Parameters:
    //
    // msg - Network Message that this iterator will iterate upon
    //
    MessagePacketIterator( const MessagePacket<ksize>& msg )
        : mMessage(msg)
        , mireadIndex(0)
    {

    }

    template <typename T> T readItem() {
    	T rval;
        mMessage.read(rval,*this);
	return rval;
    }

    template <typename T> inline void _swapBytesInPlace( T& item ) // inplace endian swap
    {
	int isize = sizeof(T);

	T temp = item;

	auto src = reinterpret_cast<uint8_t *>( & item );
	auto tmp = reinterpret_cast<uint8_t *>( & temp );

	for( int i=0, j=isize-1; i<isize; i++, j-- )
	{
		tmp[j] = src[i];
	}

	for( int i=0; i<isize; i++ )
	{
		src[i] = tmp[i];
	}

    }

    template <typename T> T readItemSwapped() {
        T rval;
        mMessage.read(rval,*this);
	_swapBytesInPlace<T>(rval);
        return rval;
    }


    ///////////////////////////////////////////////////
    void clear() { mireadIndex=0; }
    bool valid() const { return mireadIndex<mMessage.length(); }
    int index() const { return mireadIndex; }
    void skip(int count) { mireadIndex+=count; }
    ////////////////////////////////////////////////////
    // Variable: mMessage
    // Network Message this iterator is iterating inside
    const MessagePacket<ksize>&     mMessage;
    ////////////////////////////////////////////////////
    // Variable: mireadIndex
    // read Byte Index into the message
    int                             mireadIndex;
    ////////////////////////////////////////////////////
};

typedef MessagePacketIterator<4096> NetworkMessageIterator;

//! message packet (an atomic network message)
//! statically sized, storage for the message is embedded.
//! This allow explicit allocation policies, such as embedding in shared memory

struct MessagePacketBase
{
    virtual ~MessagePacketBase(){}

    mutable uint64_t        miSerial;
    mutable uint64_t        miTimeSent;

    ///////////////////////////////////////////////////////
    virtual void clear() = 0;
    virtual const void* data() const = 0;
    virtual void* data() = 0;
    virtual size_t max() const = 0;
    virtual size_t length() const =0;
    ///////////////////////////////////////////////////////
};

template <size_t ksize> struct MessagePacket : public MessagePacketBase
{
    typedef MessagePacketIterator<ksize> iter_t;

    ///////////////////////////////////////////////////////
    // constructor: MessagePacket
    //
    // construct a message packet
    //
    ///////////////////////////////////////////////////////
    MessagePacket() { clear(); }

    ///////////////////////////////////////////////////////
    // create a read iterator (pointing to the start of this message)
    ///////////////////////////////////////////////////////

    iter_t makeIterator() const {  
        return iter_t(*this);
    }

    ///////////////////////////////////////////////////////
    // method: write
    //
    // add data to the content of this message, increment write index by size of T
    //
    // Parameters:
    //
    // inp - the data to add
    //
    ///////////////////////////////////////////////////////
    static const int kstrbuflen = ksize/2;
    template <typename T> void write( const T& inp )
    {
        static_assert(std::is_trivially_copyable<T>::value,"can only write is_trivially_copyable's into a NetworkMessage!");
        size_t ilen = sizeof(T);
        writeDataInternal((const void*) & inp, ilen);
    }
    void writeString( const char* pstr )
    {
        size_t ilen = strlen(pstr)+1;
        assert( (miSize+ilen)<kmaxsize );
        assert( ilen<kstrbuflen );
        writeDataInternal(&ilen,sizeof(ilen));
        writeDataInternal(pstr,ilen);
        //for( size_t i=0; i<ilen; i ++ )
        //  write( pstr[i] ); // slow method, oh well doesnt matter now...
    }
    void writeString( const std::string& str )
    {
        size_t ilen = str.length()+1;
        assert( (miSize+ilen)<kmaxsize );
        assert( ilen<kstrbuflen );
        writeDataInternal(&ilen,sizeof(ilen));
        writeDataInternal(str.c_str(),ilen);
        //for( size_t i=0; i<ilen; i ++ )
        //  write( pstr[i] ); // slow method, oh well doesnt matter now...
    }
    void writeData( const void* pdata, size_t ilen )
    {
        assert( (miSize+ilen)<=kmaxsize );
        write(ilen);
        writeDataInternal(pdata,ilen);
    }
    void writeDataInternal( const void* pdata, size_t ilen )
    {
        assert( (miSize+ilen)<=kmaxsize );
        const char* pch = (const char*) pdata;
        char* pdest = & mBuffer[miSize];
        memcpy( pdest, (const char*) pdata, ilen );
        miSize += ilen;
    }
    ///////////////////////////////////////////////////////
    // method: read
    //
    // read data from the content of this message at a given position specified by an iterator, then increment read index by size of T
    //
    // Parameters:
    //
    // it - the data's read iterator
    //
    ///////////////////////////////////////////////////////
    template <typename T> void read( T& outp, iter_t& it ) const
    {
        static_assert(std::is_trivially_copyable<T>::value,"can only read is_trivially_copyable's from a NetworkMessage!");
        size_t ilen = sizeof(T);
        assert( (it.mireadIndex+ilen)<=kmaxsize );
        readDataInternal((void*)&outp,ilen,it);
    }
    std::string readString( iter_t& it ) const
    {
        size_t ilen = 0;
        char buffer[kstrbuflen];
        //read( ilen, it );
        readDataInternal((void*)&ilen,sizeof(ilen),it);
        assert( ilen<kstrbuflen );
        readDataInternal((void*)buffer,ilen,it);
        //for( size_t i=0; i<ilen; i++ )
        //  read( buffer[i], it );
        return std::string(buffer);
    }
    void readData( void* pdest, size_t ilen, iter_t& it ) const
    {
        size_t rrlen = 0;
        read( rrlen, it );
        assert(rrlen==ilen);
        readDataInternal(pdest,ilen,it);
    }
    void readDataInternal( void* pdest, size_t ilen, iter_t& it ) const
    {
        const char* psrc = & mBuffer[it.mireadIndex];
        memcpy( (char*) pdest, psrc, ilen );
        it.mireadIndex += ilen;
    }
    ///////////////////////////////////////////////////////
    NetworkMessage& operator = ( const NetworkMessage& rhs )
    {
        miSize = rhs.miSize;
        miSerial = rhs.miSerial;
        miTimeSent = rhs.miTimeSent;

        if( miSize )
            memcpy(mBuffer,rhs.mBuffer,miSize);

        return *this;
    }
    ////////////////////////////////////////////////////
    void sendZmq(zmq_socket_ptr_t skt) {
        zmq::message_t zmqmsg_send(this->data(), this->length());
        skt->send(zmqmsg_send, zmq::send_flags::dontwait);
    }
    void recvZmq(zmq_socket_ptr_t skt) {
        zmq::message_t zmqmsg_recv;
        auto recv_status = skt->recv(zmqmsg_recv);
        this->clear();
        this->writeDataInternal(zmqmsg_recv.data(), zmqmsg_recv.size());
    }
    ////////////////////////////////////////////////////
    void dump(const char* label)
    {
        size_t icount = miSize;
        size_t j = 0;
        while(icount>0)
        {
            uint8_t* paddr = (uint8_t*)(mBuffer+j);
            printf( "msg<%p:%s> [%02lx : ", this,label, j );
            size_t thisc = (icount>=16) ? 16 : icount;
            for( size_t i=0; i<thisc; i++ )
            {
                size_t idx = j+i;
                printf( "%02x ", paddr[i] );
            }
            j+=thisc;
            icount -= thisc;

            printf("\n");
        }
    }
    const void* data() const final { return (const void*) mBuffer; }
    void* data() final { return (void*) mBuffer; }
    size_t max() const final { return kmaxsize; }
    size_t length() const final { return miSize; }
    void clear() final { miSize = 0; }

    ////////////////////////////////////////////////////
    // Constant: kmaxsize
    // maximum size of content of a single atomic network message
    static const size_t kmaxsize = ksize;
    ////////////////////////////////////////////////////
    // Variable: mBuffer
    // statically sized buffer for holding a network message's content
    char                mBuffer[kmaxsize];
    size_t                  miSize;
    ////////////////////////////////////////////////////
};

//! Message Stream Abstract interface
//! allows bidirectional serialization with half the code (serdes)

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct MessageStreamTraits;

///////////////////////////////////////////////////////////////////////////////

struct MessageStreamBase
{
    template <typename T> MessageStreamBase& operator || ( T& inp );

    virtual void serdesImpl( void* pdata, size_t len ) = 0;
    virtual void serdesStringImpl( std::string& str ) = 0;
    virtual void serdesFixedStringImpl( FixedStringBase& str ) = 0;
    virtual bool isOutputStream() const = 0;

    NetworkMessage          mMessage;

};
///////////////////////////////////////////////////////////////////////////////
template <typename T> struct MessageStreamTraits
{   static void serdes(MessageStreamBase& stream_bas, T& data_ref)
    {   static_assert(std::is_trivially_copyable<T>::value,"can only write is_trivially_copyable's into a NetworkMessage!");
        size_t ilen = sizeof(T);
        stream_bas.serdesImpl(&data_ref,ilen);
    }
};
///////////////////////////////////////////////////////////////////////////////
// TODO - figure out how to templatize on size of the fxstring!
template<> struct MessageStreamTraits<fxstring<64>>
{   static void serdes(MessageStreamBase& stream_bas, fxstring<64>& data_ref)
    {   stream_bas.serdesFixedStringImpl(data_ref);
    }
};
///////////////////////////////////////////////////////////////////////////////
template<> struct MessageStreamTraits<std::string>
{   static void serdes(MessageStreamBase& stream_bas, std::string& data_ref)
    {   stream_bas.serdesStringImpl(data_ref);
    }
};
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline MessageStreamBase& MessageStreamBase::operator || ( T& inp )
{
    MessageStreamTraits<T>::serdes(*this,inp);
    //this->serdes(inp);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

//! Message Stream outgoing stream (write into NetworkMessage )
struct MessageOutStream : public MessageStreamBase // into netmessage
{
    void serdesFixedStringImpl( ork::FixedStringBase& str ) override
    {
        mMessage.writeString(str.c_str());
    }
    void serdesStringImpl( std::string& str ) override
    {
        mMessage.writeString(str.c_str());
    }
    void serdesImpl( void* pdata, size_t ilen ) override
    {
        mMessage.writeDataInternal(pdata,ilen);
    }
    template <typename T> void writeItem( T& item )
    {
        item.serdes(*this);
    }
    bool isOutputStream() const override { return true; }

};

//! Message Stream outgoing stream (read from NetworkMessage )
struct MessageInpStream : public MessageStreamBase // outof netmessage
{
    MessageInpStream() : mIterator(mMessage) {}

    void serdesFixedStringImpl( ork::FixedStringBase& str ) override
    {
        // todo make me more efficient!!!
        std::string inp = mMessage.readString(mIterator);
        str.set(inp.c_str());
    }
    void serdesStringImpl( std::string& str ) override
    {
        str = mMessage.readString(mIterator);
    }
    void serdesImpl( void* pdata, size_t ilen ) override
    {
        mMessage.readDataInternal(pdata,ilen,mIterator);
    }

    template <typename T> void readItem( T& item )
    {
        item.serdes(*this);
    }
    bool isOutputStream() const override { return false; }

    NetworkMessageIterator  mIterator;


};


} // ork
