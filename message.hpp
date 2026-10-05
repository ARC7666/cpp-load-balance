#ifndef __MESSAGE_H_
#define __MESSAGE_H_

#include <stdint.h>
#include <string.h>

#include "common.hpp"

#define JSON_MIN_LENGTH 8

/**
 * Load Balancer Protocol
 * 
 * Data Length(4byte)
 * Data(variable length byte)
*/

class Message
{
    public:
        length_t length;
        json msg;
        byte_t *query = nullptr;

        Message(byte_t *_query);
        int Parse(int len);
        ~Message();
};

#endif 