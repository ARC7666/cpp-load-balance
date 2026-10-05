/**
 * Message Parsing Utility
 * Helper functions for parsing Msgpack payloads from the control channel.
 */
#include "message.hpp"

#include <iostream>

Message::Message(byte_t *_query)
{
    query = _query;
}

Message::~Message()
{
    delete []query;
}

/**
*/
int Message::Parse(int len)
{
    std::vector<byte_t> data(query, query+len);
    try
    {
        msg = json::from_msgpack(data); 
        return C_OK;
    }
    catch(const std::exception& e)
    {
        //std::cout<<e.what()<<std::endl;
        return C_ERR;
    }
}