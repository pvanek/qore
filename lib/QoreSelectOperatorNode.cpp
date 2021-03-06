/*
  QoreSelectOperatorNode.cpp

  Qore Programming Language

  Copyright (C) 2003 - 2016 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>

#include "qore/intern/qore_program_private.h"
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

#include <memory>

QoreString QoreSelectOperatorNode::select_str("select operator expression");

// if del is true, then the returned QoreString * should be selectd, if false, then it must not be
QoreString *QoreSelectOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = false;
   return &select_str;
}

int QoreSelectOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
   str.concat(&select_str);
   return 0;
}

AbstractQoreNode* QoreSelectOperatorNode::parseInitImpl(LocalVar* oflag, int pflag, int& lvids, const QoreTypeInfo*& typeInfo) {
   assert(!typeInfo);

   pflag &= ~PF_RETURN_VALUE_IGNORED;

   // check iterator expression
   const QoreTypeInfo* iteratorTypeInfo = 0;
   left = left->parseInit(oflag, pflag, lvids, iteratorTypeInfo);

   // check filter expression
   const QoreTypeInfo* expTypeInfo = 0;
   right = right->parseInit(oflag, pflag, lvids, expTypeInfo);

   // use lazy evaluation if the iterator expression supports it
   iterator_func = dynamic_cast<FunctionalOperator*>(left);

   // if iterator is a list or an iterator, then the return type is a list, otherwise it's the return type of the iterated expression
   if (iteratorTypeInfo->hasType()) {
      if (iteratorTypeInfo->isType(NT_NOTHING)) {
	 qore_program_private::makeParseWarning(getProgram(), QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", "the iterator expression with the select operator (the first expression) has no value (NOTHING) and therefore this expression will also return no value; update the expression to return a value or use '%%disable-warning invalid-operation' in your code to avoid seeing this warning in the future");
	 typeInfo = nothingTypeInfo;
      }
      else if (iteratorTypeInfo->isType(NT_LIST)) {
	 typeInfo = listTypeInfo;
      }
      else {
	 const QoreClass* qc = iteratorTypeInfo->getUniqueReturnClass();
	 if (qc && qore_class_private::parseCheckCompatibleClass(*qc, *QC_ABSTRACTITERATOR))
	    typeInfo = listTypeInfo;
	 else if ((iteratorTypeInfo->parseReturnsType(NT_LIST) == QTI_NOT_EQUAL)
		  && (iteratorTypeInfo->parseReturnsClass(QC_ABSTRACTITERATOR) == QTI_NOT_EQUAL))
	    typeInfo = iteratorTypeInfo;
      }
   }

   return this;
}

QoreValue QoreSelectOperatorNode::evalValueImpl(bool& needs_deref, ExceptionSink* xsink) const {
   FunctionalValueType value_type;
   std::unique_ptr<FunctionalOperatorInterface> f(getFunctionalIterator(value_type, xsink));
   if (*xsink || value_type == nothing)
      return QoreValue();

   ReferenceHolder<QoreListNode> rv(ref_rv && (value_type != single) ? new QoreListNode : 0, xsink);

   while (true) {
      ValueOptionalRefHolder iv(xsink);
      if (f->getNext(iv, xsink))
	 break;

      if (*xsink)
	 return QoreValue();

      if (value_type == single)
	 return ref_rv ? iv.takeReferencedValue() : QoreValue();

      if (ref_rv)
	 rv->push(iv.getReferencedValue());
   }

   return rv.release();
}

FunctionalOperatorInterface* QoreSelectOperatorNode::getFunctionalIteratorImpl(FunctionalValueType& value_type, ExceptionSink* xsink) const {
   if (iterator_func) {
      std::unique_ptr<FunctionalOperatorInterface> f(iterator_func->getFunctionalIterator(value_type, xsink));
      if (*xsink || value_type == nothing)
	 return 0;
      return new QoreFunctionalSelectOperator(this, f.release());
   }

   ValueEvalRefHolder marg(left, xsink);
   if (*xsink)
      return 0;

   qore_type_t t = marg->getType();
   if (t != NT_LIST) {
      if (t == NT_OBJECT) {
	 AbstractIteratorHelper h(xsink, "select operator", const_cast<QoreObject*>(marg->get<const QoreObject>()));
	 if (*xsink)
	    return 0;
	 if (h) {
	    bool temp = marg.isTemp();
	    marg.clearTemp();
	    value_type = list;
	    return new QoreFunctionalSelectIteratorOperator(this, temp, h, xsink);
	 }
      }
      if (t == NT_NOTHING) {
	 value_type = nothing;
	 return 0;
      }

      value_type = single;
      return new QoreFunctionalSelectSingleValueOperator(this, marg.getReferencedValue(), xsink);
   }

   value_type = list;
   return new QoreFunctionalSelectListOperator(this, marg.takeReferencedNode<QoreListNode>(), xsink);
}

bool QoreFunctionalSelectListOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
   while (true) {
      if (!next())
	 return true;

      // set offset in thread-local data for "$#"
      ImplicitElementHelper eh(index());
      SingleArgvContextHelper argv_helper(getReferencedValue(), xsink);

      // check if value can be selected
      ValueEvalRefHolder result(select->right, xsink);
      if (*xsink)
         return false;
      if (!result->getAsBool())
         continue;

      val.setValue(getReferencedValue(), true);
      break;
   }
   return false;
}

bool QoreFunctionalSelectSingleValueOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
   if (done)
      return true;

   done = true;

   // setup the implicit argument
   SingleArgvContextHelper argv_helper(v.refSelf(), xsink);

   // check if value can be selected
   ValueEvalRefHolder result(select->right, xsink);
   if (*xsink)
      return false;
   if (!result->getAsBool())
      return true;

   val.setValue(v, true);
   v.clear();
   return false;
}

bool QoreFunctionalSelectIteratorOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
   while (true) {
      bool b = h.next(xsink);
      if (!b)
	 return true;
      if (*xsink)
	 return false;

      // set offset in thread-local data for "$#"
      ImplicitElementHelper eh(index++);

      // get the current value
      ValueHolder iv(h.getValue(xsink), xsink);
      if (*xsink)
	 return false;
      // setup the implicit argument
      SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
      // check if value can be selected
      ValueEvalRefHolder result(select->right, xsink);
      if (*xsink)
         return false;
      if (!result->getAsBool())
         continue;

      val.setValue(iv.release(), true);
      break;
   }
   return false;
}

bool QoreFunctionalSelectOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
   while (true) {
      ValueOptionalRefHolder iv(xsink);
      if (f->getNext(iv, xsink))
	 return true;
      if (*xsink)
	 return false;

      // set offset in thread-local data for "$#"
      ImplicitElementHelper eh(index++);

      // setup the implicit argument
      SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
      // check if value can be selected
      ValueEvalRefHolder result(select->right, xsink);
      if (*xsink)
         return false;
      if (!result->getAsBool())
         continue;

      iv.ensureReferencedValue();
      val.takeValueFrom(iv);
      break;
   }
   return false;
}
