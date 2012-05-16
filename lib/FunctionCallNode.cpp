/*
  FunctionCallNode.cpp

  Qore Programming Language

  Copyright 2003 - 2012 David Nichols

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
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreNamespaceIntern.h>
#include <qore/intern/qore_program_private.h>

#include <vector>

// eval method against an object where the assumed qoreclass and method were saved at parse time
AbstractQoreNode *AbstractMethodCallNode::exec(QoreObject *o, const char *c_str, ExceptionSink *xsink) const {
   /* the class and method saved at parse time are used here for this run-time
      optimization: the method pointer saved at parse time is used to execute the
      method directly if the object used at run-time is of the same class as
      either the method or the parse-time class.  Actually any class between the
      parse-time class and the method's class could be used, however I'd have to
      check and make sure that search would be quicker than the quick check
      implemented below on average
   */
   if (qc && (o->getClass() == qc || o->getClass() == method->getClass())) {
      //printd(5, "AbstractMethodCallNode::exec() using parse info for %s::%s() qc=%s\n", method->getClassName(), method->getName(), qc->getName());
      assert(method);
      return variant 
	 ? method->evalNormalVariant(o, reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, xsink)
	 : method->eval(o, args, xsink);
   }
   //printd(5, "AbstractMethodCallNode::exec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(), c_str);
   return o->evalMethod(c_str, args, xsink);
}

// eval method against an object where the assumed qoreclass and method were saved at parse time
int64 AbstractMethodCallNode::bigIntExec(QoreObject *o, const char *c_str, ExceptionSink *xsink) const {
   if (qc && (o->getClass() == qc || o->getClass() == method->getClass())) {
      //printd(5, "AbstractMethodCallNode::exec() using parse info for %s::%s() qc=%s\n", method->getClassName(), method->getName(), qc->getName());
      assert(method);
      return variant 
	 ? method->bigIntEvalNormalVariant(o, reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, xsink)
	 : method->bigIntEval(o, args, xsink);
   }
   //printd(5, "AbstractMethodCallNode::exec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(), c_str);
   return o->bigIntEvalMethod(c_str, args, xsink);
}

// eval method against an object where the assumed qoreclass and method were saved at parse time
int AbstractMethodCallNode::intExec(QoreObject *o, const char *c_str, ExceptionSink *xsink) const {
   if (qc && (o->getClass() == qc || o->getClass() == method->getClass())) {
      //printd(5, "AbstractMethodCallNode::intExec() using parse info for %s::%s() qc=%s\n", method->getClassName(), method->getName(), qc->getName());
      assert(method);
      return variant 
	 ? method->intEvalNormalVariant(o, reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, xsink)
	 : method->intEval(o, args, xsink);
   }
   //printd(5, "AbstractMethodCallNode::intExec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(), c_str);
   return o->intEvalMethod(c_str, args, xsink);
}

// eval method against an object where the assumed qoreclass and method were saved at parse time
bool AbstractMethodCallNode::boolExec(QoreObject *o, const char *c_str, ExceptionSink *xsink) const {
   if (qc && (o->getClass() == qc || o->getClass() == method->getClass())) {
      //printd(5, "AbstractMethodCallNode::boolExec() using parse info for %s::%s() qc=%s\n", method->getClassName(), method->getName(), qc->getName());
      assert(method);
      return variant 
	 ? method->boolEvalNormalVariant(o, reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, xsink)
	 : method->boolEval(o, args, xsink);
   }
   //printd(5, "AbstractMethodCallNode::boolExec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(), c_str);
   return o->boolEvalMethod(c_str, args, xsink);
}

// eval method against an object where the assumed qoreclass and method were saved at parse time
double AbstractMethodCallNode::floatExec(QoreObject *o, const char *c_str, ExceptionSink *xsink) const {
   if (qc && (o->getClass() == qc || o->getClass() == method->getClass())) {
      //printd(5, "AbstractMethodCallNode::floatExec() using parse info for %s::%s() qc=%s\n", method->getClassName(), method->getName(), qc->getName());
      assert(method);
      return variant 
	 ? method->floatEvalNormalVariant(o, reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, xsink)
	 : method->floatEval(o, args, xsink);
   }
   //printd(5, "AbstractMethodCallNode::floatExec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(), c_str);
   return o->floatEvalMethod(c_str, args, xsink);
}

