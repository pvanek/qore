/*
  Operator.cc

  Qore Programming Language

  Copyright (C) 2003, 2004, 2005, 2006, 2007 David Nichols

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

#include <qore/Qore.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pcre.h>

DLLLOCAL class OperatorList oplist;

// the standard, system-default operator pointers
class Operator *OP_ASSIGNMENT, *OP_MODULA, 
   *OP_BIN_AND, *OP_BIN_OR, *OP_BIN_NOT, *OP_BIN_XOR, *OP_MINUS, *OP_PLUS, 
   *OP_MULT, *OP_DIV, *OP_UNARY_MINUS, *OP_SHIFT_LEFT, *OP_SHIFT_RIGHT, 
   *OP_POST_INCREMENT, *OP_POST_DECREMENT, *OP_PRE_INCREMENT, *OP_PRE_DECREMENT, 
   *OP_LOG_CMP, *OP_PLUS_EQUALS, *OP_MINUS_EQUALS, *OP_AND_EQUALS, *OP_OR_EQUALS, 
   *OP_LIST_REF, *OP_OBJECT_REF, *OP_ELEMENTS, *OP_KEYS, *OP_QUESTION_MARK, 
   *OP_OBJECT_FUNC_REF, *OP_NEW, *OP_SHIFT, *OP_POP, *OP_PUSH,
   *OP_UNSHIFT, *OP_REGEX_SUBST, *OP_LIST_ASSIGNMENT, *OP_SPLICE, *OP_MODULA_EQUALS, 
   *OP_MULTIPLY_EQUALS, *OP_DIVIDE_EQUALS, *OP_XOR_EQUALS, *OP_SHIFT_LEFT_EQUALS, 
   *OP_SHIFT_RIGHT_EQUALS, *OP_REGEX_TRANS, *OP_REGEX_EXTRACT, 
   *OP_CHOMP, *OP_TRIM, *OP_LOG_AND, *OP_LOG_OR, *OP_LOG_LT, 
   *OP_LOG_GT, *OP_LOG_EQ, *OP_LOG_NE, *OP_LOG_LE, *OP_LOG_GE, *OP_NOT, 
   *OP_ABSOLUTE_EQ, *OP_ABSOLUTE_NE, *OP_REGEX_MATCH, *OP_REGEX_NMATCH,
   *OP_EXISTS, *OP_INSTANCEOF;

// call to get a node with reference count 1 (copy on write)
static inline void ensure_unique(class QoreNode **v, class ExceptionSink *xsink)
{
   if (!(*v)->is_unique())
   {
      QoreNode *old = *v;
      (*v) = old->realCopy();
      old->deref(xsink);
   }
}

// call to get a unique douvle node
static inline void ensure_unique_float(class QoreNode **v, class ExceptionSink *xsink)
{
   if ((*v)->type != NT_FLOAT)
   {
      double f = (*v)->getAsFloat();
      (*v)->deref(xsink);
      (*v) = new QoreFloatNode(f);
   }
   else
      ensure_unique(v, xsink);
}

// call to get a unique int64 node
static inline void ensure_unique_int(class QoreNode **v, class ExceptionSink *xsink)
{
   assert(*v);
   QoreBigIntNode *b = dynamic_cast<QoreBigIntNode *>(*v);
   if (!b)
   {
      int64 i = (*v)->getAsBigInt();
      (*v)->deref(xsink);
      (*v) = new QoreBigIntNode(i);
   }
   else
      ensure_unique(v, xsink);
}

// operator functions for builtin types
static bool op_log_lt_bigint(int64 left, int64 right)
{
   return left < right;
}

static bool op_log_gt_bigint(int64 left, int64 right)
{
   return left > right;
}

static bool op_log_eq_bigint(int64 left, int64 right)
{
   return left == right;
}

static bool op_log_eq_binary(QoreNode *left, QoreNode *right)
{
   assert(left->type == NT_BINARY || right->type == NT_BINARY);
   const BinaryNode *l = dynamic_cast<const BinaryNode *>(left);
   const BinaryNode *r = dynamic_cast<const BinaryNode *>(right);
   if (!l || !r)
      return false;
   return !l->compare(r);
}

static bool op_log_ne_binary(QoreNode *left, QoreNode *right)
{
   assert(left->type == NT_BINARY || right->type == NT_BINARY);
   const BinaryNode *l = dynamic_cast<const BinaryNode *>(left);
   const BinaryNode *r = dynamic_cast<const BinaryNode *>(right);
   if (!l || !r)
      return true;
   return l->compare(r);
}

static bool op_log_eq_boolean(bool left, bool right)
{
   return left == right;
}

static bool op_log_ne_boolean(bool left, bool right)
{
   return left != right;
}

static bool op_log_ne_bigint(int64 left, int64 right)
{
   return left != right;
}

static bool op_log_le_bigint(int64 left, int64 right)
{
   return left <= right;
}

static bool op_log_ge_bigint(int64 left, int64 right)
{
   return left >= right;
}

static bool op_log_eq_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return left->isEqual(right);
}

static bool op_log_gt_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return DateTime::compareDates(left, right) > 0;
}

static bool op_log_ge_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return DateTime::compareDates(left, right) >= 0;
}

static bool op_log_lt_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return DateTime::compareDates(left, right) < 0;
}

static bool op_log_le_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return DateTime::compareDates(left, right) <= 0;
}

static bool op_log_ne_date(const DateTimeNode *left, const DateTimeNode *right)
{
   return !left->isEqual(right);
}

static bool op_log_lt_float(double left, double right)
{
   return left < right;
}

static bool op_log_gt_float(double left, double right)
{
   return left > right;
}

static bool op_log_eq_float(double left, double right)
{
   return left == right;
}

static bool op_log_ne_float(double left, double right)
{
   return left != right;
}

static bool op_log_le_float(double left, double right)
{
   return left <= right;
}

static bool op_log_ge_float(double left, double right)
{
   return left >= right;
}

static bool op_log_eq_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return !left->compareSoft(right, xsink);
}

static bool op_log_gt_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return left->compare(right) > 0;
}

static bool op_log_ge_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return right->compare(left) >= 0;
}

static bool op_log_lt_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return left->compare(right) < 0;
}

static bool op_log_le_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return left->compare(right) <= 0;
}

static bool op_log_ne_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return left->compare(right);
}

static bool op_absolute_log_eq(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder lnp(left, xsink);
   if (*xsink)
      return false;

   QoreNodeEvalOptionalRefHolder rnp(right, xsink);
   if (*xsink)
      return false;

   if (is_nothing(*lnp))
      if (is_nothing(*rnp))
	 return true;
      else 
	 return false;
   
   if (is_nothing(*rnp))
      return false;

   return lnp->is_equal_hard(*rnp, xsink);
}

static bool op_absolute_log_neq(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   return !op_absolute_log_eq(left, right, xsink);
}

static bool op_regex_match(const QoreString *left, const QoreRegexNode *right, ExceptionSink *xsink)
{
   return right->exec(left, xsink);
}

static bool op_regex_nmatch(const QoreString *left, const QoreRegexNode *right, ExceptionSink *xsink)
{
   return !right->exec(left, xsink);
}

// takes all arguments unevaluated so logic short-circuiting can happen
static bool op_log_or(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   bool l = left->boolEval(xsink);   
   if (*xsink)
      return false;

   // if left side is true, then do not evaluate right side
   return l ? true : right->boolEval(xsink);
}

// "soft" comparison
static inline bool list_is_equal(const QoreListNode *l, const QoreListNode *r, ExceptionSink *xsink)
{
   if (l->size() != r->size())
      return false;
   for (int i = 0; i < l->size(); i++)
      if (compareSoft(l->retrieve_entry(i), r->retrieve_entry(i), xsink) || *xsink)
	 return false;
   return true;
}

static bool op_log_eq_list(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreListNode *l = dynamic_cast<QoreListNode *>(left);
   if (!l)
      return false;

   QoreListNode *r = dynamic_cast<QoreListNode *>(right);
   if (!r)
      return false;
   
   return l->is_equal_soft(r, xsink);
}

static bool op_log_eq_hash(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreHashNode *lh = dynamic_cast<QoreHashNode *>(left);
   if (!lh)
      return false;

   QoreHashNode *rh = dynamic_cast<QoreHashNode *>(right);
   if (!rh)
      return false;

   return !lh->compareSoft(rh, xsink);
}

static bool op_log_eq_object(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreObject *l = dynamic_cast<QoreObject *>(left);
   if (!l)
      return false;

   QoreObject *r = dynamic_cast<QoreObject *>(right);
   if (!r)
      return false;

   return !l->compareSoft(r, xsink);
}

static bool op_log_eq_nothing(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   assert(left->type == NT_NOTHING && right->type == NT_NOTHING);
   return true;
}

static bool op_log_eq_null(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   if (left && left->type == NT_NULL && right && right->type == NT_NULL)
      return true;
   return false;
}

static bool op_log_ne_list(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreListNode *l = dynamic_cast<QoreListNode *>(left);
   if (!l)
      return true;

   QoreListNode *r = dynamic_cast<QoreListNode *>(right);
   if (!r)
      return true;
   
   return !l->is_equal_soft(r, xsink);
}

static bool op_log_ne_hash(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{   
   QoreHashNode *lh = dynamic_cast<QoreHashNode *>(left);
   if (!lh)
      return true;

   QoreHashNode *rh = dynamic_cast<QoreHashNode *>(right);
   if (!rh)
      return true;

   return lh->compareSoft(rh, xsink);
}

static bool op_log_ne_object(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreObject *l = dynamic_cast<QoreObject *>(left);
   if (!l)
      return true;

   QoreObject *r = dynamic_cast<QoreObject *>(right);
   if (!r)
      return true;

   return l->compareSoft(r, xsink);
}

static bool op_log_ne_nothing(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   assert(left->type == NT_NOTHING && right->type == NT_NOTHING);
   return false;
}

static bool op_log_ne_null(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   if (left && left->type == NT_NULL && right && right->type == NT_NULL)
      return false;
   return true;
}

static bool op_exists(QoreNode *left, class QoreNode *x, ExceptionSink *xsink)
{
   if (is_nothing(left))
      return false;
   if (!left->needs_eval())
      return true;

   class QoreNode *tn = NULL;
   class AutoVLock vl;

   // FIXME: modify getExistingVarValue() to take a ReferenceHolder<QoreNode>
   class QoreNode *n = getExistingVarValue(left, xsink, &vl, &tn);
   // return if an exception happened
   if (xsink->isEvent())
   {
      vl.del();
      if (tn) tn->deref(xsink);
      //traceout("op_exists()");
      return false;
   }

   bool b;
   // FIXME: this should return false for objects that have been deleted
   if (is_nothing(n))
      b = false;
   else
      b = true;
   if (tn) tn->deref(xsink);
   return b;
}

static bool op_instanceof(class QoreNode *l, class QoreNode *r, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder nl(l, xsink);
   if (*xsink || !nl)
      return false;

   assert(r && r->getType() == NT_CLASSREF);
   QoreObject *o = dynamic_cast<QoreObject *>(*nl);
   if (o && o->validInstanceOf(reinterpret_cast<const ClassRefNode *>(r)->getID()))
      return true;

   return false;  
}

// takes all arguments unevaluated so logic short-circuiting can happen
static bool op_log_and(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   // if left side is 0, then do not evaluate right side
   bool l = left->boolEval(xsink);
   if (xsink->isEvent())
      return false;
   bool b;
   if (!l)
      b = false;
   else
      b = right->boolEval(xsink);
   
   return b;
}

static int64 op_cmp_bigint(int64 left, int64 right)
{
   return left - right;
}

static int64 op_minus_bigint(int64 left, int64 right)
{
   return left - right;
}

static int64 op_plus_bigint(int64 left, int64 right)
{
   return left + right;
}

static int64 op_multiply_bigint(int64 left, int64 right)
{
   return left * right;
}

static int64 op_divide_bigint(int64 left, int64 right, ExceptionSink *xsink)
{
   if (!right)
   {
      xsink->raiseException("DIVISION-BY-ZERO", "division by zero in integer expression");
      return 0;
   }
   return left / right;
}

static DateTimeNode *op_minus_date(const DateTimeNode *left, const DateTimeNode *right)
{
    return left->subtractBy(right);
}

static DateTimeNode *op_plus_date(const DateTimeNode *left, const DateTimeNode *right)
{
    return left->add(right);
}

static double op_minus_float(double left, double right)
{
   return left - right;
}

static double op_plus_float(double left, double right)
{
   return left + right;
}

static double op_multiply_float(double left, double right)
{
   return left * right;
}

static double op_divide_float(double left, double right, ExceptionSink *xsink)
{
   if (!right)
   {
      xsink->raiseException("DIVISION-BY-ZERO", "division by zero in floating-point expression!");
      return 0.0;
   }
   return left / right;
}

static class QoreStringNode *op_plus_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   TempQoreStringNode str(new QoreStringNode(*left));
   //printd(5, "op_plus_string() (%d) %08p \"%s\" + (%d) %08p \"%s\"\n", left->strlen(), left->getBuffer(), left->getBuffer(), right->strlen(), right->getBuffer(), right->getBuffer());
   //printd(5, "op_plus_string() str= (%d) %08p \"%s\"\n", str->strlen(), str->getBuffer(), str->getBuffer());
   str->concat(right, xsink);
   if (*xsink)
      return 0;
   
   printd(5, "op_plus_string() result=\"%s\"\n", str->getBuffer());
   return str.release();
}

static int64 op_cmp_string(const QoreString *left, const QoreString *right, ExceptionSink *xsink)
{
   return (int64)left->compare(right);
}

static int64 op_elements(QoreNode *left, class QoreNode *null, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder np(left, xsink);
   if (*xsink || !np)
      return 0;

   {
      const QoreListNode *l = dynamic_cast<const QoreListNode *>(*np);
      if (l)
	 return l->size();
   }

   {
      const QoreObject *o = dynamic_cast<const QoreObject *>(*np);
      if (o)
	 return o->size(xsink);
   }

   {
      const QoreHashNode *h = dynamic_cast<const QoreHashNode *>(*np);
      if (h)
	 return h->size();	 
   }

   {
      const BinaryNode *b = dynamic_cast<const BinaryNode *>(*np);
      if (b)
	 return b->size();
   }

   {
      const QoreStringNode *str = dynamic_cast<const QoreStringNode *>(*np);
      if (str)
	 return str->length();
   }

   return 0;
}

static QoreListNode *get_keys(QoreNode *p, ExceptionSink *xsink)
{   
   if (!p)
      return 0;

   {
      const QoreHashNode *h = dynamic_cast<const QoreHashNode *>(p);
      if (h)
	 return h->getKeys();
   }

   {
      const QoreObject *o = dynamic_cast<const QoreObject *>(p);
      if (p)
	 return o->getMemberList(xsink);
   }

   return 0;
}

// FIXME: do not need ref_rv here - also do not need second argument
static class QoreNode *op_keys(QoreNode *left, QoreNode *null, bool ref_rv, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder np(left, xsink);
   if (*xsink)
      return 0;

   return get_keys(*np, xsink);
}

// FIXME: do not need ref_rv here
static class QoreNode *op_question_mark(QoreNode *left, QoreNode *list, bool ref_rv, ExceptionSink *xsink)
{
   assert(list && list->type == NT_LIST);
   bool b = left->boolEval(xsink);
   if (xsink->isEvent())
      return NULL;

   QoreListNode *l = reinterpret_cast<QoreListNode *>(list);

   if (b)
      return l->retrieve_entry(0)->eval(xsink);
   return l->retrieve_entry(1)->eval(xsink);
}

static class QoreNode *op_regex_subst(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   // get current value and save
   class AutoVLock vl;
   class QoreNode **v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   // if it's not a string, then do nothing
   if (!(*v) || (*v)->type != NT_STRING)
      return NULL;

   QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
   assert(right && right->getType() == NT_REGEX_SUBST);
   RegexSubstNode *rs = reinterpret_cast<RegexSubstNode *>(right);
   QoreStringNode *nv = rs->exec((*vs), xsink);
   if (xsink->isEvent())
      return NULL;

   // assign new value to lvalue
   (*vs)->deref();
   (*v) = nv;

   // reference for return value if necessary
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_regex_trans(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   // get current value and save
   class AutoVLock vl;
   class QoreNode **v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   // if it's not a string, then do nothing
   if (!(*v) || (*v)->type != NT_STRING)
      return NULL;

   assert(right && right->getType() == NT_REGEX_TRANS);
   QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
   QoreStringNode *nv = reinterpret_cast<const RegexTransNode *>(right)->exec((*vs), xsink);
   if (*xsink)
      return NULL;

   // assign new value to lvalue
   (*vs)->deref();
   (*v) = nv;
   // reference for return value
   (*v)->ref();
   return (*v);      
}

static class QoreNode *op_list_ref(QoreNode *left, QoreNode *index, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder lp(left, xsink);

   // return NULL if left side is not a list or string (or exception)
   if (!lp || *xsink || (lp->type != NT_LIST && lp->type != NT_STRING))
      return 0;

   class QoreNode *rv = 0;
   int ind = index->integerEval(xsink);
   if (!*xsink) {
      // get value
      if (lp->type == NT_LIST) {
	 QoreListNode *l = reinterpret_cast<QoreListNode *>(*lp);
	 rv = l->retrieve_entry(ind);
	 // reference for return
	 if (rv)
	    rv->ref();
      }
      else if (ind >= 0) {
	 QoreStringNode *lpstr = reinterpret_cast<QoreStringNode *>(*lp);
	 rv = lpstr->substr(ind, 1);
      }
      //printd(5, "op_list_ref() index=%d, rv=%08p\n", ind, rv);
   }
   return rv;
}

// for the member name, a string is required.  non-string arguments will
// be converted.  The null string can also be used
static class QoreNode *op_object_ref(QoreNode *left, class QoreNode *member, bool ref_rv, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder op(left, xsink);
   if (*xsink || !op)
      return 0;

   QoreHashNode *h = 0;
   QoreObject *o = 0;
   if (((!(h = dynamic_cast<QoreHashNode *>(*op))) && (!(o = dynamic_cast<QoreObject *>(*op)))))
      return NULL;

   // evaluate member expression
   QoreNodeEvalOptionalRefHolder mem(member, xsink);
   if (*xsink)
      return 0;

   QoreStringNodeValueHelper key(*mem);

   if (h)
      return h->evalKey(key->getBuffer(), xsink);

   return o->evalMember(*key, xsink);
}

static class QoreNode *op_object_method_call(QoreNode *left, class QoreNode *func, bool ref_rv, ExceptionSink *xsink)
{
   QoreNodeEvalOptionalRefHolder op(left, xsink);
   if (*xsink)
      return 0;

   assert(func && func->getType() == NT_FUNCTION_CALL);
   FunctionCallNode *f = reinterpret_cast<FunctionCallNode *>(func);

   {
      // FIXME: this is an ugly hack!
      QoreHashNode *h = dynamic_cast<QoreHashNode *>(*op);
      if (h) {
	 // see if the hash member is a call reference
	 QoreNode *c = h->getKeyValue(f->f.c_str);
	 if (c && c->type == NT_FUNCREF)
	    return c->val.funcref->exec(f->args, xsink);
      }
   }

   QoreObject *o = dynamic_cast<QoreObject *>(*op);
   if (!o)
   {
      //printd(5, "op=%08p (%s) func=%08p (%s)\n", op, op ? op->getTypeName() : "n/a", func, func ? func->getTypeName() : "n/a");
      xsink->raiseException("OBJECT-METHOD-EVAL-ON-NON-OBJECT", "member function \"%s\" called on type \"%s\"", 
			    f->f.c_str, op ? op->getTypeName() : "NOTHING" );
      return 0;
   }

   return o->getClass()->evalMethod(o, f->f.c_str, f->args, xsink);
}

static class QoreNode *op_new_object(QoreNode *left, class QoreNode *x, bool ref_rv, ExceptionSink *xsink)
{
   tracein("op_new_object()");
   
   assert(left->getType() == NT_SCOPE_REF);
   ScopedObjectCallNode *c = reinterpret_cast<ScopedObjectCallNode *>(left);
   QoreNode *rv = c->oc->execConstructor(c->args, xsink);
   printd(5, "op_new_object() returning node=%08p (type=%s)\n", rv, c->oc->getName());
   // if there's an exception, the constructor will delete the object without the destructor
   traceout("op_new_object()");
   return rv;
}

static class QoreNode *op_assignment(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   // tracein("op_assignment()");

   /* assign new value, this value gets referenced with the
      eval(xsink) call, so there's no need to reference it again
      for the variable assignment - however it does need to be
      copied/referenced for the return value
   */
   ReferenceHolder<QoreNode> new_value(right->eval(xsink), xsink);
   if (*xsink)
      return 0;

   // get current value and save
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   // dereference old value if necessary
   discard(*v, xsink);
   if (*xsink)
   {
      *v = 0;
      return 0;
   }

   // assign new value 
   (*v) = new_value.release();
   vl.del();

