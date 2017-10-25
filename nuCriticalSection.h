//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __CRITICALSECTION_H__
#define __CRITICALSECTION_H__


/* -------------------------------------------------------------------------- */

#include <pthread.h>
#include <memory.h>


/* -------------------------------------------------------------------------- */

#include "nuAssert.h"
#include "nuTrace.h"
#include "nuTerminal.h"

#include <string>


/* -------------------------------------------------------------------------- */

namespace nu
{

    class critical_section
    {
        private:
            int _timeout;
            int _counter;

            int create_success_;
            mutable pthread_mutex_t muid_;
            std::string name_;

        public:
            enum {
                MU_RECURSIVE, MU_NORECURSIVE
            };

            critical_section( 
                    const char* name,
                    unsigned long flags = MU_RECURSIVE,
                    int timeout = 0) 
            :
                _timeout(timeout),
                _counter(0)
            {
                NU_ASSERT(name);
                name_ = name;

                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr,
                        flags != MU_RECURSIVE ? 
                        PTHREAD_MUTEX_FAST_NP : 
                        PTHREAD_MUTEX_RECURSIVE_NP);

                unsigned long result = pthread_mutex_init(&muid_, &attr);

                pthread_mutexattr_destroy(&attr);
                create_success_ = (result == 0);
            }

            bool create_success() const throw() {
                return create_success_;
            }

            ~critical_section() {
                if (create_success_) 
                    pthread_mutex_destroy(&muid_);
            }

            bool enter() throw() {
                if (!create_success())  
                    return false;

                unsigned long result = pthread_mutex_lock(&muid_);
                return (result == 0);
            }

            bool try_enter() throw() {
                if (!create_success()) 
                    return false;

                unsigned long result = pthread_mutex_trylock(&muid_);
                return (result == 0);
            }

            bool leave() throw() {
                if (!create_success()) 
                    return false;

                unsigned long result = pthread_mutex_unlock(&muid_);
                return (result == 0);
            }

            const char* name() const throw() {
                return name_.c_str();
            }

            pthread_mutex_t* get_mutex_handle() const {
                return &muid_;
            }

    };


/* -------------------------------------------------------------------------- */

    class autoCs_t {
        private:
            nu::critical_section* _cs;

        public:
            autoCs_t(nu::critical_section& cs) : _cs(&cs) {
                cs.enter();
            }

            ~autoCs_t() {
                _cs->leave();
            }
    };

}


/* -------------------------------------------------------------------------- */

#endif