static void invalid_access(QoreFunction *func) {
   // func will always be non-zero with builtin functions
   const char *class_name = func->className();
   parse_error("parse options do not allow access to builtin %s '%s%s%s()'", class_name ? "method" : "function", class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void warn_retval_ignored(QoreFunction *func) {
   const char *class_name = func->className();
   qore_program_private::makeParseWarning(getProgram(), QP_WARN_RETURN_VALUE_IGNORED, "RETURN-VALUE-IGNORED", "call to %s %s%s%s() does not have any side effects and the return value is ignored; to disable this warning, use '%%disable-warning return-value-ignored' in your code", class_name ? "method" : "function", class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void warn_deprecated(QoreFunction *func) {
   const char *class_name = func->className();
   qore_program_private::makeParseWarning(getProgram(), QP_WARN_DEPRECATED, "DEPRECATED", "call to deprecated %s %s%s%s(); to disable this warning, use '%%disable-warning deprecated' in your code", class_name ? "method" : "function", class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void check_flags(QoreFunction *func, int64 flags, int64 pflag) {
   if ((pflag & PF_RETURN_VALUE_IGNORED) && ((flags & QC_CONSTANT) == QC_CONSTANT))
      warn_retval_ignored(func);
   if (flags & QC_DEPRECATED)
      warn_deprecated(func);
}

int FunctionCallBase::parseArgsVariant(LocalVar *oflag, int pflag, QoreFunction *func, const QoreTypeInfo *&returnTypeInfo) {
   // number of local variables declared in arguments
   int lvids = 0;

   // number of arguments in call
   unsigned num_args = args ? args->size() : 0;

   // argument type list
   type_vec_t argTypeInfo;
   argTypeInfo.reserve(num_args);

   bool have_arg_type_info = num_args ? false : true;
   // initialize arguments and setup argument type list (argTypeInfo)
   if (num_args) {
      // do arguments need to be evaluated?
      bool needs_eval = args->needs_eval();

      // turn off reference ok and retval ignored flags
      int n_pflag = pflag & ~(PF_REFERENCE_OK | PF_RETURN_VALUE_IGNORED);
      
      // loop through all args
      for (unsigned i = 0; i < num_args; ++i) {
	 AbstractQoreNode **n = args->get_entry_ptr(i);
	 assert(*n);
	 argTypeInfo.push_back(0);
	 //printd(5, "FunctionCallBase::parseArgsVariant() this=%p (%s) oflag=%p pflag=%d func=%p i=%d/%d arg=%p (%d %s)\n", this, func ? func->getName() : "n/a", oflag, pflag, func, i, num_args, *n, (*n)->getType(), (*n)->getTypeName());
	 if ((*n)->getType() == NT_REFERENCE)
	    (*n) = (*n)->parseInit(oflag, n_pflag | PF_REFERENCE_OK, lvids, argTypeInfo[i]);
	 else
	    (*n) = (*n)->parseInit(oflag, n_pflag, lvids, argTypeInfo[i]);
	 if (!have_arg_type_info && argTypeInfo[i])
	    have_arg_type_info = true;
	 if (!needs_eval && (*n)->needs_eval()) {
	    args->setNeedsEval();
	    needs_eval = true;
	 }
      }
   }

   // resolves pending signatures unconditionally
   if (func) {
      func->resolvePendingSignatures();

      // initialize function or class immediately if parsing a constant initialization expression
      if (pflag & PF_CONST_EXPRESSION) {
	 const QoreClass *qc = func->getClass();
	 if (qc)
	    const_cast<QoreClass *>(qc)->parseInit();
	 else
	    func->parseInit();
      }
   }
   
   // find variant
   //variant = func && have_arg_type_info ? func->parseFindVariant(argTypeInfo) : 0;
   variant = func ? func->parseFindVariant(argTypeInfo) : 0;

   QoreProgram* pgm = getProgram();

   //printd(5, "FunctionCallBase::parseArgsVariant() this=%p po=%lld, ign=%d func=%p have_arg_type_info=%d variant=%p\n", this, po, pflag & PF_RETURN_VALUE_IGNORED, func, have_arg_type_info, variant);

   if (variant) {
      //printd(5, "FunctionCallBase::parseArgsVariant() this=%p variant=%p f=%lld (%lld) c=%lld (%lld)\n", this, variant, variant->getFunctionality(), variant->getFunctionality() & po, variant->getFlags(), variant->getFlags() & QC_RET_VALUE_ONLY);

      if (qore_program_private::parseAddDomain(pgm, variant->getFunctionality()))
	 invalid_access(func);
      int64 flags = variant->getFlags();
      check_flags(func, flags, pflag);
   }
   else if (func) {
      //printd(5, "FunctionCallBase::parseArgsVariant() this=%p func=%p f=%lld (%lld) c=%lld (%lld)\n", this, func, func->getUniqueFunctionality(), func->getUniqueFunctionality() & po, func->getUniqueFlags(), func->getUniqueFlags() & QC_RET_VALUE_ONLY);

      int64 po = pgm->getParseOptions64();
      if (func->getUniqueFunctionality() & po)
	 invalid_access(func);
      check_flags(func, func->getUniqueFlags(), pflag);
   }

   returnTypeInfo = variant ? variant->parseGetReturnTypeInfo() : (func ? func->parseGetUniqueReturnTypeInfo() : 0);

   //printd(5, "FunctionCallBase::parseArgsVariant() this=%p func=%s variant=%p pflag=%d pe=%d\n", this, func ? func->getName() : "n/a", variant, pflag, func ? func->pendingEmpty() : -1);

   // if the function call is being made as a part of a constant expression and 
   // there are uncommitted user variants in the function, then raise an error
   if (func && (pflag & PF_CONST_EXPRESSION) && !variant && !func->pendingEmpty()) {
      const char *name = func->getName();
      const char *cname = func->className();
      QoreStringNode *desc = new QoreStringNode("cannot ");
      if (cname && !strcmp(name, "constructor"))
	 desc->sprintf("instantiate class %s", cname);
      else
	 desc->sprintf("cannot call %s%s%s()", cname ? cname : "", cname ? "::" : "", name);

      desc->concat(" in an expression initializing a constant value at parse time when the function has uncommitted variants and the variant cannot be matched at parse time; to fix this error, add enough type information to the call to allow the variant to be resolved");

      parseException("ILLEGAL-CALL", desc);
   }

   return lvids;
}
 
// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *AbstractFunctionCallNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return evalImpl(xsink);
}

int64 AbstractFunctionCallNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsBigInt() : 0;
}

int AbstractFunctionCallNode::integerEvalImpl(ExceptionSink *xsink) const {
   return bigIntEvalImpl(xsink);
}

bool AbstractFunctionCallNode::boolEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsBool() : 0;
}

double AbstractFunctionCallNode::floatEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsFloat() : 0;
}

AbstractQoreNode *SelfFunctionCallNode::evalImpl(ExceptionSink *xsink) const {
   QoreObject *self = runtime_get_stack_object();
   
   //printd(0, "SelfFunctionCallNode::evalImpl() this=%p self=%p method=%p (%s)\n", this, self, method, ns.ostr);
   if (is_copy)
      return self->getClass()->execCopy(self, xsink);

   if (ns.strlist.size() == 1)
      return exec(self, ns.ostr, xsink);

   assert(method);
   return self->evalMethod(*method, args, xsink);
}

void SelfFunctionCallNode::parseInitCall(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   lvids += parseArgs(oflag, pflag, method ? method->getFunction() : 0, returnTypeInfo);

   if (method)
      printd(5, "SelfFunctionCallNode::parseInitCall() this=%p resolved '%s' to %p\n", this, method->getName(), method);
}

// called at parse time
AbstractQoreNode *SelfFunctionCallNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   if (!oflag) {
      parse_error("cannot call method '%s' outside of class code", getName());
      return this;
   }

   printd(5, "SelfFunctionCallNode::parseInitImpl() this=%p resolving base class call '%s'\n", this, ns.ostr);
   assert(!method);

   // copy method calls will be recognized by name = 0
   if (ns.strlist.size() == 1) {
      if (!strcmp(ns.ostr, "copy")) {
	 printd(5, "SelfFunctionCallNode::parseInitImpl() this=%p resolved to copy constructor\n", this);
	 is_copy = true;
	 if (args)
	    parse_error("no arguments may be passed to copy methods (%d argument%s given in call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", oflag->getTypeInfo()->getUniqueReturnClass()->getName());
      }
      else
         method = qore_class_private::parseResolveSelfMethod(*(getParseClass()), ns.ostr);
   }
   else
      method = qore_class_private::parseResolveSelfMethod(*(getParseClass()), &ns);

   // by here, if there are no errors, the class has been initialized
   parseInitCall(oflag, pflag, lvids, returnTypeInfo);
   return this;
}

int SelfFunctionCallNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("in-object method call (0x%p) to %s::%s()", this, method->getClass()->getName(), method->getName());
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *SelfFunctionCallNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

AbstractQoreNode *SelfFunctionCallNode::makeReferenceNodeAndDeref() {
   AbstractQoreNode *rv;
   if (ns.size() == 1)
      rv = new ParseSelfMethodReferenceNode(ns.takeName());
   else
      rv = new ParseScopedSelfMethodReferenceNode(ns.copy());
   deref();
   return rv;
}

/* get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
   the ExceptionSink is only needed for QoreObject where a method may be executed
   use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
   returns -1 for exception raised, 0 = OK
*/
int FunctionCallNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("function call to '%s()' (0x%p)", getName(), this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *FunctionCallNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

// eval(): return value requires a deref(xsink)
AbstractQoreNode *FunctionCallNode::evalImpl(ExceptionSink *xsink) const {
   //printd(5, "FunctionCallNode::evalImpl() calling %s() current pgm=%p new pgm=%p\n", func->getName(), ::getProgram(), pgm);
   return func->evalFunction(variant, args, pgm, xsink);
}

int64 FunctionCallNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   //printd(5, "FunctionCallNode::bigIntEvalImpl() calling %s() current pgm=%p new pgm=%p\n", func->getName(), ::getProgram(), pgm);
   return func->bigIntEvalFunction(variant, args, pgm, xsink);
}

int FunctionCallNode::integerEvalImpl(ExceptionSink *xsink) const {
   //printd(5, "FunctionCallNode::intEvalImpl() calling %s() current pgm=%p new pgm=%p\n", func->getName(), ::getProgram(), pgm);
   return func->intEvalFunction(variant, args, pgm, xsink);
}

bool FunctionCallNode::boolEvalImpl(ExceptionSink *xsink) const {
   //printd(5, "FunctionCallNode::boolEvalImpl() calling %s() current pgm=%p new pgm=%p\n", func->getName(), ::getProgram(), pgm);
   return func->boolEvalFunction(variant, args, pgm, xsink);
}

double FunctionCallNode::floatEvalImpl(ExceptionSink *xsink) const {
   //printd(5, "FunctionCallNode::floatEvalImpl() calling %s() current pgm=%p new pgm=%p\n", func->getName(), ::getProgram(), pgm);
   return func->floatEvalFunction(variant, args, pgm, xsink);
}

AbstractQoreNode *FunctionCallNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   if (func)
      return this;
   //assert(!func);
   assert(c_str);

   bool abr = checkParseOption(PO_ALLOW_BARE_REFS);

   // try to resolve bare reference if allowed
   if (abr) {
      // check for a local variable with the same name

      bool in_closure;
      LocalVar *id = find_local_var(c_str, in_closure);
      if (id) {
         VarRefNode *vrn = new VarRefNode(takeName(), id, in_closure);
	 CallReferenceCallNode *crcn = new CallReferenceCallNode(vrn, take_args());	 
	 deref();
	 return crcn->parseInit(oflag, pflag, lvids, returnTypeInfo);
      }
   }

   // try to resolve a method call if we are parsing in an object context
   if (oflag) {
      const QoreClass *qc = oflag->getTypeInfo()->getUniqueReturnClass();

      AbstractQoreNode *n = 0;
      if (abr && !qore_class_private::parseResolveInternalMemberAccess(qc, c_str, returnTypeInfo)) {
	 n = new SelfVarrefNode(takeName());
      }
      else if ((n = qore_class_private::parseFindConstantValue(const_cast<QoreClass *>(qc), c_str, returnTypeInfo))) {
	 //printd(5, "FunctionCallNode::parseInitImpl() this=%p n=%p (%d -> %d)\n", this, n, n->reference_count(), n->reference_count() + 1);
	 n->ref();
      }
      else {
	 // check for class static var reference
	 const QoreClass *oqc = 0;
	 QoreVarInfo *vi = qore_class_private::parseFindStaticVar(qc, c_str, oqc, returnTypeInfo);
	 if (vi) {
	    assert(qc);
	    n = new StaticClassVarRefNode(c_str, *oqc, *vi);
	 }
      }

      if (n) {
	 CallReferenceCallNode *crcn = new CallReferenceCallNode(n, take_args());	 
	 deref();
	 return crcn->parseInit(oflag, pflag, lvids, returnTypeInfo);
      }

      if (abr) {
	 SelfFunctionCallNode *sfcn = 0;
	 if (!strcmp(c_str, "copy")) {
	    if (args) {
	       parse_error("no arguments may be passed to copy methods (%d argument%s given in call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", qc->getName());
	       return this;
	    }
	    sfcn = new SelfFunctionCallNode(takeName(), 0);
	 }
	 else {
	    const QoreMethod *m = qore_class_private::parseFindSelfMethod(const_cast<QoreClass *>(qc), c_str);
	    if (m)
	       sfcn = new SelfFunctionCallNode(takeName(), take_args(), m);
	 }
	 if (sfcn) {
	    deref();
	    sfcn->parseInitCall(oflag, pflag, lvids, returnTypeInfo);
	    return sfcn;
	 }
      }
   }

   return parseInitCall(oflag, pflag, lvids, returnTypeInfo);
}