#if 0
   printd(5, "op_assignment() *%08p=%08p (type=%s refs=%d)\n",
	  v, new_value, 
	  new_value ? new_value->getTypeName() : "(null)",
	  new_value ? new_value->reference_count() : 0 );
#endif

   // traceout("op_assignment()");
   if (ref_rv && (*v))
      return (*v)->RefSelf();

   return NULL;
}

static class QoreNode *op_list_assignment(QoreNode *n_left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   assert(n_left && n_left->type == NT_LIST);
   QoreListNode *left = reinterpret_cast<QoreListNode *>(n_left);

   // tracein("op_assignment()");

   /* assign new value, this value gets referenced with the
      eval(xsink) call, so there's no need to reference it again
      for the variable assignment - however it does need to be
      copied/referenced for the return value
   */
   QoreNodeEvalOptionalRefHolder new_value(right, xsink);
   if (*xsink)
      return 0;

   // get values and save
   int i;
   for (i = 0; i < left->size(); i++)
   {
      class QoreNode *lv = left->retrieve_entry(i);

      class AutoVLock vl;
      v = get_var_value_ptr(lv, &vl, xsink);
      if (*xsink)
	 return 0;
      
      // dereference old value if necessary
      discard(*v, xsink);
      if (*xsink)
      {
	 *v = NULL;
	 return 0;
      }

      // if there's only one value, then save it
      QoreListNode *nv = dynamic_cast<QoreListNode *>(*new_value);
      if (nv) { // assign to list position
	 (*v) = nv->retrieve_entry(i);
	 if (*v)
	    (*v)->ref();
      }
      else {
	 if (!i)
	    (*v) = new_value.getReferencedValue();
	 else
	    (*v) = NULL;
      }
      vl.del();
   }

   // traceout("op_list_assignment()");
   return ref_rv ? new_value.takeReferencedValue() : 0;
}

