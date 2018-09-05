#ifndef MOSQUITTO_PRELUDE_H
#define MOSQUITTO_PRELUDE_H

#ifndef RFLOAT_VALUE
#define RFLOAT_VALUE(v) (RFLOAT(v)->value)
#endif

#ifdef RUBINIUS
#include "rubinius.h"
#else
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
#include "ruby19.h"
#else
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
#include "ruby2.h"
#else
#include "ruby18.h"
#endif
#endif
#endif

#endif