AbstractQoreNode *FunctionCallNode::parseInitCall(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   assert(!func);
   assert(c_str);

   bool abr = checkParseOption(PO_ALLOW_BARE_REFS);

   AbstractQoreNode *n = 0;

   // try to resolve a global var
   if (abr) {
      Var *v = qore_root_ns_private::parseFindGlobalVar(c_str);
      if (v)
	 n = new GlobalVarRefNode(takeName(), v);
   }

   // see if a constant can be resolved
   if (!n) {
      n = qore_root_ns_private::parseFindConstantValue(c_str, returnTypeInfo, false);
      if (n)
	 n->ref();
   }

   if (n) {
      CallReferenceCallNode *crcn = new CallReferenceCallNode(n, take_args());	 
      deref();
      return crcn->parseInit(oflag, pflag, lvids, returnTypeInfo);
   }

   // resolves the function
   func = qore_root_ns_private::parseResolveFunction(c_str);
   free(c_str);
   c_str = 0;

   if (func)
      parseInitFinalizedCall(oflag, pflag, lvids, returnTypeInfo);
      
   return this;
}

void FunctionCallNode::parseInitFinalizedCall(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   assert(func);
   lvids += parseArgs(oflag, pflag, const_cast<QoreFunction *>(func), returnTypeInfo);
}

