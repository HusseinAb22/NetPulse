//
// Created by Hussein Abbas on 30/04/2026.
//

#ifndef NETPULSE_MESSAGE_H
#define NETPULSE_MESSAGE_H
#include <string>
struct Message {
  int sender_fd;
  std::string text;
};
#endif //NETPULSE_MESSAGE_H
