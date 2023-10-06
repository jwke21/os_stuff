#ifndef OS_THREADING_THREADINGCONSTANTS_H
#define OS_THREADING_THREADINGCONSTANTS_H

#include <time.h>
#include <boost/chrono/duration.hpp>

using namespace boost;

namespace Threading {
static const int MICROSECONDS_TICK = 50000;
static const chrono::duration<boost::int_least64_t, milli> THREAD_TIMEOUT = chrono::milliseconds(50);
static const int PAUSE_THREAD_POOL_SIZE = 10;
}  // namespace Threading
#endif  // OS_THREADING_THREADINGCONSTANTS_H