AbstractQoreNode *FunctionCallNode::makeReferenceNodeAndDerefImpl() {
   return new UnresolvedCallReferenceNode(takeName());
}

AbstractQoreNode *ProgramFunctionCallNode::makeReferenceNodeAndDerefImpl() {
   return new UnresolvedProgramCallReferenceNode(takeName());
}

AbstractQoreNode *ScopedObjectCallNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   if (name) {
      assert(!oc);
      // find object class
      if ((oc = qore_root_ns_private::parseFindScopedClass(loc, *name))) {
	 // check if parse options allow access to this class
	 if (qore_program_private::parseAddDomain(getProgram(), oc->getDomain()))
	    parseException("ILLEGAL-CLASS-INSTANTIATION", "parse options do not allow access to the '%s' class", oc->getName());
      }
      delete name;
      name = 0;
   }
#ifdef DEBUG
   else assert(oc);
#endif

   const QoreMethod *constructor = oc ? oc->parseGetConstructor() : 0;
   lvids += parseArgs(oflag, pflag, constructor ? constructor->getFunction() : 0, typeInfo);

   if (oc) {
      // initialize class immediately, in case the class will be instantiated immediately after during parsing
      // to be assigned to a constant
      qore_class_private::initialize(*const_cast<QoreClass *>(oc));

      typeInfo = oc->getTypeInfo();
      desc.sprintf("new %s", oc->getName());
   }

   //printd(5, "ScopedObjectCallNode::parseInitImpl() this=%p constructor=%p variant=%p\n", this, constructor, variant);

   if (((constructor && constructor->parseIsPrivate()) || (variant && CONMV_const(variant)->isPrivate())) && !qore_class_private::parseCheckPrivateClassAccess(*oc)) {
      if (variant)
	 parse_error("illegal external access to private constructor %s::constructor(%s)", oc->getName(), variant->getSignature()->getSignatureText());
      else
	 parse_error("illegal external access to private constructor of class %s", oc->getName());
   }

   //printd(5, "ScopedObjectCallNode::parseInitImpl() this=%p class=%s (%p) constructor=%p function=%p variant=%p\n", this, oc->getName(), oc, constructor, constructor ? constructor->getFunction() : 0, variant);
      
   return this;
}