static class QoreNode *op_plus_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   // tracein("op_plus_equals()");
   QoreNodeEvalOptionalRefHolder new_right(right, xsink);
   if (*xsink || !new_right)
      return 0;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return 0;
   
   // dereferences happen in each section so that the
   // already referenced value can be passed to list->push()
   // if necessary
   // do list plus-equals if left-hand side is a list
   const QoreType *vtype = *v ? (*v)->getType() : 0;
   if (vtype == NT_LIST)
   {
      ensure_unique(v, xsink);
      QoreListNode *l = reinterpret_cast<QoreListNode *>(*v);
      if (new_right->type == NT_LIST)
	 l->merge(reinterpret_cast<QoreListNode *>(*new_right));
      else
	 l->push(new_right.takeReferencedValue());
   }
   // do hash plus-equals if left side is a hash
   else if (vtype == NT_HASH)
   {
      if (new_right->type == NT_HASH)
      {
	 ensure_unique(v, xsink);
	 reinterpret_cast<QoreHashNode *>(*v)->merge(reinterpret_cast<QoreHashNode *>(*new_right), xsink);
      }
      else if (new_right->type == NT_OBJECT)
      {
	 ensure_unique(v, xsink);
	 reinterpret_cast<QoreObject *>(*new_right)->mergeDataToHash(reinterpret_cast<QoreHashNode *>(*v), xsink);
      }
   }
   // do hash/object plus-equals if left side is an object
   else if (vtype == NT_OBJECT)
   {
      QoreObject *o = reinterpret_cast<QoreObject *>(*v);
      // do not need ensure_unique() for objects
      if (new_right->type == NT_OBJECT)
      {
	 QoreHash *h = reinterpret_cast<QoreObject *>(*new_right)->copyData(xsink);
	 if (h)
	    o->assimilate(h, xsink);
      }
      else if (new_right->type == NT_HASH)
	 o->merge(reinterpret_cast<QoreHashNode *>(*new_right), xsink);
   }
   // do string plus-equals if left-hand side is a string
   else if (vtype == NT_STRING)
   {
      QoreStringValueHelper str(*new_right);

      ensure_unique(v, xsink);
      QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
      (*vs)->concat(*str, xsink);
   }
   else if (vtype == NT_FLOAT)
   {
      ensure_unique(v, xsink);
      QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
      (*vf)->f += new_right->getAsFloat();
   }
   else if (vtype == NT_DATE)
   {
      DateTimeValueHelper date(*new_right);

      DateTimeNode **vd = reinterpret_cast<DateTimeNode **>(v);
      DateTimeNode *nd = (*vd)->add(*date);
      (*v)->deref(xsink);
      (*v) = nd;
   }
   else if (!vtype || vtype == NT_NOTHING)
   {
      if (*v)
	 (*v)->deref(xsink); // exception not possible here
      // assign rhs to lhs (take reference for assignment)
      *v = new_right.takeReferencedValue();
   }
   else // do integer plus-equals
   {
      QoreBigIntNode *i;
      // get new value if necessary
      if (!(*v)) {
	 i = new QoreBigIntNode();
	 (*v) = i;
      }
      else
      {
	 ensure_unique_int(v, xsink);
	 if (*xsink)
	    return 0;
	 i = reinterpret_cast<QoreBigIntNode *>(*v);
      }

      // increment current value
      i->val += new_right->getAsBigInt();
   }

   // reference return value
   // traceout("op_plus_equals()");
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_minus_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   // tracein("op_minus_equals()");

   class QoreNode **v;

   QoreNodeEvalOptionalRefHolder new_right(right, xsink);
   if (*xsink || !new_right)
      return 0;

   // get ptr to current value

   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   // do float minus-equals if left side is a float
   const QoreType *vtype = *v ? (*v)->getType() : 0;
   if (vtype == NT_FLOAT)
   {
      ensure_unique(v, xsink);
      QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
      (*vf)->f -= new_right->getAsFloat();
   }
   else if (vtype == NT_DATE)
   {
      DateTimeValueHelper date(*new_right);

      ensure_unique(v, xsink);
      DateTimeNode **vd = reinterpret_cast<DateTimeNode **>(v);
      DateTimeNode *nd = (*vd)->subtractBy(*date);
      (*v)->deref(xsink);
      (*v) = nd;
   }
   else if (vtype == NT_HASH)
   {
      if (new_right->type == NT_HASH) {
	 // do nothing
      }
      else {
	 ensure_unique(v, xsink);
	 QoreHashNode *vh = reinterpret_cast<QoreHashNode *>(*v);

	 QoreListNode *nrl = dynamic_cast<QoreListNode *>(*new_right);
	 if (nrl && nrl->size()) {
	    // treat each element in the list as a string giving a key to delete
	    ListIterator li(nrl);
	    while (li.next()) {
	       QoreStringValueHelper val(li.getValue());
	       
	       vh->deleteKey(*val, xsink);
	       if (*xsink)
		  return 0;
	    }
	 }
	 else {
	    QoreStringValueHelper str(*new_right);
	    vh->deleteKey(*str, xsink);
	 }
      }
   }
   else // do integer minus-equals
   {
      if (new_right->type == NT_FLOAT)
      {
	 // we know the lhs type is not NT_FLOAT already
	 // get current float value and dereference node
	 double val;
	 if (*v) {
	    val = (*v)->getAsFloat();
	    (*v)->deref(xsink);
	 }
	 else
	    val = 0.0;

	 // assign negative argument
	 (*v) = new QoreFloatNode(val - new_right->getAsFloat());
      }
      else
      {
	 QoreBigIntNode *i;
	 // get new value if necessary
	 if (!(*v)) {
	    i = new QoreBigIntNode();
	    (*v) = i;
	 }
	 else
	 {
	    ensure_unique_int(v, xsink);
	    if (*xsink)
	       return 0;
	    i = reinterpret_cast<QoreBigIntNode *>(*v);
	 }

	 // increment current value
	 i->val -= new_right->getAsBigInt();	 
      }
   }

   // traceout("op_minus_equals()");
   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_and_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   //tracein("op_and_equals()");
   int64 val = right->bigIntEval(xsink);
   if (xsink->isEvent())
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;
   
   QoreBigIntNode *b;
   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode();
      (*v) = b;
   }
   else 
   {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;
      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // and current value with arg val
   b->val &= val;

   //traceout("op_and_equals()");

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_or_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   int64 val = right->bigIntEval(xsink);
   if (xsink->isEvent())
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   class QoreNode **v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode(0);
      (*v) = b;
   }
   else {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;
      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // or current value with arg val
   b->val |= val;

   //traceout("op_or_equals()");
   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_modula_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   int64 val = right->bigIntEval(xsink);
   if (xsink->isEvent())
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   class QoreNode **v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode(0);
      (*v) = b;
   }
   else {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;
      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // or current value with arg val
   b->val %= val;

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_multiply_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;
   //tracein("op_multiply_equals()");

   QoreNodeEvalOptionalRefHolder res(right, xsink);
   if (*xsink)
      return 0;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   // is either side a float?
   if (res && res->type == NT_FLOAT)
   {
      if (!(*v))
	 (*v) = new QoreFloatNode(0.0);
      else
      {
	 ensure_unique_float(v, xsink);
	 if (*xsink)
	    return 0;
	 
	 // multiply current value with arg val
	 QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
	 (*vf)->f *= (reinterpret_cast<QoreFloatNode *>(*res))->f;
      }
   }
   else if ((*v) && (*v)->type == NT_FLOAT)
   {
      if (res)
      {
	 ensure_unique(v, xsink);
	 QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
	 (*vf)->f *= res->getAsFloat();
      }
      else // if factor is NOTHING, assign 0.0
      {
	 (*v)->deref(xsink);
	 (*v) = new QoreFloatNode(0.0);
      }
   }
   else // do integer multiply equals
   {
      // get new value if necessary
      if (!(*v))
	 (*v) = new QoreBigIntNode();
      else 
      {
	 if (res)
	 {
	    ensure_unique_int(v, xsink);
	    if (*xsink)
	       return 0;

	    QoreBigIntNode *b = reinterpret_cast<QoreBigIntNode *>(*v);
	    
	    // multiply current value with arg val
	    b->val *= res->getAsBigInt();
	 }
	 else // if factor is NOTHING, assign 0
	 {
	    (*v)->deref(xsink);
	    (*v) = new QoreBigIntNode();
	 }
      }
   }

   //traceout("op_multiply_equals()");

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_divide_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;
   //tracein("op_divide_equals()");

   QoreNodeEvalOptionalRefHolder res(right, xsink);
   if (*xsink)
      return 0;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   // is either side a float?
   if (res && res->type == NT_FLOAT)
   {
      QoreFloatNode *rf = reinterpret_cast<QoreFloatNode *>(*res);

      if (!rf->f) {
	 xsink->raiseException("DIVISION-BY-ZERO", "division by zero in floating-point expression!");
	 return 0;
      }

      if (!(*v))
	 (*v) = new QoreFloatNode(0.0);
      else
      {
	 ensure_unique_float(v, xsink);

	 QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
	 // divide current value with arg val
	 (*vf)->f /= rf->f;
      }
   }
   else if ((*v) && (*v)->type == NT_FLOAT)
   {
      if (res)
      {
	 float val = res->getAsFloat();
	 if (!val) {
	    xsink->raiseException("DIVISION-BY-ZERO", "division by zero in floating-point expression!");
	    return 0;
	 }
	 else {
	    ensure_unique(v, xsink);

	    QoreFloatNode **vf = reinterpret_cast<QoreFloatNode **>(v);
	    (*vf)->f /= val;
	 }
      }
      else // if factor is NOTHING, raise exception
	 xsink->raiseException("DIVISION-BY-ZERO", "division by zero in floating-point expression!");
   }
   else // do integer divide equals
   {
      int64 val = res ? res->getAsBigInt() : 0;
      if (!val) {
	 xsink->raiseException("DIVISION-BY-ZERO", "division by zero in integer expression!");
	 return 0;
      }
      // get new value if necessary
      if (!(*v))
	 (*v) = new QoreBigIntNode(0);
      else 
      {
	 ensure_unique_int(v, xsink);
	 if (*xsink)
	    return 0;

	 QoreBigIntNode *b = reinterpret_cast<QoreBigIntNode *>(*v);
	 
	 // divide current value with arg val
	 b->val /= val;
      }
   }

   assert(*v);
   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_xor_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   int64 val = right->bigIntEval(xsink);
   if (*xsink)
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   class QoreNode **v = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   QoreBigIntNode *b;

   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode(0);
      (*v) = b;
   }
   else 
   {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;

      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // xor current value with arg val
   b->val ^= val;

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_shift_left_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   //tracein("op_shift_left_equals()");

   int64 val = right->bigIntEval(xsink);
   if (xsink->isEvent())
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode(0);
      (*v) = b;
   }
   else 
   {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;

      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // shift left current value by arg val
   b->val <<= val;

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;
}

static class QoreNode *op_shift_right_equals(QoreNode *left, QoreNode *right, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **v;

   //tracein("op_shift_right_equals()");

   int64 val = right->bigIntEval(xsink);
   if (xsink->isEvent())
      return NULL;

   // get ptr to current value
   class AutoVLock vl;
   v = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // get new value if necessary
   if (!(*v)) {
      b = new QoreBigIntNode();
      (*v) = b;
   }
   else 
   {
      ensure_unique_int(v, xsink);
      if (*xsink)
	 return 0;

      b = reinterpret_cast<QoreBigIntNode *>(*v);
   }

   // shift right current value by arg val
   b->val >>= val;

   // reference return value and return
   return ref_rv ? (*v)->RefSelf() : 0;

   //traceout("op_shift_right_equals()");
   return NULL;
}

// this is the default (highest-priority) function for the + operator, so any type could be sent here on either side
static QoreNode *op_plus_list(QoreNode *left, QoreNode *right)
{
   {
      QoreListNode *l = dynamic_cast<QoreListNode *>(left);
      if (l) {
	 QoreListNode *rv = l->copy();
	 QoreListNode *r = dynamic_cast<QoreListNode *>(right);
	 if (r)
	    rv->merge(r);
	 else
	    rv->push(right->RefSelf());
	 //printd(5, "op_plus_list() returning list=%08p size=%d\n", rv, rv->size());
	 return rv;
      }
   }

   QoreListNode *r = dynamic_cast<QoreListNode *>(right);
   if (!r)
      return 0;

   QoreListNode *rv = new QoreListNode();
   rv->push(left->RefSelf());
   rv->merge(r);
   return rv;
}

static class QoreNode *op_plus_hash_hash(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreHashNode *lh = dynamic_cast<QoreHashNode *>(left);
   if (lh) {
      QoreHashNode *rh = dynamic_cast<QoreHashNode *>(right);
      if (!rh)
	 return left->RefSelf();

      ReferenceHolder<QoreHashNode> rv(lh->copy(), xsink);
      rv->merge(rh, xsink);
      if (*xsink)
	 return 0;
      return rv.release();
   }

   return right->type == NT_HASH ? right->RefSelf() : 0;
}

static class QoreNode *op_plus_hash_object(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreHashNode *lh = dynamic_cast<QoreHashNode *>(left);
   if (lh) {
      QoreObject *r = dynamic_cast<QoreObject *>(right);
      if (r) {
	 ReferenceHolder<QoreHashNode> rv(lh->copy(), xsink);
	 r->mergeDataToHash(*rv, xsink);
	 if (*xsink)
	    return 0;

	 return rv.release();
      }
      return left->RefSelf();
   }
   QoreObject *r = dynamic_cast<QoreObject *>(right);
   return r ? right->RefSelf() : 0;
}

