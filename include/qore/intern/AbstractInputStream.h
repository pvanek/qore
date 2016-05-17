/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AbstractInputStream.h

  Qore Programming Language

  Copyright (C) 2003 - 2016 David Nichols

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QORE_ABSTRACTINPUTSTREAM_H
#define _QORE_ABSTRACTINPUTSTREAM_H

#include "qore/AbstractPrivateData.h"

DLLEXPORT extern QoreClass* QC_ABSTRACTINPUTSTREAM;

/**
 * \brief Base class for private data of input stream implementations.
 */
class AbstractInputStream : public AbstractPrivateData {

public:
   /**
    * \brief Constructor.
    * \param self the QoreObject associated to this private data
    */
   AbstractInputStream(QoreObject *self) : self(self) {
   }

   /**
    * \brief Reads a single byte from the input stream.
    *
    * Default implementation invokes the Qore version of this method on the actual QoreObject. This is intended to
    * happen only for classes implementing the AbstractInputStream in the Qore language. C++ implementations should
    * override this method and read the byte directly from the source.
    * \param xsink the exception sink
    * \return the byte (0-255) read or -1 if the end of the stream has been reached
    */
   virtual int64 read(ExceptionSink* xsink) {
      return self->evalMethodValue("read", 0, xsink).getAsBigInt();
   }

protected:
   QoreObject *self;                    //!< The QoreObject associated to this private data
};

#endif // _QORE_ABSTRACTINPUTSTREAM_H