AbstractQoreNode *MethodCallNode::execPseudo(const AbstractQoreNode *n, ExceptionSink *xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant=%p\n", qc->getName(), method->getName(), variant);
   // if n == 0 make sure and use the "<value>" class with a dynamic method lookup
   if (!n && qc != QC_PSEUDOVALUE)
      return qore_class_private::evalPseudoMethod(QC_PSEUDOVALUE, n, method->getName(), args, xsink);
   else
      return qore_class_private::evalPseudoMethod(qc, method, variant, n, args, xsink);
}

int64 MethodCallNode::bigIntExecPseudo(const AbstractQoreNode *n, ExceptionSink *xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant=%p\n", qc->getName(), method->getName(), variant);
   // if n == 0 make sure and use the "<value>" class with a dynamic method lookup
   if (!n && qc != QC_PSEUDOVALUE)
      return qore_class_private::bigIntEvalPseudoMethod(QC_PSEUDOVALUE, n, method->getName(), args, xsink);
   else
      return qore_class_private::bigIntEvalPseudoMethod(qc, method, variant, n, args, xsink);
}

int MethodCallNode::intExecPseudo(const AbstractQoreNode *n, ExceptionSink *xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant=%p\n", qc->getName(), method->getName(), variant);
   // if n == 0 make sure and use the "<value>" class with a dynamic method lookup
   if (!n && qc != QC_PSEUDOVALUE)
      return qore_class_private::intEvalPseudoMethod(QC_PSEUDOVALUE, n, method->getName(), args, xsink);
   else
      return qore_class_private::intEvalPseudoMethod(qc, method, variant, n, args, xsink);
}

