/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iwakeup.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IWAKEUP_H
#define IWAKEUP_H

#include <core/kernel/ipoll.h>

namespace ishell {

class iWakeup
{
public:
    iWakeup();
    ~iWakeup();

    void getPollfd(iPollFD* fd) const;

    void signal();
    void acknowledge();

private:
    intptr_t m_fds[2];
};

} // namespace ishell

#endif // IWAKEUP_H
