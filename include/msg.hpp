#ifndef __MSG_HPP
#define __MSG_HPP

#include "slave.hpp"
#include "msg_type.hpp"
#include <thread>
#include <mutex>
#include <sstream>

#define MSG_BUFFER_SIZE 1024

void subtask_input_update(int root_id, int subtask_id, std::string fname);

void msg_send();

void msg_recv();

void peerS_msg_send(PeerNode *peer);

void peerS_msg_recv(PeerNode *peer);

void peerC_msg_send(PeerNode *peer);

void peerC_msg_recv(PeerNode *peer);

#endif //__MSG_HPP
