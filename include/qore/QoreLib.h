/*
  QoreLib.h

  Qore Programming Language

  Copyright 2003 - 2008 David Nichols

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_QORELIB_H

#define _QORE_QORELIB_H

#include <qore/common.h>
#include <qore/QoreThreadLock.h>
#include <qore/qore_bitopts.h>
#include <qore/safe_dslist>

#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/** @file QoreLib.h
    contains prototypes for various helper functions in the Qore library
 */

//! function to try and make a class name out of a file path, returns a new string that must be free()ed
DLLEXPORT char *make_class_name(const char *fn);

//! a string formatting function that works with Qore data structures
DLLEXPORT class QoreStringNode *q_sprintf(const class QoreListNode *params, int field, int offset, class ExceptionSink *xsink);

//! a string formatting function that works with Qore data structures
DLLEXPORT class QoreStringNode *q_vsprintf(const class QoreListNode *params, int field, int offset, class ExceptionSink *xsink);

//! thread-safe version of "localtime()"
DLLEXPORT struct tm *q_localtime(const time_t *clock, struct tm *tms);

//! thread-safe version of "gmtime()"
DLLEXPORT struct tm *q_gmtime(const time_t *clock, struct tm *tms);

//! thread-safe basename function (resulting pointer must be free()ed)
DLLEXPORT char *q_basename(const char *path);

//! returns a pointer within the same string
DLLEXPORT char *q_basenameptr(const char *path);

//! thread-safe dirname function (resulting pointer must be free()ed)
DLLEXPORT char *q_dirname(const char *path);

//! frees memory if there is an allocation error
DLLEXPORT void *q_realloc(void *ptr, size_t size);

//! sets up the Qore ARGV and QORE_ARGV values
DLLEXPORT void qore_setup_argv(int pos, int argc, char *argv[]);

//! STL-like list containing all presently-loaded Qore features
/** this list must be thread-safe for reading, writing under a lock
 */
class FeatureList : public safe_dslist<std::string>
{
   private:
      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL FeatureList(const FeatureList&);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL FeatureList& operator=(const FeatureList&);

   public:
      //! initialized by the library, constructor not exported
      DLLLOCAL FeatureList();

      //! destructor not exported
      DLLLOCAL ~FeatureList();
};

//! list of qore features
DLLEXPORT extern FeatureList qoreFeatureList;

//! find one of any characters in a string
static inline char *strchrs(const char *str, const char *chars)
{
   while (*str)
   {
      if (strchr(chars, *str))
	 return (char *)str;
      str++;
   }
   return 0;
}

//! find a character in a string up to len
static inline char *strnchr(const char *str, int len, char c)
{
   int i = 0;
   while (i++ != len)
   {
      if (*str == c)
	 return (char *)str;
      ++str;
   }
   return 0;
}

//! convert a string to lower-case in place
static inline void strtolower(char *str)
{
   while (*(str))
   {
      (*str) = tolower(*str);
      str++;
   }
}

//! convert a string to upper-case in place
static inline char *strtoupper(char *str)
{
   char *p = str;
   while (*(p))
   {
      *p = toupper(*p);
      p++;
   }
   return str;
}

//! for getting an integer number of seconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getSecZeroInt(const AbstractQoreNode *a);

//! for getting an integer number of seconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getSecZeroBigInt(const AbstractQoreNode *a);

//! for getting an integer number of seconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int getSecMinusOneInt(const AbstractQoreNode *a);

//! for getting an integer number of seconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getSecMinusOneBigInt(const AbstractQoreNode *a);

//! for getting an integer number of milliseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getMsZeroInt(const AbstractQoreNode *a);

//! for getting an integer number of milliseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getMsZeroBigInt(const AbstractQoreNode *a);

//! for getting an integer number of milliseconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int getMsMinusOneInt(const AbstractQoreNode *a);

//! for getting an integer number of milliseconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getMsMinusOneBigInt(const AbstractQoreNode *a);

//! for getting an integer number of microseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getMicroSecZeroInt(const AbstractQoreNode *a);

//! to check if an AbstractQoreNode object is NOTHING
static inline bool is_nothing(const AbstractQoreNode *n)
{
   if (!n || n->getType() == NT_NOTHING)
      return true;
   
   return false;
}

//! to deref an AbstractQoreNode (when the pointer may be 0)
static inline void discard(AbstractQoreNode *n, ExceptionSink *xsink)
{
   if (n)
      n->deref(xsink);
}

static inline const char *get_type_name(AbstractQoreNode *n)
{
   return n ? n->getTypeName() : "NOTHING";
}

class BinaryNode;
class QoreStringNode;
class ExceptionSink;

//! compresses data with the DEFLATE algorithm
DLLEXPORT BinaryNode     *qore_deflate(void *ptr, unsigned long len, int level, ExceptionSink *xsink);
//! decompresses data compressed with the DEFLATE algorithm to a string
DLLEXPORT QoreStringNode *qore_inflate_to_string(const BinaryNode *b, const QoreEncoding *enc, ExceptionSink *xsink);
//! decompresses data compressed with the DEFLATE algorithm to a binary
DLLEXPORT BinaryNode     *qore_inflate_to_binary(const BinaryNode *b, ExceptionSink *xsink);
//! gzips data
DLLEXPORT BinaryNode     *qore_gzip(void *ptr, unsigned long len, int level, ExceptionSink *xsink);
//! gunzips compressed data to a string
DLLEXPORT QoreStringNode *qore_gunzip_to_string(const BinaryNode *bin, const QoreEncoding *enc, ExceptionSink *xsink);
//! gunzips compressed data to a binary
DLLEXPORT BinaryNode     *qore_gunzip_to_binary(const BinaryNode *bin, ExceptionSink *xsink);
//! compresses data with bzip2
DLLEXPORT BinaryNode     *qore_bzip2(void *ptr, unsigned long len, int level, ExceptionSink *xsink);
//! decompresses bzip2 data to a string
DLLEXPORT QoreStringNode *qore_bunzip2_to_string(const BinaryNode *bin, const QoreEncoding *enc, ExceptionSink *xsink);
//! decompresses bzip2 data to a binary
DLLEXPORT BinaryNode     *qore_bunzip2_to_binary(const BinaryNode *bin, ExceptionSink *xsink);


#endif // _QORE_QORELIB_H