bool MethodCallNode::boolExecPseudo(const AbstractQoreNode *n, ExceptionSink *xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant=%p\n", qc->getName(), method->getName(), variant);
   // if n == 0 make sure and use the "<value>" class with a dynamic method lookup
   if (!n && qc != QC_PSEUDOVALUE)
      return qore_class_private::boolEvalPseudoMethod(QC_PSEUDOVALUE, n, method->getName(), args, xsink);
   else
      return qore_class_private::boolEvalPseudoMethod(qc, method, variant, n, args, xsink);
}

double MethodCallNode::floatExecPseudo(const AbstractQoreNode *n, ExceptionSink *xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant=%p\n", qc->getName(), method->getName(), variant);
   // if n == 0 make sure and use the "<value>" class with a dynamic method lookup
   if (!n && qc != QC_PSEUDOVALUE)
      return qore_class_private::floatEvalPseudoMethod(QC_PSEUDOVALUE, n, method->getName(), args, xsink);
   else
      return qore_class_private::floatEvalPseudoMethod(qc, method, variant, n, args, xsink);
}

AbstractQoreNode *StaticMethodCallNode::makeReferenceNodeAndDeref() {
   if (args) {
      parse_error("argument given to static method call reference");
      return this;
   }

   UnresolvedStaticMethodCallReferenceNode *rv = new UnresolvedStaticMethodCallReferenceNode(takeScope());
   deref();
   return rv;
}