// note that this will return a hash
static class QoreNode *op_plus_object_hash(QoreNode *left, QoreNode *right, ExceptionSink *xsink)
{
   QoreObject *l = dynamic_cast<QoreObject *>(left);
   if (l) {
      QoreHashNode *rh = dynamic_cast<QoreHashNode *>(right);
      if (!rh)
	 return left->RefSelf();

      ReferenceHolder<QoreHashNode> h(l->copyDataNode(xsink), xsink);
      if (*xsink)
	 return 0;
      
      h->merge(rh, xsink);
      if (*xsink)
	 return 0;
      
      return h.release();
   }
   QoreHashNode *r = dynamic_cast<QoreHashNode *>(right);
   return r ? right->RefSelf() : 0;
}

static int64 op_cmp_double(double left, double right)
{
   if (left < right)
       return -1;
       
   if (left == right)
      return 0;
       
   return 1;
}

static int64 op_modula_int(int64 left, int64 right)
{
   return left % right;
}

static int64 op_bin_and_int(int64 left, int64 right)
{
   return left & right;
}

static int64 op_bin_or_int(int64 left, int64 right)
{
   return left | right;
}

static int64 op_bin_xor_int(int64 left, int64 right)
{
   return left ^ right;
}

static int64 op_shift_left_int(int64 left, int64 right)
{
   return left << right;
}

static int64 op_shift_right_int(int64 left, int64 right)
{
   return left >> right;
}

// variable assignment
static class QoreNode *op_post_inc(QoreNode *left, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **n, *rv;

   // tracein("op_post_inc()");
   class AutoVLock vl;
   n = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   // reference for return value is reference for variable assignment (if not null)
   rv = *n;

   // acquire new value
   QoreBigIntNode *b = new QoreBigIntNode((*n) ? (*n)->getAsBigInt() : 0);
   (*n) = b;

   // increment value
   b->val++;

   // traceout("op_post_inc");
   // return original value (may be null or non-integer)
   return rv;
}

// variable assignment
static class QoreNode *op_post_dec(QoreNode *left, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **n, *rv;
   // tracein("op_post_dec()");

   class AutoVLock vl;
   n = get_var_value_ptr(left, &vl, xsink);
   //printd(5, "op_post_dec() n=%08p, *n=%08p\n", n, *n);
   if (xsink->isEvent())
      return NULL;

   // reference for return value is reference for variable assignment (if not null)
   rv = *n;

   // acquire new value
   QoreBigIntNode *b = new QoreBigIntNode((*n) ? (*n)->getAsBigInt() : 0);
   (*n) = b;

   // decrement value
   b->val--;

   //printd(5, "op_post_dec(): n=%08p, *n=%08p\n", n, *n);
   // traceout("op_post_dec()");
   // return original value (may be null or non-integer)
   return rv;
}

// variable assignment
static class QoreNode *op_pre_inc(QoreNode *left, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **n;

   class AutoVLock vl;
   n = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // acquire new value if necessary
   if (!(*n)) {
      b = new QoreBigIntNode(0);
      (*n) = b;
   }
   else {
      ensure_unique_int(n, xsink);
      if (*xsink)
	 return 0;
      b = reinterpret_cast<QoreBigIntNode *>(*n);
   }

   // increment value
   b->val++;

   //printd(5, "op_pre_inc() ref_rv=%s\n", ref_rv ? "true" : "false");
   // reference for return value
   return ref_rv ? (*n)->RefSelf() : 0;
}

// variable assignment
static class QoreNode *op_pre_dec(QoreNode *left, bool ref_rv, ExceptionSink *xsink)
{
   class QoreNode **n;
   // tracein("op_pre_dec()");

   class AutoVLock vl;
   n = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   QoreBigIntNode *b;
   // acquire new value if necessary
   if (!(*n)) {
      b = new QoreBigIntNode();
      (*n) = b;
   }
   else {
      ensure_unique_int(n, xsink);
      if (*xsink)
	 return 0;
      b = reinterpret_cast<QoreBigIntNode *>(*n);
   }

   // decrement value
   b->val--;

   // traceout("op_pre_dec()");

   // reference return value
   return ref_rv ? (*n)->RefSelf() : 0;
}

// unshift lvalue, element
static QoreNode *op_unshift(QoreNode *left, class QoreNode *elem, bool ref_rv, ExceptionSink *xsink)
{
   printd(5, "op_unshift(%08p, %08p, isEvent=%d)\n", left, elem, xsink->isEvent());

   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;

   QoreListNode *l = dynamic_cast<QoreListNode *>(*val);
   // value is not a list, so throw exception
   if (!l)
   {
      xsink->raiseException("UNSHIFT-ERROR", "first argument to unshift is not a list");
      return 0;
   }

   if (!l->is_unique()) {
      l = l->copy();
      (*val)->deref(xsink);
      (*val) = l;
   }

   printd(5, "op_unshift() *val=%08p (%s)\n", *val, *val ? (*val)->getTypeName() : "(none)");
   printd(5, "op_unshift() about to call unshift() on list node %08p (%d) with element %08p\n", l, l->size(), elem);

   if (elem)
   {
      elem = elem->eval(xsink);
      if (*xsink)
      {
	 if (elem) elem->deref(xsink);
	 return NULL;
      }
   }

   l->insert(elem);

   // reference for return value
   if (ref_rv)
      return (*val)->RefSelf();

   return NULL;
}

static QoreNode *op_shift(QoreNode *left, class QoreNode *x, bool ref_rv, ExceptionSink *xsink)
{
   //tracein("op_shift()");
   printd(5, "op_shift(%08p, %08p, isEvent=%d)\n", left, x, xsink->isEvent());

   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;
   QoreListNode *l = dynamic_cast<QoreListNode *>(*val);
   if (!l)
      return NULL;

   if (!l->is_unique()) {
      l = l->copy();
      (*val)->deref(xsink);
      (*val) = l;
   }

   printd(5, "op_shift() *val=%08p (%s)\n", *val, *val ? (*val)->getTypeName() : "(none)");
   printd(5, "op_shift() about to call QoreListNode::shift() on list node %08p (%d)\n", l, l->size());

   // the list reference will now be the reference for return value
   // therefore no need to reference again
   return l->shift();
}

static QoreNode *op_pop(QoreNode *left, class QoreNode *x, bool ref_rv, ExceptionSink *xsink)
{
   printd(5, "op_pop(%08p, %08p, isEvent=%d)\n", left, x, xsink->isEvent());

   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;
   QoreListNode *l = dynamic_cast<QoreListNode *>(*val);
   if (!l)
      return NULL;

   if (!l->is_unique()) {
      l = l->copy();
      (*val)->deref(xsink);
      (*val) = l;
   }

   printd(5, "op_pop() *val=%08p (%s)\n", *val, *val ? (*val)->getTypeName() : "(none)");
   printd(5, "op_pop() about to call QoreListNode::pop() on list node %08p (%d)\n", (*val), l->size());

   // the list reference will now be the reference for return value
   // therefore no need to reference again
   QoreNode *rv = l->pop();

   printd(5, "op_pop() got node %08p (%s)\n", rv, rv ? rv->getTypeName() : "(none)");

   return rv;
}

static QoreNode *op_push(QoreNode *left, class QoreNode *elem, bool ref_rv, ExceptionSink *xsink)
{
   //tracein("op_push()");
   printd(5, "op_push(%08p, %08p, isEvent=%d)\n", left, elem, xsink->isEvent());

   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(left, &vl, xsink);
   if (*xsink)
      return 0;
   QoreListNode *l = dynamic_cast<QoreListNode *>(*val);
   if (!l) {
      xsink->raiseException("PUSH-ERROR", "first argument to push is not a list");
      return 0;
   }

   if (!l->is_unique()) {
      l = l->copy();
      (*val)->deref(xsink);
      (*val) = l;
   }

   ensure_unique(val, xsink);

   printd(5, "op_push() about to call push() on list node %08p (%d) with element %08p\n", (*val), l->size(), elem);

   if (elem)
   {
      elem = elem->eval(xsink);
      if (*xsink)
      {
	 if (elem) elem->deref(xsink);
	 return NULL;
      }
   }

   l->push(elem);

   // reference for return value
   return ref_rv ? l->RefSelf() : 0;
}

// lvalue, offset, [length, [list]]
static QoreNode *op_splice(QoreNode *left, class QoreNode *n_l, bool ref_rv, ExceptionSink *xsink)
{
   printd(5, "op_splice(%08p, %08p, isEvent=%d)\n", left, n_l, xsink->isEvent());

   assert(n_l->type == NT_LIST);
   QoreListNode *l = reinterpret_cast<QoreListNode *>(n_l);

   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(left, &vl, xsink);
   if (xsink->isEvent())
      return NULL;

   // if value is not a list or string, throw exception
   if (!(*val) || ((*val)->type != NT_LIST && (*val)->type != NT_STRING))
   {
      xsink->raiseException("SPLICE-ERROR", "first argument to splice is not a list or a string");
      return NULL;
   }
   
   // evaluate list
   QoreListNodeEvalOptionalRefHolder nl(l, xsink);
   if (*xsink)
      return 0;

   ensure_unique(val, xsink);

   // evaluating a list must give another list
   int size = nl->size();
   int offset = nl->getEntryAsInt(0);

#ifdef DEBUG
   if ((*val)->type == NT_LIST) {
      QoreListNode **vl = reinterpret_cast<QoreListNode **>(val);
      printd(5, "op_splice() val=%08p (size=%d) list=%08p (size=%d) offset=%d\n", (*val), (*vl)->size(), *nl, size, offset);
   }
   else {
      QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(val);

      printd(5, "op_splice() val=%08p (strlen=%d) list=%08p (size=%d) offset=%d\n", (*val), (*vs)->strlen(), *nl, size, offset);
   }
#endif

   if ((*val)->type == NT_LIST)
   {
      QoreListNode *vl = reinterpret_cast<QoreListNode *>(*val);
      if (size == 1)
	 vl->splice(offset, xsink);
      else
      {
	 int length = nl->getEntryAsInt(1);
	 if (size == 2)
	    vl->splice(offset, length, xsink);
	 else
	    vl->splice(offset, length, nl->retrieve_entry(2), xsink);
      }
   }
   else // must be a string
   {
      QoreStringNode *vs = reinterpret_cast<QoreStringNode *>(*val);

      if (size == 1)
	 vs->splice(offset, xsink);
      else
      {
	 int length = nl->getEntryAsInt(1);
	 if (size == 2)
	    vs->splice(offset, length, xsink);
	 else
	    vs->splice(offset, length, nl->retrieve_entry(2), xsink);
      }
   }

   // reference for return value
   return ref_rv ? (*val)->RefSelf() : 0;
}

static int64 op_chomp(class QoreNode *arg, class QoreNode *x, ExceptionSink *xsink)
{
   //tracein("op_chomp()");
   
   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(arg, &vl, xsink);
   if (xsink->isEvent())
      return 0;

   if (!(*val) || ((*val)->type != NT_STRING && (*val)->type != NT_LIST && (*val)->type != NT_HASH))
      return 0;

   // note that no exception can happen here
   ensure_unique(val, xsink);
   assert(!*xsink);

   {
      QoreStringNode *str = dynamic_cast<QoreStringNode *>(*val);
      if (str) {
	 return str->chomp();
      }
   }

   int64 count = 0;   

   {
      QoreListNode *l = dynamic_cast<QoreListNode *>(*val);
      if (l) {
	 ListIterator li(l);
	 while (li.next())
	 {
	    class QoreNode **v = li.getValuePtr();
	    if (*v && (*v)->type == NT_STRING)
	    {
	       // note that no exception can happen here
	       ensure_unique(v, xsink);
	       assert(!*xsink);
	       QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
	       count += (*vs)->chomp();
	    }
	 }      
	 return count;
      }
   }

   // must be a hash
   QoreHashNode *vh = reinterpret_cast<QoreHashNode *>(*val);
   HashIterator hi(vh);
   while (hi.next())
   {
      class QoreNode **v = hi.getValuePtr();
      if (*v && (*v)->type == NT_STRING)
      {
	 // note that no exception can happen here
	 ensure_unique(v, xsink);
	 assert(!*xsink);
	 QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
	 count += (*vs)->chomp();
      }
   }
   return count;
}

