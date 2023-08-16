#ifndef __MSG_HPP
#define __MSG_HPP

#include "slave.hpp"
#include "msg_type.hpp"
#include "file.hpp"
#include <thread>

#define MSG_BUFFER_SIZE 1024

void msg_send();
void msg_recv();


#endif //__MSG_HPP
