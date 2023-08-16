#ifndef __TASK_HPP
#define __TASK_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <thread>
#include <mutex>
#include <jsoncpp/json/json.h>
#include "master.hpp"
#include "list.hpp"

bool task_add(std::string path);

void task_deploy();

#endif //__TASK_HPP