static QoreNode *op_trim(class QoreNode *arg, class QoreNode *x, bool ref_rv, ExceptionSink *xsink)
{
   //tracein("op_trim()");
   
   class AutoVLock vl;
   QoreNode **val = get_var_value_ptr(arg, &vl, xsink);
   if (xsink->isEvent())
      return 0;
   
   if (!(*val) || ((*val)->type != NT_STRING && (*val)->type != NT_LIST && (*val)->type != NT_HASH))
      return 0;
   
   // note that no exception can happen here
   ensure_unique(val, xsink);
   assert(!*xsink);

   if ((*val)->type == NT_STRING) {
      QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(val);
      (*vs)->trim();
   }
   else if ((*val)->type == NT_LIST)
   {
      QoreListNode *l = reinterpret_cast<QoreListNode *>(*val);
      ListIterator li(l);
      while (li.next())
      {
	 class QoreNode **v = li.getValuePtr();
	 if (*v && (*v)->type == NT_STRING)
	 {
	    // note that no exception can happen here
	    ensure_unique(v, xsink);
	    assert(!*xsink);
	    QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
	    (*vs)->trim();
	 }
      }      
   }
   else // is a hash
   {
      QoreHashNode *vh = reinterpret_cast<QoreHashNode *>(*val);
      HashIterator hi(vh);
      while (hi.next())
      {
	 class QoreNode **v = hi.getValuePtr();
	 if (*v && (*v)->type == NT_STRING)
	 {
	    // note that no exception can happen here
	    assert(!*xsink);
	    ensure_unique(v, xsink);
	    QoreStringNode **vs = reinterpret_cast<QoreStringNode **>(v);
	    (*vs)->trim();
	 }
      }
   }

   // reference for return value
   if (ref_rv)
      return (*val)->RefSelf();
   
   return 0;
}

static QoreHashNode *op_minus_hash_string(const QoreHashNode *h, const QoreString *s, ExceptionSink *xsink)
{
   ReferenceHolder<QoreHashNode> nh(h->copy(), xsink);
   nh->deleteKey(s, xsink);
   if (*xsink)
      return 0;
   return nh.release();
}

static QoreHashNode *op_minus_hash_list(const QoreHashNode *h, const QoreListNode *l, ExceptionSink *xsink)
{
   ReferenceHolder<QoreHashNode> x(h->copy(), xsink);

   // treat each element in the list as a string giving a key to delete
   ConstListIterator li(l);
   while (li.next()) {
      QoreStringValueHelper val(li.getValue());
      
      x->deleteKey(*val, xsink);
      if (*xsink)
	 return 0;
   }
   return x.release();
}

static class QoreNode *op_regex_extract(const QoreString *left, const QoreRegexNode *right, ExceptionSink *xsink)
{
   return right->extractSubstrings(left, xsink);
}

static QoreNode *get_node_type(QoreNode *n, const QoreType *t)
{
   assert(n);
   assert(n->type != t);

   if (t == NT_STRING) {
      QoreStringNode *str = new QoreStringNode();
      n->getStringRepresentation(*str);
      return str;
   }

   if (t == NT_INT)
      return new QoreBigIntNode(n->getAsBigInt());

   if (t == NT_FLOAT)
      return new QoreFloatNode(n->getAsFloat());

   if (t == NT_BOOLEAN)
      return new QoreBoolNode(n->getAsBool());

   if (t == NT_DATE) {
      DateTimeNode *dt = new DateTimeNode();
      n->getDateTimeRepresentation(*dt);
      return dt;
   }
   
   if (t == NT_LIST) {
      QoreListNode *l = new QoreListNode();
      l->push(n ? n->RefSelf() : 0);
      return l;
   }

   printd(0, "get_node_type() got type '%s'\n", t->getName());
   assert(false);
   return 0;
}

AbstractOperatorFunction::AbstractOperatorFunction(const QoreType *lt, const QoreType *rt) : ltype(lt), rtype(rt)
{
}

OperatorFunction::OperatorFunction(const QoreType *lt, const QoreType *rt, op_func_t f) : AbstractOperatorFunction(lt, rt), op_func(f)
{
}

BoolOperatorFunction::BoolOperatorFunction(const QoreType *lt, const QoreType *rt, op_bool_func_t f) : AbstractOperatorFunction(lt, rt), op_func(f)
{
}

BigIntOperatorFunction::BigIntOperatorFunction(const QoreType *lt, const QoreType *rt, op_bigint_func_t f) : AbstractOperatorFunction(lt, rt), op_func(f)
{
}

FloatOperatorFunction::FloatOperatorFunction(const QoreType *lt, const QoreType *rt, op_float_func_t f) : AbstractOperatorFunction(lt, rt), op_func(f)
{
}

class QoreNode *OperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1)
      return op_func(left, 0, ref_rv, xsink);

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return op_func(left, right, ref_rv, xsink);
}

bool OperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      ReferenceHolder<QoreNode> rv(op_func(left, 0, true, xsink), xsink);
      return *rv ? rv->getAsBool() : false;
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsBool() : false;
}

int64 OperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      ReferenceHolder<QoreNode> rv(op_func(left, 0, true, xsink), xsink);
      return *rv ? rv->getAsBigInt() : false;
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsBigInt() : 0;
}

double OperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      ReferenceHolder<QoreNode> rv(op_func(left, 0, true, xsink), xsink);
      return *rv ? rv->getAsFloat() : false;
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsFloat() : 0;
}

class QoreNode *NodeOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   if (!ref_rv)
      return 0;
   return op_func(left, right, xsink);
}

bool NodeOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, xsink), xsink);
   return *rv ? rv->getAsBool() : false;
}

int64 NodeOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, xsink), xsink);
   return *rv ? rv->getAsBigInt() : 0;
}

double NodeOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, xsink), xsink);
   return *rv ? rv->getAsFloat() : 0;
}

class QoreNode *EffectNoEvalOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   return op_func(left, right, ref_rv, xsink);
}

bool EffectNoEvalOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsBool() : false;
}

int64 EffectNoEvalOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsBigInt() : 0;
}

double EffectNoEvalOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> rv(op_func(left, right, true, xsink), xsink);
   return *rv ? rv->getAsFloat() : 0;
}

class QoreNode *HashStringOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // return immediately if the return value is ignored, this statement will have no effect and there can be no side-effects
   if (!ref_rv) return 0;

   QoreStringValueHelper r(right);
   return op_func(reinterpret_cast<QoreHashNode *>(left), *r, xsink);
}

bool HashStringOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in a boolean context and cannot have a side-effect
   return false;
}

int64 HashStringOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in an integer context and cannot have a side-effect
   return 0;
}

double HashStringOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in a floating-point context and cannot have a side-effect
   return 0.0;
}

class QoreNode *HashListOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   assert(right && right->type == NT_LIST);
   // return immediately if the return value is ignored, this statement will have no effect and there can be no side-effects
   if (!ref_rv) return 0;

   return op_func(reinterpret_cast<QoreHashNode *>(left), reinterpret_cast<QoreListNode *>(right), xsink);
}

bool HashListOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in a boolean context and cannot have a side-effect
   return false;
}

int64 HashListOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in an integer context and cannot have a side-effect
   return 0;
}

double HashListOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left && left->type == NT_HASH);
   // this operator can never return a value in a floating-point context and cannot have a side-effect
   return 0.0;
}

class QoreNode *NoConvertOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // return immediately if the return value is ignored, this statement will have no effect and there can be no side-effects
   if (!ref_rv) return 0;

   return op_func(left, right);
}

bool NoConvertOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   // this operator can never return a value in a boolean context and cannot have a side-effect
   return false;
}

int64 NoConvertOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   // this operator can never return a value in an integer context and cannot have a side-effect
   return 0;
}

double NoConvertOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   // this operator can never return a value in a floating-point context and cannot have a side-effect
   return 0.0;
}

class QoreNode *EffectBoolOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   bool b = op_func(left, right, xsink);
   return *xsink ? 0 : new QoreBoolNode(b);
}

bool EffectBoolOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left, right, xsink);
}

int64 EffectBoolOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left, right, xsink);
}

double EffectBoolOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left, right, xsink);
}

class QoreNode *SimpleBoolOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   if (!ref_rv)
      return 0;

   bool b = op_func(left, right);
   return new QoreBoolNode(b);
}

bool SimpleBoolOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left, right);
}

int64 SimpleBoolOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left, right);
}

double SimpleBoolOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left, right);
}

class QoreNode *VarRefOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   assert(left);
   return op_func(left, ref_rv, xsink);
}

bool VarRefOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left);
   ReferenceHolder<QoreNode> rv(op_func(left, true, xsink), xsink);
   return *rv ? rv->getAsBool() : false;
}

int64 VarRefOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left);
   ReferenceHolder<QoreNode> rv(op_func(left, true, xsink), xsink);
   return *rv ? rv->getAsBigInt() : 0;
}

double VarRefOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(left);
   ReferenceHolder<QoreNode> rv(op_func(left, true, xsink), xsink);
   return *rv ? rv->getAsFloat() : 0.0;
}

class QoreNode *StringStringStringOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // return immediately if the return value is ignored, this statement will have no effect and there can be no side-effects
   if (!ref_rv) return 0;

   QoreStringValueHelper l(left);
   QoreStringValueHelper r(right);
   return op_func(*l, *r, xsink);
}

bool StringStringStringOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   QoreStringValueHelper r(right);

   TempQoreStringNode rv(op_func(*l, *r, xsink));
   return *rv ? rv->getAsBool() : false;
}

int64 StringStringStringOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   QoreStringValueHelper r(right);

   TempQoreStringNode rv(op_func(*l, *r, xsink));
   return *rv ? rv->getAsBigInt() : 0;
}

double StringStringStringOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   QoreStringValueHelper r(right);

   TempQoreStringNode rv(op_func(*l, *r, xsink));
   return *rv ? rv->getAsFloat() : 0.0;
}

class QoreNode *ListStringRegexOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);

   // conditionally evaluate left-hand node only
   QoreNodeEvalOptionalRefHolder le(left, xsink);
   if (*xsink) return 0;
   
   // return immediately if the return value is ignored, this statement will have no effect and there can be no (other) side-effects
   if (!ref_rv) return 0;

   QoreStringValueHelper l(*le);
   return op_func(*l, reinterpret_cast<const QoreRegexNode *>(right), xsink);
}

bool ListStringRegexOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);
   // this operator can never return a value in this context and cannot have a side-effect
   return false;
}

int64 ListStringRegexOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);
   // this operator can never return a value in this context and cannot have a side-effect
   return 0;
}

double ListStringRegexOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);
   // this operator can never return a value in this context and cannot have a side-effect
   return 0.0;
}

class QoreNode *BoolOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      bool rv = op_func(left, 0, xsink);
      if (!ref_rv || *xsink)
	 return 0;
      return new QoreBoolNode(rv);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   bool rv = op_func(left, right, xsink);
   if (!ref_rv || *xsink)
      return 0;
   return new QoreBoolNode(rv);
}

bool BoolOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return op_func(left, right, xsink);
}

int64 BoolOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (int64)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (int64)op_func(left, right, xsink);
}

double BoolOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (double)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (double)op_func(left, right, xsink);
}

class QoreNode *NoConvertBoolOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   if (args == 1) {
      bool rv = op_func(left, 0, xsink);
      if (!ref_rv || *xsink)
	 return 0;
      return new QoreBoolNode(rv);
   }

   bool rv = op_func(left, right, xsink);
   if (!ref_rv || *xsink)
      return 0;
   return new QoreBoolNode(rv);
}

bool NoConvertBoolOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   if (args == 1)
      return op_func(left, right, xsink);

   return op_func(left, right, xsink);
}

int64 NoConvertBoolOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   if (args == 1)
      return (int64)op_func(left, right, xsink);

   return (int64)op_func(left, right, xsink);
}

double NoConvertBoolOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   if (args == 1)
      return (double)op_func(left, right, xsink);

   return (double)op_func(left, right, xsink);
}

class QoreNode *BoolStrStrOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   bool rv;
   if (args == 1) {
      rv = op_func(*l, 0, xsink);
   }
   else {
      QoreStringValueHelper r(right);

      rv = op_func(*l, *r, xsink);
   }
   if (!ref_rv || *xsink)
      return 0;
   return new QoreBoolNode(rv);
}

bool BoolStrStrOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   if (args == 1)
      return op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return op_func(*l, *r, xsink);
}