AbstractQoreNode *StaticMethodCallNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   bool abr = checkParseOption(PO_ALLOW_BARE_REFS);

   QoreClass* qc = qore_root_ns_private::parseFindScopedClassWithMethod(*scope, false);

   // see if this is a call to a base class method if bare refs are allowed
   // and we're parsing in a class context and the class found is in the
   // current class parse context
   if (qc)
      method = (oflag && abr && oflag->getTypeInfo()->getUniqueReturnClass()->parseCheckHierarchy(qc))
	 ? qore_class_private::parseFindAnyMethodIntern(qc, scope->getIdentifier())
	 : qc->parseFindStaticMethodTree(scope->getIdentifier());

   //printd(5, "StaticMethodCallNode::parseInitImpl() %s qc: %p method: %p %s\n", scope->ostr, qc, method, scope->getIdentifier());

   // see if a constant can be resolved
   if (!method) {
      {
	 // see if this is a function call to a function defined in a namespace
	 const QoreFunction* f = qore_root_ns_private::parseResolveFunction(*scope);
	 if (f) {
	    FunctionCallNode* fcn = new FunctionCallNode(f, takeArgs(), 0);
	    deref();
	    fcn->parseInitFinalizedCall(oflag, pflag, lvids, typeInfo);
	    return fcn;
	 }
      }

      AbstractQoreNode* n = 0;

      if (abr) {
         Var *v = qore_root_ns_private::parseFindGlobalVar(*scope);
         if (v)
            n = new GlobalVarRefNode(strdup(scope->getIdentifier()), v);
      }

      if (!n)
         n = qore_root_ns_private::parseFindConstantValue(*scope, typeInfo, false);

      if (n) {
	 CallReferenceCallNode *crcn = new CallReferenceCallNode(n->refSelf(), takeArgs());	 
	 deref();
	 return crcn->parseInit(oflag, pflag, lvids, typeInfo);
      }

      parse_error("cannot resolve call '%s()' to any reachable and callable object", scope->ostr);
      return this;
   }

   // check class capabilities against parse options
   if (qore_program_private::parseAddDomain(getProgram(), qc->getDomain())) {
      parseException("INVALID-METHOD", "class '%s' implements capabilities that are not allowed by current parse options", qc->getName());
      return this;
   }

   // we don't need to check for accessibility if the method is not static
   if (!method->isStatic()) {
      SelfFunctionCallNode *sfcn = new SelfFunctionCallNode(scope->takeName(), takeArgs(), qc);
      deref();
      sfcn->parseInit(oflag, pflag, lvids, typeInfo);
      return sfcn;
   }

   assert(method->isStatic());

   delete scope;
   scope = 0;

   if (method->parseIsPrivate()) {
      const QoreClass *cls = getParseClass();
      if (!cls || !cls->parseCheckHierarchy(qc)) {
	 parseException("PRIVATE-METHOD", "method %s::%s() is private and cannot be accessed outside of the class", qc->getName(), method->getName());
	 return this;
      }
   }

   lvids += parseArgs(oflag, pflag, method->getFunction(), typeInfo);
   return this;
}