int64 BoolStrStrOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   if (args == 1)
      return (int64)op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return (int64)op_func(*l, *r, xsink);
}

double BoolStrStrOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);   
   if (args == 1)
      return (double)op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return (double)op_func(*l, *r, xsink);
}

class QoreNode *BoolDateOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side-effects
   if (!ref_rv)
      return 0;

   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);
   bool b = op_func(*l, *r);
   return new QoreBoolNode(b);
}

bool BoolDateOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);
   return op_func(*l, *r);
}

int64 BoolDateOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);
   return (int64)op_func(*l, *r);
}

double BoolDateOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);   
   DateTimeNodeValueHelper r(right);
   return (double)op_func(*l, *r);
}

class QoreNode *DateOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions (date addition and subtraction) can have no side-effects
   if (!ref_rv)
      return 0;

   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);

   return op_func(*l, *r);
}

bool DateOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);

   SimpleRefHolder<DateTimeNode> date(op_func(*l, *r));
   return date->getEpochSeconds() ? true : false;
}

int64 DateOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);

   SimpleRefHolder<DateTimeNode> date(op_func(*l, *r));
   return date->getEpochSeconds();
}

double DateOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeNodeValueHelper l(left);
   DateTimeNodeValueHelper r(right);

   SimpleRefHolder<DateTimeNode> date(op_func(*l, *r));
   return (double)date->getEpochSeconds();
}

QoreNode *BoolIntOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   bool b = op_func(left->getAsBigInt(), right->getAsBigInt());
   return new QoreBoolNode(b);
}

bool BoolIntOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsBigInt(), right->getAsBigInt());
}

int64 BoolIntOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left->getAsBigInt(), right->getAsBigInt());
}

double BoolIntOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsBigInt(), right->getAsBigInt());
}

QoreNode *IntIntOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   int64 i = op_func(left->getAsBigInt(), right->getAsBigInt());
   return new QoreBigIntNode(i);
}

bool IntIntOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)op_func(left->getAsBigInt(), right->getAsBigInt());
}

int64 IntIntOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsBigInt(), right->getAsBigInt());
}

double IntIntOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsBigInt(), right->getAsBigInt());
}

QoreNode *DivideIntOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   int64 i = op_func(left->getAsBigInt(), right->getAsBigInt(), xsink);
   return *xsink ? 0 : new QoreBigIntNode(i);
}

bool DivideIntOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)op_func(left->getAsBigInt(), right->getAsBigInt(), xsink);
}

int64 DivideIntOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsBigInt(), right->getAsBigInt(), xsink);
}

double DivideIntOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsBigInt(), right->getAsBigInt(), xsink);
}

QoreNode *UnaryMinusIntOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   int64 i = -left->getAsBigInt();
   return new QoreBigIntNode(i);
}

bool UnaryMinusIntOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)-left->getAsBigInt();
}

int64 UnaryMinusIntOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return -left->getAsBigInt();
}

double UnaryMinusIntOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)-left->getAsBigInt();
}

QoreNode *BoolFloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   bool b = op_func(left->getAsFloat(), right->getAsFloat());
   return new QoreBoolNode(b);
}

bool BoolFloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsFloat(), right->getAsFloat());
}

int64 BoolFloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left->getAsFloat(), right->getAsFloat());
}

double BoolFloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsFloat(), right->getAsFloat());
}

QoreNode *FloatFloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   double f = op_func(left->getAsFloat(), right->getAsFloat());
   return new QoreFloatNode(f);
}

bool FloatFloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)op_func(left->getAsFloat(), right->getAsFloat());
}

int64 FloatFloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left->getAsFloat(), right->getAsFloat());
}

double FloatFloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsFloat(), right->getAsFloat());
}

QoreNode *DivideFloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   double f = op_func(left->getAsFloat(), right->getAsFloat(), xsink);
   return *xsink ? 0 : new QoreFloatNode(f);
}

bool DivideFloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)op_func(left->getAsFloat(), right->getAsFloat(), xsink);
}

int64 DivideFloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left->getAsFloat(), right->getAsFloat(), xsink);
}

double DivideFloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsFloat(), right->getAsFloat(), xsink);
}

QoreNode *UnaryMinusFloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   double f = -left->getAsFloat();
   return new QoreFloatNode(f);
}

bool UnaryMinusFloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)-left->getAsFloat();
}

int64 UnaryMinusFloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)-left->getAsFloat();
}

double UnaryMinusFloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return -left->getAsFloat();
}

QoreNode *CompareFloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   int64 i = op_func(left->getAsFloat(), right->getAsFloat());
   return new QoreBigIntNode(i);
}

bool CompareFloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)op_func(left->getAsFloat(), right->getAsFloat());
}

int64 CompareFloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsFloat(), right->getAsFloat());
}

double CompareFloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsFloat(), right->getAsFloat());
}

QoreNode *BoolNotOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   return new QoreBoolNode(!left->getAsBool());
}

bool BoolNotOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return !left->getAsBool();
}

int64 BoolNotOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)(!left->getAsBool());
}

double BoolNotOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)(!left->getAsBool());
}

QoreNode *IntegerNotOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   return new QoreBigIntNode(~left->getAsBigInt());
}

bool IntegerNotOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (bool)~left->getAsBigInt();
}

int64 IntegerNotOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return ~left->getAsBigInt();
}

double IntegerNotOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)(~left->getAsBigInt());
}

QoreNode *CompareDateOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // this operator can have no side effects
   if (!ref_rv)
      return 0;

   DateTimeValueHelper l(left);   
   DateTimeValueHelper r(right);   

   int64 i = DateTime::compareDates(*l, *r);
   return new QoreBigIntNode(i);
}

bool CompareDateOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeValueHelper l(left);   
   DateTimeValueHelper r(right);   

   return (bool)DateTime::compareDates(*l, *r);
}

int64 CompareDateOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeValueHelper l(left);   
   DateTimeValueHelper r(right);   

   return DateTime::compareDates(*l, *r);
}

double CompareDateOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   DateTimeValueHelper l(left);   
   DateTimeValueHelper r(right);   

   return (double)DateTime::compareDates(*l, *r);
}

QoreNode *LogicOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   // these functions can have no side effects
   if (!ref_rv)
      return 0;

   bool b = op_func(left->getAsBool(), right->getAsBool());
   return new QoreBoolNode(b);
}

bool LogicOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return op_func(left->getAsBool(), right->getAsBool());
}

int64 LogicOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (int64)op_func(left->getAsBool(), right->getAsBool());
}

double LogicOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   return (double)op_func(left->getAsBool(), right->getAsBool());
}

QoreNode *BoolStrRegexOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);

   // conditionally evaluate left-hand node only
   QoreNodeEvalOptionalRefHolder le(left, xsink);
   if (*xsink) return 0;

   // return immediately if the return value is ignored, this statement will have no effect and there can be no (other) side-effects
   if (!ref_rv) return 0;

   QoreStringValueHelper l(*le);
   bool rv = op_func(*l, reinterpret_cast<const QoreRegexNode *>(right), xsink);
   return *xsink ? 0 : new QoreBoolNode(rv);
}

bool BoolStrRegexOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);

   // conditionally evaluate left-hand node only
   QoreNodeEvalOptionalRefHolder le(left, xsink);
   if (*xsink) return 0;

   QoreStringValueHelper l(*le);

   return op_func(*l, reinterpret_cast<const QoreRegexNode *>(right), xsink);
}

int64 BoolStrRegexOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);

   // conditionally evaluate left-hand node only
   QoreNodeEvalOptionalRefHolder le(left, xsink);
   if (*xsink) return 0;

   QoreStringValueHelper l(*le);

   return (int64)op_func(*l, reinterpret_cast<const QoreRegexNode *>(right), xsink);
}

double BoolStrRegexOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   assert(right && right->type == NT_REGEX);

   // conditionally evaluate left-hand node only
   QoreNodeEvalOptionalRefHolder le(left, xsink);
   if (*xsink) return 0;

   QoreStringValueHelper l(*le);

   return (double)op_func(*l, reinterpret_cast<const QoreRegexNode *>(right), xsink);
}

QoreNode *BigIntStrStrOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   int64 rv;
   if (args == 1) {
      rv = op_func(*l, 0, xsink);
   }
   else {
      QoreStringValueHelper r(right);

      rv = op_func(*l, *r, xsink);
   }
   if (!ref_rv || *xsink)
      return 0;
   return new QoreBigIntNode(rv);
}

bool BigIntStrStrOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   if (args == 1)
      return (bool)op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return (bool)op_func(*l, *r, xsink);
}

int64 BigIntStrStrOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);
   
   if (args == 1)
      return op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return op_func(*l, *r, xsink);
}

double BigIntStrStrOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   QoreStringValueHelper l(left);   
   if (args == 1)
      return (double)op_func(*l, 0, xsink);

   QoreStringValueHelper r(right);
   return (double)op_func(*l, *r, xsink);
}

QoreNode *BigIntOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      int64 rv = op_func(left, right, xsink);
      if (!ref_rv || xsink->isException())
	 return NULL;
      return new QoreBigIntNode(rv);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   int64 rv = op_func(left, right, xsink);
   if (!ref_rv || xsink->isException())
      return NULL;
   return new QoreBigIntNode(rv);
}

bool BigIntOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (bool)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (bool)op_func(left, right, xsink);
}

int64 BigIntOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL)) {
      right = get_node_type(right, rtype);
      r = right;
   }

   return op_func(left, right, xsink);
}

double BigIntOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (double)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL)) {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (double)op_func(left, right, xsink);
}

QoreNode *FloatOperatorFunction::eval(QoreNode *left, QoreNode *right, bool ref_rv, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      double rv = op_func(left, right, xsink);
      if (!ref_rv || xsink->isException())
	 return NULL;
      return new QoreFloatNode(rv);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL)) {
      right = get_node_type(right, rtype);
      r = right;
   }

   double rv = op_func(left, right, xsink);
   if (!ref_rv || xsink->isException())
      return NULL;
   return new QoreFloatNode(rv);
}

bool FloatOperatorFunction::bool_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (bool)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (bool)op_func(left, right, xsink);
}

int64 FloatOperatorFunction::bigint_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return (int64)op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return (int64)op_func(left, right, xsink);
}

double FloatOperatorFunction::float_eval(QoreNode *left, QoreNode *right, int args, ExceptionSink *xsink) const
{
   ReferenceHolder<QoreNode> l(xsink);

   // convert node type to required argument types for operator if necessary
   if ((left->type != ltype) && (ltype != NT_ALL))
   {
      left = get_node_type(left, ltype);
      l = left;
   }

   if (args == 1) {
      return op_func(left, right, xsink);
   }

   ReferenceHolder<QoreNode> r(xsink);

   // convert node type to required argument types for operator if necessary
   if ((right->type != rtype) && (rtype != NT_ALL))
   {
      right = get_node_type(right, rtype);
      r = right;
   }

   return op_func(left, right, xsink);
}

Operator::Operator(int arg, char *n, char *desc, int n_evalArgs, bool n_effect, bool n_lvalue)
{
   args = arg;
   name = n;
   description = desc;
   evalArgs = n_evalArgs;
   opMatrix = NULL;
   effect = n_effect;
   lvalue = n_lvalue;
}

Operator::~Operator()
{
   // erase all functions
   for (unsigned i = 0, size = functions.size(); i < size; i++)
      delete functions[i];
   if (opMatrix)
      delete [] opMatrix;
}

bool Operator::hasEffect() const
{ 
   return effect; 
}

bool Operator::needsLValue() const
{ 
   return lvalue;
}

char *Operator::getName() const
{
   return name;
}

char *Operator::getDescription() const
{
   return description;
}

void Operator::init()
{
   if (!evalArgs || (functions.size() == 1))
      return;
   opMatrix = new int[NUM_VALUE_TYPES][NUM_VALUE_TYPES];
   // create function lookup matrix
   for (int i = 0; i < NUM_VALUE_TYPES; i++)
      for (int j = 0; j < NUM_VALUE_TYPES; j++)
	 opMatrix[i][j] = findFunction(QTM.find(i), QTM.find(j));
}

// if there is no exact match, the first partial match counts as a match
// static method
int Operator::match(const QoreType *ntype, const QoreType *rtype)
{
   // if any type is OK, or an exact match
   if (rtype == NT_ALL || ntype == rtype || (rtype == NT_VARREF && ntype == NT_SELF_VARREF))
      return 1;
   else // otherwise fail
      return 0;
}

int Operator::get_function(QoreNodeEvalOptionalRefHolder &nleft, ExceptionSink *xsink) const
{
   int t;
   // find operator function
   if (functions.size() == 1)
      t = 0;
   else if (nleft->type->getID() < NUM_VALUE_TYPES)
      t = opMatrix[nleft->type->getID()][NT_NOTHING->getID()];
   else
      t = findFunction(nleft->type, NT_NOTHING);
   
   printd(5, "Operator::get_function() found function %d\n", t);
   return t;
}

int Operator::get_function(QoreNodeEvalOptionalRefHolder &nleft, QoreNodeEvalOptionalRefHolder &nright, ExceptionSink *xsink) const
{
   int t;
   // find operator function
   if (functions.size() == 1)
      t = 0;
   else if (nleft->type->getID() < NUM_VALUE_TYPES && nright->type->getID() < NUM_VALUE_TYPES)
      t = opMatrix[nleft->type->getID()][nright->type->getID()];
   else
      t = findFunction(nleft->type, nright->type);

   printd(5, "Operator::get_function() found function %d\n", t);
   return t;
}

// Operator::eval(): return value requires a deref(xsink) afterwards
// there are 3 main cases which have been split into 3 sections as a speed optimization
// 1: evalArgs 1 argument
// 2: evalArgs 2 arguments
// 3: pass-through all arguments
QoreNode *Operator::eval(QoreNode *left_side, QoreNode *right_side, bool ref_rv, ExceptionSink *xsink) const
{
   printd(5, "evaluating operator %s (0x%08p 0x%08p)\n", description, left_side, right_side);
   if (evalArgs)
   {
      QoreNodeEvalOptionalRefHolder nleft(left_side, xsink);
      if (*xsink)
	 return 0;
      if (!nleft)
	 nleft.assign(false, Nothing);

      int t;

      if (args == 1)
      {
	 if ((t = get_function(nleft, xsink)) == -1)
	    return 0;

	 return functions[t]->eval(*nleft, 0, ref_rv, 1, xsink);
      }

      QoreNodeEvalOptionalRefHolder nright(right_side, xsink);
      if (*xsink)
	 return 0;
      if (!nright)
	 nright.assign(false, Nothing);
      
      // find operator function
      if ((t = get_function(nleft, nright, xsink)) == -1)
	 return 0;
      
      return functions[t]->eval(*nleft, *nright, ref_rv, 2, xsink);
   }

   // in this case there will only be one entry (0)
   printd(5, "Operator::eval() evaluating function 0\n");
   return functions[0]->eval(left_side, right_side, ref_rv, args, xsink);
}

// Operator::bool_eval(): return value
// there are 3 main cases which have been split into 3 sections as a speed optimization
// 1: evalArgs 1 argument
// 2: evalArgs 2 arguments
// 3: pass-through all arguments
bool Operator::bool_eval(QoreNode *left_side, QoreNode *right_side, ExceptionSink *xsink) const
{
   printd(5, "evaluating operator %s (0x%08p 0x%08p)\n", description, left_side, right_side);
   if (evalArgs)
   {
      QoreNodeEvalOptionalRefHolder nleft(left_side, xsink);
      if (*xsink)
	 return 0;
      if (!nleft)
	 nleft.assign(false, Nothing);
      
      int t;
      if (args == 1)
      {
	 if ((t = get_function(nleft, xsink)) == -1)
	    return 0;

	 return functions[t]->bool_eval(*nleft, NULL, 1, xsink);
      }

      QoreNodeEvalOptionalRefHolder nright(right_side, xsink);
      if (*xsink)
	 return 0;
      if (!nright)
	 nright.assign(false, Nothing);
      
      // find operator function
      if ((t = get_function(nleft, nright, xsink)) == -1)
	 return 0;
      
      return functions[t]->bool_eval(*nleft, *nright, 2, xsink);
   }

   // in this case there will only be one entry (0)
   printd(5, "Operator::bool_eval() evaluating function 0\n");
   return functions[0]->bool_eval(left_side, right_side, args, xsink);
}

// Operator::bigint_eval(): return value
// there are 3 main cases which have been split into 3 sections as a speed optimization
// 1: evalArgs 1 argument
// 2: evalArgs 2 arguments
// 3: pass-through all arguments
int64 Operator::bigint_eval(QoreNode *left, QoreNode *right, ExceptionSink *xsink) const
{
   printd(5, "evaluating operator %s (0x%08p 0x%08p)\n", description, left, right);
   if (evalArgs)
   {
      QoreNodeEvalOptionalRefHolder nleft(left, xsink);
      if (*xsink)
	 return 0;
      if (!nleft)
	 nleft.assign(false, Nothing);

      int t;

      if (args == 1)
      {
	 if ((t = get_function(nleft, xsink)) == -1)
	    return 0;

	 return functions[t]->bigint_eval(*nleft, NULL, 1, xsink);
      }

      QoreNodeEvalOptionalRefHolder nright(right, xsink);
      if (*xsink)
	 return 0;
      if (!nright)
	 nright.assign(false, Nothing);
	 
      // find operator function
      if ((t = get_function(nleft, nright, xsink)) == -1)
	 return 0;
      
      return functions[t]->bigint_eval(*nleft, *nright, 2, xsink);
   }

   // in this case there will only be one entry (0)
   printd(5, "Operator::bigint_eval() evaluating function 0\n");
   return functions[0]->bigint_eval(left, right, args, xsink);
}

// Operator::float_eval(): return value
// there are 3 main cases which have been split into 3 sections as a speed optimization
// 1: evalArgs 1 argument
// 2: evalArgs 2 arguments
// 3: pass-through all arguments
double Operator::float_eval(QoreNode *left, QoreNode *right, ExceptionSink *xsink) const
{
   printd(5, "evaluating operator %s (0x%08p 0x%08p)\n", description, left, right);
   if (evalArgs)
   {
      QoreNodeEvalOptionalRefHolder nleft(left, xsink);
      if (*xsink)
	 return 0;
      if (!nleft)
	 nleft.assign(false, Nothing);

      int t;

      if (args == 1)
      {
	 if ((t = get_function(nleft, xsink)) == -1)
	    return 0;

	 return functions[t]->float_eval(*nleft, NULL, 1, xsink);
      }

      QoreNodeEvalOptionalRefHolder nright(right, xsink);
      if (*xsink)
	 return 0;
      if (!nright)
	 nright.assign(false, Nothing);
	 
      // find operator function
      if ((t = get_function(nleft, nright, xsink)) == -1)
	 return 0;

      return functions[t]->float_eval(*nleft, *nright, 2, xsink);
   }

   // in this case there will only be one entry (0)
   printd(5, "Operator::float_eval() evaluating function 0\n");
   return functions[0]->float_eval(left, right, args, xsink);
}

int Operator::findFunction(const QoreType *ltype, const QoreType *rtype) const
{
   int m = -1;
   
   //tracein("Operator::findFunction()");
   // loop through all operator functions
   
   for (int i = 0, size = functions.size(); i < size; i++)
   {
      // check for a match on the left side
      if (match(ltype, functions[i]->ltype))
      {
	 /* if there is only one operator or there is also
	 * a match on the right side, return */
	 if ((args == 1) || 
	     ((args == 2) && match(rtype, functions[i]->rtype)))
	    return i;
	 if (m == -1)
	    m = i;
	 continue;
      }
      if ((args == 2) && match(rtype, functions[i]->rtype) 
	  && (m == -1))
	 m = i;
   }
   /* if there is no match of any kind, take the highest priority function
      * (row 0), and try to convert the arguments, otherwise return the best 
      * partial match
      */
   //traceout("Operator::findFunction()");
   return m == -1 ? 0 : m;
}

void Operator::addFunction(const QoreType *lt, const QoreType *rt, op_func_t f)
{
   functions.push_back(new OperatorFunction(lt, rt, f));
}

void Operator::addFunction(const QoreType *lt, const QoreType *rt, op_bool_func_t f)
{
   functions.push_back(new BoolOperatorFunction(lt, rt, f));
}

void Operator::addFunction(const QoreType *lt, const QoreType *rt, op_bigint_func_t f)
{
   functions.push_back(new BigIntOperatorFunction(lt, rt, f));
}

void Operator::addFunction(const QoreType *lt, const QoreType *rt, op_float_func_t f)
{
   functions.push_back(new FloatOperatorFunction(lt, rt, f));
}

OperatorList::OperatorList()
{
}

OperatorList::~OperatorList()
{
   oplist_t::iterator i;
   while ((i = begin()) != end())
   {
      delete (*i);
      erase(i);
   }
}

class Operator *OperatorList::add(class Operator *o)
{
   push_back(o);
   return o;
}

// registers the system operators and system operator functions
void OperatorList::init()
{
   tracein("OperatorList::init()");
   
   OP_LOG_AND = add(new Operator(2, "&&", "logical-and", 0, false));
   OP_LOG_AND->addEffectFunction(op_log_and);

   OP_LOG_OR = add(new Operator(2, "||", "logical-or", 0, false));
   OP_LOG_OR->addEffectFunction(op_log_or);
   
   OP_LOG_LT = add(new Operator(2, "<", "less-than", 1, false));
   OP_LOG_LT->addFunction(op_log_lt_float);
   OP_LOG_LT->addFunction(op_log_lt_bigint);
   OP_LOG_LT->addFunction(op_log_lt_string);
   OP_LOG_LT->addFunction(op_log_lt_date);
   
   OP_LOG_GT = add(new Operator(2, ">", "greater-than", 1, false));
   OP_LOG_GT->addFunction(op_log_gt_float);
   OP_LOG_GT->addFunction(op_log_gt_bigint);
   OP_LOG_GT->addFunction(op_log_gt_string);
   OP_LOG_GT->addFunction(op_log_gt_date);

   OP_LOG_EQ = add(new Operator(2, "==", "logical-equals", 1, false));
   OP_LOG_EQ->addFunction(op_log_eq_string);
   OP_LOG_EQ->addFunction(op_log_eq_float);
   OP_LOG_EQ->addFunction(op_log_eq_bigint);
   OP_LOG_EQ->addFunction(op_log_eq_boolean);
   OP_LOG_EQ->addFunction(op_log_eq_date);
   OP_LOG_EQ->addNoConvertFunction(NT_LIST,    NT_ALL,     op_log_eq_list);
   OP_LOG_EQ->addNoConvertFunction(NT_ALL,     NT_LIST,    op_log_eq_list);
   OP_LOG_EQ->addNoConvertFunction(NT_HASH,    NT_ALL,     op_log_eq_hash);
   OP_LOG_EQ->addNoConvertFunction(NT_ALL,     NT_HASH,    op_log_eq_hash);
   OP_LOG_EQ->addNoConvertFunction(NT_OBJECT,  NT_ALL,     op_log_eq_object);
   OP_LOG_EQ->addNoConvertFunction(NT_ALL,     NT_OBJECT,  op_log_eq_object);
   OP_LOG_EQ->addFunction(NT_NULL,    NT_ALL,     op_log_eq_null);
   OP_LOG_EQ->addFunction(NT_ALL,     NT_NULL,    op_log_eq_null);
   OP_LOG_EQ->addFunction(NT_NOTHING, NT_NOTHING, op_log_eq_nothing);
   OP_LOG_EQ->addFunction(NT_BINARY,  NT_BINARY,  op_log_eq_binary);

   OP_LOG_NE = add(new Operator(2, "!=", "not-equals", 1, false));
   OP_LOG_NE->addFunction(op_log_ne_string);
   OP_LOG_NE->addFunction(op_log_ne_float);
   OP_LOG_NE->addFunction(op_log_ne_bigint);
   OP_LOG_NE->addFunction(op_log_ne_boolean);
   OP_LOG_NE->addFunction(op_log_ne_date);
   OP_LOG_NE->addNoConvertFunction(NT_LIST,    NT_ALL,     op_log_ne_list);
   OP_LOG_NE->addNoConvertFunction(NT_ALL,     NT_LIST,    op_log_ne_list);
   OP_LOG_NE->addNoConvertFunction(NT_HASH,    NT_ALL,     op_log_ne_hash);
   OP_LOG_NE->addNoConvertFunction(NT_ALL,     NT_HASH,    op_log_ne_hash);
   OP_LOG_NE->addNoConvertFunction(NT_OBJECT,  NT_ALL,     op_log_ne_object);
   OP_LOG_NE->addNoConvertFunction(NT_ALL,     NT_OBJECT,  op_log_ne_object);
   OP_LOG_NE->addFunction(NT_NULL,    NT_ALL,     op_log_ne_null);
   OP_LOG_NE->addFunction(NT_ALL,     NT_NULL,    op_log_ne_null);
   OP_LOG_NE->addFunction(NT_NOTHING, NT_NOTHING, op_log_ne_nothing);
   OP_LOG_NE->addFunction(NT_BINARY,  NT_BINARY,  op_log_ne_binary);
   
   OP_LOG_LE = add(new Operator(2, "<=", "less-than-or-equals", 1, false));
   OP_LOG_LE->addFunction(op_log_le_float);
   OP_LOG_LE->addFunction(op_log_le_bigint);
   OP_LOG_LE->addFunction(op_log_le_string);
   OP_LOG_LE->addFunction(op_log_le_date);

   OP_LOG_GE = add(new Operator(2, ">=", "greater-than-or-equals", 1, false));
   OP_LOG_GE->addFunction(op_log_ge_float);
   OP_LOG_GE->addFunction(op_log_ge_bigint);
   OP_LOG_GE->addFunction(op_log_ge_string);
   OP_LOG_GE->addFunction(op_log_ge_date);

   OP_ABSOLUTE_EQ = add(new Operator(2, "===", "absolute logical-equals", 0, false));
   OP_ABSOLUTE_EQ->addFunction(NT_ALL, NT_ALL, op_absolute_log_eq);
   
   OP_ABSOLUTE_NE = add(new Operator(2, "!==", "absolute logical-not-equals", 0, false));
   OP_ABSOLUTE_NE->addFunction(NT_ALL, NT_ALL, op_absolute_log_neq);
   
   OP_REGEX_MATCH = add(new Operator(2, "=~", "regular expression match", 0, false));
   OP_REGEX_MATCH->addFunction(op_regex_match);
   
   OP_REGEX_NMATCH = add(new Operator(2, "!~", "regular expression negative match", 0, false));
   OP_REGEX_NMATCH->addFunction(op_regex_nmatch);

   OP_EXISTS = add(new Operator(1, "exists", "exists", 0, false));
   OP_EXISTS->addFunction(NT_ALL, NT_NONE, op_exists);
   
   OP_INSTANCEOF = add(new Operator(2, "instanceof", "instanceof", 0, false));
   OP_INSTANCEOF->addFunction(NT_ALL, NT_CLASSREF, op_instanceof);
   
   OP_NOT = add(new Operator(1, "!", "logical-not", 1, false));
   OP_NOT->addBoolNotFunction();
      
   // bigint operators
   OP_LOG_CMP = add(new Operator(2, "<=>", "logical-comparison", 1, false));
   OP_LOG_CMP->addFunction(op_cmp_string);
   OP_LOG_CMP->addFunction(op_cmp_double);
   OP_LOG_CMP->addFunction(op_cmp_bigint);
   OP_LOG_CMP->addCompareDateFunction();

   OP_ELEMENTS = add(new Operator(1, "elements", "number of elements", 0, false));
   OP_ELEMENTS->addFunction(NT_ALL, NT_NONE, op_elements);

   OP_MODULA = add(new Operator(2, "%", "modula", 1, false));
   OP_MODULA->addFunction(op_modula_int);

   // non-boolean operators
   OP_ASSIGNMENT = add(new Operator(2, "=", "assignment", 0, true, true));
   OP_ASSIGNMENT->addFunction(NT_ALL, NT_ALL, op_assignment);
   
   OP_LIST_ASSIGNMENT = add(new Operator(2, "(list) =", "list assignment", 0, true, true));
   OP_LIST_ASSIGNMENT->addFunction(NT_ALL, NT_ALL, op_list_assignment);
   
   OP_BIN_AND = add(new Operator(2, "&", "binary-and", 1, false));
   OP_BIN_AND->addFunction(op_bin_and_int);

   OP_BIN_OR = add(new Operator(2, "|", "binary-or", 1, false));
   OP_BIN_OR->addFunction(op_bin_or_int);

   OP_BIN_NOT = add(new Operator(1, "~", "binary-not", 1, false));
   OP_BIN_NOT->addIntegerNotFunction();

   OP_BIN_XOR = add(new Operator(2, "^", "binary-xor", 1, false));
   OP_BIN_XOR->addFunction(op_bin_xor_int);

   OP_MINUS = add(new Operator(2, "-", "minus", 1, false));
   OP_MINUS->addFunction(op_minus_date);
   OP_MINUS->addFunction(op_minus_float);
   OP_MINUS->addFunction(op_minus_bigint);
   OP_MINUS->addFunction(op_minus_hash_string);
   OP_MINUS->addFunction(op_minus_hash_list);

   OP_PLUS = add(new Operator(2, "+", "plus", 1, false));
   OP_PLUS->addFunction(NT_LIST,    NT_LIST,   op_plus_list);
   OP_PLUS->addFunction(op_plus_string);
   OP_PLUS->addFunction(op_plus_date);
   OP_PLUS->addFunction(op_plus_float);
   OP_PLUS->addFunction(op_plus_bigint);
   OP_PLUS->addFunction(NT_HASH,    NT_HASH,   op_plus_hash_hash);
   OP_PLUS->addFunction(NT_HASH,    NT_OBJECT, op_plus_hash_object);
   OP_PLUS->addFunction(NT_OBJECT,  NT_HASH,   op_plus_object_hash);

   OP_MULT = add(new Operator(2, "*", "multiply", 1, false));
   OP_MULT->addFunction(op_multiply_float);
   OP_MULT->addFunction(op_multiply_bigint);

   OP_DIV = add(new Operator(2, "/", "divide", 1, false));
   OP_DIV->addFunction(op_divide_float);
   OP_DIV->addFunction(op_divide_bigint);

   OP_UNARY_MINUS = add(new Operator(1, "-", "unary-minus", 1, false));
   OP_UNARY_MINUS->addUnaryMinusFloatFunction();
   OP_UNARY_MINUS->addUnaryMinusIntFunction();

   OP_SHIFT_LEFT = add(new Operator(2, "<<", "shift-left", 1, false));
   OP_SHIFT_LEFT->addFunction(op_shift_left_int);

   OP_SHIFT_RIGHT = add(new Operator(2, ">>", "shift-right", 1, false));
   OP_SHIFT_RIGHT->addFunction(op_shift_right_int);

   OP_POST_INCREMENT = add(new Operator(1, "++", "post-increment", 0, true, true));
   OP_POST_INCREMENT->addFunction(op_post_inc);

   OP_POST_DECREMENT = add(new Operator(1, "--", "post-decrement", 0, true, true));
   OP_POST_DECREMENT->addFunction(op_post_dec);

   OP_PRE_INCREMENT = add(new Operator(1, "++", "pre-increment", 0, true, true));
   OP_PRE_INCREMENT->addFunction(op_pre_inc);

   OP_PRE_DECREMENT = add(new Operator(1, "--", "pre-decrement", 0, true, true));
   OP_PRE_DECREMENT->addFunction(op_pre_dec);

   OP_PLUS_EQUALS = add(new Operator(2, "+=", "plus-equals", 0, true, true));
   OP_PLUS_EQUALS->addFunction(op_plus_equals);

   OP_MINUS_EQUALS = add(new Operator(2, "-=", "minus-equals", 0, true, true));
   OP_MINUS_EQUALS->addFunction(op_minus_equals);

   OP_AND_EQUALS = add(new Operator(2, "&=", "and-equals", 0, true, true));
   OP_AND_EQUALS->addFunction(op_and_equals);

   OP_OR_EQUALS = add(new Operator(2, "|=", "or-equals", 0, true, true));
   OP_OR_EQUALS->addFunction(op_or_equals);

   OP_MODULA_EQUALS = add(new Operator(2, "%=", "modula-equals", 0, true, true));
   OP_MODULA_EQUALS->addFunction(op_modula_equals);

   OP_MULTIPLY_EQUALS = add(new Operator(2, "*=", "multiply-equals", 0, true, true));
   OP_MULTIPLY_EQUALS->addFunction(op_multiply_equals);

   OP_DIVIDE_EQUALS = add(new Operator(2, "/=", "divide-equals", 0, true, true));
   OP_DIVIDE_EQUALS->addFunction(op_divide_equals);

   OP_XOR_EQUALS = add(new Operator(2, "^=", "xor-equals", 0, true, true));
   OP_XOR_EQUALS->addFunction(op_xor_equals);

   OP_SHIFT_LEFT_EQUALS = add(new Operator(2, "<<=", "shift-left-equals", 0, true, true));
   OP_SHIFT_LEFT_EQUALS->addFunction(op_shift_left_equals);

   OP_SHIFT_RIGHT_EQUALS = add(new Operator(2, ">>=", "shift-right-equals", 0, true, true));
   OP_SHIFT_RIGHT_EQUALS->addFunction(op_shift_right_equals);

   OP_LIST_REF = add(new Operator(2, "[]", "list-reference", 0, false));
   OP_LIST_REF->addFunction(NT_ALL, NT_ALL, op_list_ref);

   OP_OBJECT_REF = add(new Operator(2, ".", "hash/object-reference", 0, false));
   OP_OBJECT_REF->addFunction(NT_ALL, NT_ALL, op_object_ref); 

   OP_KEYS = add(new Operator(1, "keys", "list of keys", 0, false));
   OP_KEYS->addFunction(NT_ALL, NT_NONE, op_keys);

   OP_QUESTION_MARK = add(new Operator(2, "question", "question-mark colon", 0, false));
   OP_QUESTION_MARK->addFunction(NT_ALL, NT_ALL, op_question_mark);

   OP_OBJECT_FUNC_REF = add(new Operator(2, ".", "object method call", 0, true, false));
   OP_OBJECT_FUNC_REF->addFunction(NT_ALL, NT_ALL, op_object_method_call);

   OP_NEW = add(new Operator(1, "new", "new object", 0, true, false));
   OP_NEW->addFunction(NT_ALL, NT_NONE, op_new_object);

   OP_SHIFT = add(new Operator(1, "shift", "shift from list", 0, true, true));
   OP_SHIFT->addFunction(op_shift);

   OP_POP = add(new Operator(1, "pop", "pop from list", 0, true, true));
   OP_POP->addFunction(op_pop);

   OP_PUSH = add(new Operator(2, "push", "push on list", 0, true, true));
   OP_PUSH->addFunction(op_push);

   OP_SPLICE = add(new Operator(2, "splice", "splice in list", 0, true, true));
   OP_SPLICE->addFunction(op_splice);

   OP_UNSHIFT = add(new Operator(2, "unshift", "unshift/insert to begnning of list", 0, true, true));
   OP_UNSHIFT->addFunction(op_unshift);

   OP_REGEX_SUBST = add(new Operator(2, "regex subst", "regular expression substitution", 0, true, true));
   OP_REGEX_SUBST->addFunction(NT_ALL, NT_REGEX_SUBST, op_regex_subst);

   OP_REGEX_TRANS = add(new Operator(2, "transliteration", "transliteration", 0, true, true));
   OP_REGEX_TRANS->addFunction(NT_ALL, NT_REGEX_TRANS, op_regex_trans);

   OP_REGEX_EXTRACT = add(new Operator(2, "regular expression subpattern extraction", "regular expression subpattern extraction", 0, false));
   OP_REGEX_EXTRACT->addFunction(op_regex_extract);

   OP_CHOMP = add(new Operator(1, "chomp", "chomp EOL marker from lvalue", 0, true, true));
   OP_CHOMP->addFunction(NT_ALL, NT_NONE, op_chomp);

   OP_TRIM = add(new Operator(1, "trim", "trim characters from an lvalue", 0, true, true));
   OP_TRIM->addFunction(NT_ALL, NT_NONE, op_trim);

   // initialize all operators
   for (oplist_t::iterator i = begin(); i != end(); i++)
      (*i)->init();

   traceout("OperatorList::init()");
}
