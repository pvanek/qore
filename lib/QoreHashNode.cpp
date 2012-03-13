/*
  QoreHashNode.cpp

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
#include <qore/minitest.hpp>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/QoreNamespaceIntern.h>
#include <qore/intern/ParserSupport.h>

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <map>

#ifdef DEBUG_TESTS
#  include "tests/Hash_tests.cpp"
#endif

static const char *qore_hash_type_name = "hash";

QoreHashNode::QoreHashNode(bool ne) : AbstractQoreNode(NT_HASH, !ne, ne), priv(new qore_hash_private) {
}

QoreHashNode::QoreHashNode() : AbstractQoreNode(NT_HASH, true, false), priv(new qore_hash_private) { 
}

QoreHashNode::~QoreHashNode() {
   delete priv;
}

AbstractQoreNode *QoreHashNode::realCopy() const {
   return copy();
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL virtual int compare(const AbstractQoreNode *val) const;
// the type passed must always be equal to the current type
bool QoreHashNode::is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   if (!v || v->getType() != NT_HASH)
      return false;

   return !compareSoft(reinterpret_cast<const QoreHashNode *>(v), xsink);
}

bool QoreHashNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   if (!v || v->getType() != NT_HASH)
      return false;

   return !compareHard(reinterpret_cast<const QoreHashNode *>(v), xsink);
}

const char *QoreHashNode::getTypeName() const {
   return qore_hash_type_name;
}

const char *QoreHashNode::getFirstKey() const  { 
   return priv->member_list ? priv->member_list->key :0; 
}

const char *QoreHashNode::getLastKey() const {
   return priv->tail ? priv->tail->key : 0; 
}

// deprecated
AbstractQoreNode **QoreHashNode::getKeyValuePtr(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;
   
   return priv->getKeyValuePtr(tmp->getBuffer());
}

// deprecated
AbstractQoreNode **QoreHashNode::getKeyValuePtr(const char *key) {
   return priv->getKeyValuePtr(key);
}

int64 QoreHashNode::getKeyAsBigInt(const char *key, bool &found) const {
   return priv->getKeyAsBigInt(key, found);
}

bool QoreHashNode::getKeyAsBool(const char *key, bool &found) const {
   return priv->getKeyAsBool(key, found);
}

void QoreHashNode::deleteKey(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   priv->deleteKey(tmp->getBuffer(), xsink);
}

void QoreHashNode::removeKey(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   priv->removeKey(tmp->getBuffer(), xsink);
}

AbstractQoreNode *QoreHashNode::takeKeyValue(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   return priv->takeKeyValue(tmp->getBuffer());
}

AbstractQoreNode *QoreHashNode::getKeyValueExistence(const QoreString *key, bool &exists, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   return getKeyValueExistence(tmp->getBuffer(), exists);
}

const AbstractQoreNode *QoreHashNode::getKeyValueExistence(const QoreString *key, bool &exists, ExceptionSink *xsink) const {
   return const_cast<QoreHashNode *>(this)->getKeyValueExistence(key, exists, xsink);
}

void QoreHashNode::setKeyValue(const QoreString *key, AbstractQoreNode *val, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink) {
      val->deref(xsink);
      return;
   }

   setKeyValue(tmp->getBuffer(), val, xsink);
}

void QoreHashNode::setKeyValue(const QoreString &key, AbstractQoreNode *val, ExceptionSink *xsink) {
   setKeyValue(&key, val, xsink);
}

void QoreHashNode::setKeyValue(const char *key, AbstractQoreNode *val, ExceptionSink *xsink) {
   hash_assignment_priv ha(*priv, key);

   ha.assign(val, xsink);
}

AbstractQoreNode *QoreHashNode::swapKeyValue(const QoreString *key, AbstractQoreNode *val, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink) {
      val->deref(xsink);
      return 0;
   }

   hash_assignment_priv ha(*priv, tmp->getBuffer());
   return ha.swap(val, xsink);
}

AbstractQoreNode *QoreHashNode::swapKeyValue(const char *key, AbstractQoreNode *val) {
   //printd(0, "QoreHashNode::swapKeyValue() this=%p key=%s val=%p (%s) deprecated API called\n", this, key, val, get_node_type(val));
   //assert(false);
   hash_assignment_priv ha(*priv, key);
   return ha.swap(val, 0);
}

AbstractQoreNode *QoreHashNode::swapKeyValue(const char *key, AbstractQoreNode *val, ExceptionSink *xsink) {
   hash_assignment_priv ha(*priv, key);
   return ha.swap(val, xsink);
}

// deprecated
AbstractQoreNode **QoreHashNode::getExistingValuePtr(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   return getExistingValuePtr(tmp->getBuffer());
}

AbstractQoreNode *QoreHashNode::getKeyValue(const QoreString *key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   return getKeyValue(tmp->getBuffer());
}

AbstractQoreNode *QoreHashNode::getKeyValue(const QoreString &key, ExceptionSink *xsink) {
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   return getKeyValue(tmp->getBuffer());
}

const AbstractQoreNode *QoreHashNode::getKeyValue(const QoreString *key, ExceptionSink *xsink) const {
   return const_cast<QoreHashNode *>(this)->getKeyValue(key, xsink);
}

// retrieve keys in order they were inserted
QoreListNode *QoreHashNode::getKeys() const {
   QoreListNode *list = new QoreListNode;
   HashMember *where = priv->member_list;
   
   while (where) {
      list->push(new QoreStringNode(where->key));
      where = where->next;
   }
   return list;
}

// adds all elements (and references them) from the hash passed, leaves the
// hash passed alone
// order is maintained
void QoreHashNode::merge(const class QoreHashNode *h, ExceptionSink *xsink) {
   HashMember *where = h->priv->member_list;
   
   while (where) {
      setKeyValue(where->key, where->node ? where->node->refSelf() : 0, xsink);
      where = where->next;
   }
}

// returns the same order
QoreHashNode *QoreHashNode::copy() const {
   QoreHashNode *h = new QoreHashNode();

   // copy all members to new object
   class HashMember *where = priv->member_list;
   while (where) {
      //printd(5, "QoreHashNode::copy() this=%p node=%p key='%s'\n", this, where->node, where->key);
      h->setKeyValue(where->key, where->node ? where->node->refSelf() : 0, 0);
      where = where->next;
   }
   return h;
}

QoreHashNode *QoreHashNode::hashRefSelf() const {
   ref();
   return const_cast<QoreHashNode *>(this);
}

// returns a hash with the same order
AbstractQoreNode *QoreHashNode::evalImpl(ExceptionSink *xsink) const {
   QoreHashNodeHolder h(new QoreHashNode(), xsink);

   HashMember *where = priv->member_list;
   while (where) {
      h->setKeyValue(where->key, where->node ? where->node->eval(xsink) : 0, xsink);
      if (*xsink)
         return 0;
      where = where->next;
   }

   return h.release();
}

// returns a hash with the same order
AbstractQoreNode *QoreHashNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   if (value) {
      needs_deref = false;
      return const_cast<QoreHashNode *>(this);
   }

   needs_deref = true;
   return QoreHashNode::evalImpl(xsink);
}

int64 QoreHashNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

int QoreHashNode::integerEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

bool QoreHashNode::boolEvalImpl(ExceptionSink *xsink) const {
   return false;
}

double QoreHashNode::floatEvalImpl(ExceptionSink *xsink) const {
   return 0.0;
}

AbstractQoreNode *QoreHashNode::evalKeyValue(const QoreString *key, ExceptionSink *xsink) const {
   TempEncodingHelper k(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return 0;

   hm_hm_t::const_iterator i = priv->hm.find(k->getBuffer());

   if (i != priv->hm.end() && i->second->node)
      return i->second->node->refSelf();

   return 0;
}

AbstractQoreNode *QoreHashNode::getReferencedKeyValue(const char *key) const {
   assert(key);

   hm_hm_t::const_iterator i = priv->hm.find(key);

   if (i != priv->hm.end() && i->second->node)
      return i->second->node->refSelf();

   return 0;
}

AbstractQoreNode *QoreHashNode::getReferencedKeyValue(const char *key, bool &exists) const {
   assert(key);

   hm_hm_t::const_iterator i = priv->hm.find(key);

   if (i != priv->hm.end()) {
      exists = true;
      if (i->second->node)
	 return i->second->node->refSelf();
      
      return 0;
   }
   exists = false;
   return 0;
}

AbstractQoreNode *QoreHashNode::getKeyValue(const char *key) {
   assert(key);

   hm_hm_t::const_iterator i = priv->hm.find(key);

   if (i != priv->hm.end())
      return i->second->node;

   return 0;
}

const AbstractQoreNode *QoreHashNode::getKeyValue(const char *key) const {
   return const_cast<QoreHashNode *>(this)->getKeyValue(key);
}

AbstractQoreNode *QoreHashNode::getKeyValueExistence(const char *key, bool &exists) {
   assert(key);

   hm_hm_t::const_iterator i = priv->hm.find(key);

   if (i != priv->hm.end()) {
      exists = true;
      return i->second->node;
   }

   exists = false;
   return 0;
}

const AbstractQoreNode *QoreHashNode::getKeyValueExistence(const char *key, bool &exists) const {
   return const_cast<QoreHashNode *>(this)->getKeyValueExistence(key, exists);
}

// does a "soft" compare (values of different types are converted if necessary and then compared)
// 0 = equal, 1 = not equal
bool QoreHashNode::compareSoft(const QoreHashNode *h, ExceptionSink *xsink) const {
   if (h->priv->len != priv->len)
      return 1;

   ConstHashIterator hi(this);
   while (hi.next()) {
      hm_hm_t::const_iterator j = h->priv->hm.find(hi.getKey());
      if (j == h->priv->hm.end())
         return 1;

      if (::compareSoft(hi.getValue(), j->second->node, xsink))
         return 1;
   }
/*
   for (hm_hm_t::const_iterator i = priv->hm.begin(); i != priv->hm.end(); i++) {
      hm_hm_t::const_iterator j = h->priv->hm.find(i->first);
      if (j == h->priv->hm.end())
         return 1;

      if (::compareSoft(i->second->node, j->second->node, xsink))
         return 1;
   }
*/
   return 0;
}

// does a "hard" compare (types must be exactly the same)
// 0 = equal, 1 = not equal
bool QoreHashNode::compareHard(const QoreHashNode *h, ExceptionSink *xsink) const {
   if (h->priv->len != priv->len)
      return 1;

   ConstHashIterator hi(this);
   while (hi.next()) {
      hm_hm_t::const_iterator j = h->priv->hm.find(hi.getKey());
      if (j == h->priv->hm.end())
         return 1;
      
      if (::compareHard(hi.getValue(), j->second->node, xsink))
         return 1;
   }
   return 0;
}

// deprecated
AbstractQoreNode **QoreHashNode::getExistingValuePtr(const char *key) {
   hm_hm_t::const_iterator i = priv->hm.find(key);

   if (i != priv->hm.end())
      return &i->second->node;
   
   return 0;
}

bool QoreHashNode::derefImpl(ExceptionSink *xsink) {
   //printd(5, "QoreHashNode::derefImpl() this=%p priv->member_list=%p\n", this, priv->member_list);
   HashMember *where = priv->member_list;
   while (where) {
#if 0
      printd(5, "QoreHashNode::derefImpl() this=%p %s=%p type=%s references=%d\n", this,
             where->key ? where->key : "(null)",
             where->node, where->node ? where->node->getTypeName() : "(null)",
             where->node ? where->node->reference_count() : 0);
#endif
      if (where->node)
         where->node->deref(xsink);
      HashMember *om = where;
      where = where->next;
      delete om;
   }
#ifdef DEBUG
   priv->member_list = 0;
#endif
   return true;
}

void QoreHashNode::clear(ExceptionSink *xsink) {
   assert(is_unique());
   derefImpl(xsink);
   priv->member_list = 0;
   priv->tail = 0;
   priv->hm.clear();
}

void QoreHashNode::deleteKey(const char *key, ExceptionSink *xsink) {
   priv->deleteKey(key, xsink);
}

void QoreHashNode::removeKey(const char *key, ExceptionSink *xsink) {
   return priv->removeKey(key, xsink);
}

AbstractQoreNode *QoreHashNode::takeKeyValue(const char *key) {
   return priv->takeKeyValue(key);
}

qore_size_t QoreHashNode::size() const { 
   return priv->len; 
}

bool QoreHashNode::empty() const {
   return !priv->len;
}

bool QoreHashNode::existsKey(const char *key) const {
   return priv->existsKey(key);
}

bool QoreHashNode::existsKeyValue(const char *key) const {
   return priv->existsKeyValue(key);   
}

void QoreHashNode::clearNeedsEval() {
   value = true;
   needs_eval_flag = false;
}

void QoreHashNode::setNeedsEval() {
   value = false;
   needs_eval_flag = true;
}

int QoreHashNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   QoreContainerHelper cch(this);
   if (!cch) {
      str.sprintf("{ERROR: recursive reference to hash %p}", this);
      return 0;
   }

   if (foff == FMT_YAML_SHORT) {
      str.concat('{');
      ConstHashIterator hi(this);
      while (hi.next()) {
	 str.sprintf("%s: ", hi.getKey());
	 const AbstractQoreNode *n = hi.getValue();
	 if (!n) n = &Nothing;
	 if (n->getAsString(str, foff, xsink))
	    return -1;
	 if (!hi.last())
	    str.concat(", ");
      }
      str.concat('}');
      return 0;
   }

   if (!size()) {
      str.concat(&EmptyHashString);
      return 0;
   }
   str.concat("hash: (");

   if (foff != FMT_NONE) {
      qore_size_t elements = size();
      str.sprintf("%lu member%s)\n", elements, elements == 1 ? "" : "s");
   }
   
   ConstHashIterator hi(this);
   while (hi.next()) {
      if (foff != FMT_NONE)
         str.addch(' ', foff + 2);

      str.sprintf("%s : ", hi.getKey());

      const AbstractQoreNode *n = hi.getValue();
      if (!n) n = &Nothing;
      if (n->getAsString(str, foff != FMT_NONE ? foff + 2 : foff, xsink))
         return -1;

      if (!hi.last()) {
         if (foff != FMT_NONE)
            str.concat('\n');
         else
            str.concat(", ");
      }
   }
   
   if (foff == FMT_NONE)
      str.concat(')');

   return 0;
}

QoreString *QoreHashNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = false;
   qore_size_t elements = size();
   if (!elements && foff != FMT_YAML_SHORT)
      return &EmptyHashString;

   TempString rv(new QoreString);
   if (getAsString(*(*rv), foff, xsink))
      return 0;

   del = true;
   return rv.release();
}

QoreHashNode *QoreHashNode::getSlice(const QoreListNode *value_list, ExceptionSink *xsink) const {
   ReferenceHolder<QoreHashNode> rv(new QoreHashNode(), xsink);

   ConstListIterator li(value_list);
   while (li.next()) {
      QoreStringValueHelper key(li.getValue(), QCS_DEFAULT, xsink);
      if (*xsink)
	 return 0;

      bool exists;
      const AbstractQoreNode *v = getKeyValueExistence(key->getBuffer(), exists);
      if (!exists)
	 continue;
      rv->setKeyValue(key->getBuffer(), v ? v->refSelf() : 0, xsink);
      if (*xsink)
	 return 0;
   }
   return rv.release();
}

AbstractQoreNode *QoreHashNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   //printd(5, "QoreHashNode::parseInit() this=%p\n", this);
   typeInfo = hashTypeInfo;

   HashIterator hi(this);
   while (hi.next()) {
      const char *k = hi.getKey();
      AbstractQoreNode **val = hi.getValuePtr();
      
      printd(5, "QoreHashNode::parseInit() this=%p resolving key '%s' val %p (%s)\n", this, k, *val, get_type_name(*val));

      // resolve constant references in keys
      if (k[0] == HE_TAG_CONST || k[0] == HE_TAG_SCOPED_CONST) {
         AbstractQoreNode *rv;
         // currently type information is ignored
         const QoreTypeInfo *keyTypeInfo = 0;
         if (k[0] == HE_TAG_CONST)
	    rv = qore_root_ns_private::parseFindConstantValue(k + 1, keyTypeInfo, true);
         else {
            NamedScope *nscope = new NamedScope(strdup(k + 1));
	    rv = qore_root_ns_private::parseFindConstantValue(nscope, keyTypeInfo, true);
            delete nscope;
         }

	 //printd(5, "QoreHashNode::parseInit() resolved constant '%s': %p\n", k + 1, rv);

         if (rv) {
            QoreStringValueHelper t(rv);

            // check for duplicate key definitions
            if (priv->existsKey(t->getBuffer()))
               doDuplicateKeyWarning(t->getBuffer());

            // move value to new hash key
            setKeyValue(t->getBuffer(), *val, 0);
	    *val = 0;
         }
         
         // delete the old key (not possible to have an exception here)
         hi.deleteKey(0);
         continue;
      }

      assert(val);
      if (*val) {
         const QoreTypeInfo *argTypeInfo = 0;

	 //printd(5, "QoreHashNode::parseInit() this=%p initializing key '%s' val=%p (%s)\n", this, k, *val, get_type_name(*val));

         (*val) = (*val)->parseInit(oflag, pflag & ~PF_REFERENCE_OK, lvids, argTypeInfo);
         if (!needs_eval_flag && *val && (*val)->needs_eval()) {
            //printd(5, "setting needs_eval on hash %p key '%s' val=%p (%s)\n", this, k, *val, get_type_name(*val));
            setNeedsEval();
         }
      }
   }
   return this;
}

// static function
void QoreHashNode::doDuplicateKeyWarning(const char *key) {
   if (key[0] < 32)
      ++key;
   getProgram()->makeParseWarning(QP_WARN_DUPLICATE_HASH_KEY, "DUPLICATE-HASH-KEY", "hash key '%s' has already been given in this hash; the value given in the last occurrence will be assigned to the hash; to avoid seeing this warning, remove the extraneous key definitions or turn off the warning by using '%%disable-warning duplicate-hash-key' in your code", key);
}

HashIterator::HashIterator(QoreHashNode *qh) : h(qh), ptr(0) {
}

HashIterator::HashIterator(QoreHashNode &qh) : h(&qh), ptr(0) {
}

AbstractQoreNode *HashIterator::getReferencedValue() const {
   return ptr && ptr->node ? ptr->node->refSelf() : 0;
}

QoreString *HashIterator::getKeyString() const {
   if (!ptr)
      return 0;
   
   return new QoreString(ptr->key, QCS_DEFAULT);
}

bool HashIterator::next() { 
   if (ptr) 
      ptr = ptr->next;
   else
      ptr = h->priv->member_list;
   return ptr;
}

const char *HashIterator::getKey() const { 
   if (!ptr)
      return 0;
   
   return ptr->key;
}

AbstractQoreNode *HashIterator::getValue() const {
   if (ptr)
      return ptr->node;
   return 0;
}

AbstractQoreNode *HashIterator::takeValueAndDelete() {
   if (!ptr)
      return 0;

   AbstractQoreNode *rv = ptr->node;
   ptr->node = 0;
   HashMember *w = ptr;
   ptr = ptr->prev;

   // remove key from map before deleting hash member with key pointer
   hm_hm_t::iterator i = h->priv->hm.find(w->key);
   assert(i != h->priv->hm.end());
   h->priv->hm.erase(i);

   h->priv->internDeleteKey(w);
   return rv;
}

void HashIterator::deleteKey(ExceptionSink *xsink) {
   if (!ptr)
      return;

   discard(ptr->node, xsink);
   ptr->node = 0;
   HashMember *w = ptr;
   ptr = ptr->prev;
   
   // remove key from map before deleting hash member with key pointer
   hm_hm_t::iterator i = h->priv->hm.find(w->key);
   assert(i != h->priv->hm.end());
   h->priv->hm.erase(i);

   h->priv->internDeleteKey(w);
}

// deprecated
AbstractQoreNode **HashIterator::getValuePtr() const {
   if (ptr)
      return &(ptr->node);
   return 0;
}

bool HashIterator::last() const { 
   return (bool)(ptr ? !ptr->next : false); 
} 

bool HashIterator::first() const { 
   return (bool)(ptr ? !ptr->prev : false); 
} 

ReverseHashIterator::ReverseHashIterator(QoreHashNode *h) : HashIterator(h) {
}

ReverseHashIterator::ReverseHashIterator(QoreHashNode &h) : HashIterator(h) {
}

bool ReverseHashIterator::next() { 
   ptr = ptr ? ptr->prev : h->priv->tail;
   return ptr;
}

ConstHashIterator::ConstHashIterator(const QoreHashNode *qh) : h(qh), ptr(0) {
}

ConstHashIterator::ConstHashIterator(const QoreHashNode &qh) : h(&qh), ptr(0) {
}

AbstractQoreNode *ConstHashIterator::getReferencedValue() const {
   return ptr && ptr->node ? ptr->node->refSelf() : 0;
}

QoreString *ConstHashIterator::getKeyString() const {
   if (!ptr)
      return 0;
   
   return new QoreString(ptr->key, QCS_DEFAULT);
}

bool ConstHashIterator::next() { 
   if (ptr) 
      ptr = ptr->next;
   else
      ptr = h->priv->member_list;
   return ptr;
}

const char *ConstHashIterator::getKey() const { 
   if (!ptr)
      return 0;
   
   return ptr->key;
}

const AbstractQoreNode *ConstHashIterator::getValue() const {
   if (ptr)
      return ptr->node;
   return 0;
}

bool ConstHashIterator::last() const { 
   return (bool)(ptr ? !ptr->next : false); 
} 

bool ConstHashIterator::first() const {
   return (bool)(ptr ? !ptr->prev : false); 
} 

ReverseConstHashIterator::ReverseConstHashIterator(const QoreHashNode *h) : ConstHashIterator(h) {
}

ReverseConstHashIterator::ReverseConstHashIterator(const QoreHashNode &h) : ConstHashIterator(h) {
}

bool ReverseConstHashIterator::next() { 
   ptr = ptr ? ptr->prev : h->priv->tail;
   return ptr;
}

hash_assignment_priv::hash_assignment_priv(qore_hash_private &n_h, const char *key, bool must_already_exist) : h(n_h), om(must_already_exist ? h.findMember(key) : h.findCreateMember(key)) {
}

hash_assignment_priv::hash_assignment_priv(QoreHashNode &n_h, const char *key, bool must_already_exist) : h(*n_h.priv), om(must_already_exist ? h.findMember(key) : h.findCreateMember(key)) {
}

hash_assignment_priv::hash_assignment_priv(QoreHashNode &n_h, const std::string &key, bool must_already_exist) : h(*n_h.priv), om(must_already_exist ? h.findMember(key.c_str()) : h.findCreateMember(key.c_str())) {
}

hash_assignment_priv::hash_assignment_priv(ExceptionSink *xsink, QoreHashNode &n_h, const QoreString &key, bool must_already_exist) : h(*n_h.priv), om(0) {
   TempEncodingHelper k(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;
      
   om = must_already_exist ? h.findMember(k->getBuffer()) : h.findCreateMember(k->getBuffer());
}

hash_assignment_priv::hash_assignment_priv(ExceptionSink *xsink, QoreHashNode &n_h, const QoreString *key, bool must_already_exist) : h(*n_h.priv), om(0) {
   TempEncodingHelper k(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   om = must_already_exist ? h.findMember(k->getBuffer()) : h.findCreateMember(k->getBuffer());
}

AbstractQoreNode *hash_assignment_priv::swapImpl(AbstractQoreNode *v, ExceptionSink *xsink) {
   AbstractQoreNode *old = om->node;
   om->node = v;
   return old;
}

AbstractQoreNode *hash_assignment_priv::getValueImpl() const {
   return om->node;
}

HashAssignmentHelper::HashAssignmentHelper(QoreHashNode &h, const char *key, bool must_already_exist) : priv(new hash_assignment_priv(*h.priv, key, must_already_exist)) {
}

HashAssignmentHelper::HashAssignmentHelper(QoreHashNode &h, const std::string &key, bool must_already_exist) : priv(new hash_assignment_priv(*h.priv, key.c_str(), must_already_exist)) {
}

HashAssignmentHelper::HashAssignmentHelper(ExceptionSink *xsink, QoreHashNode &h, const QoreString &key, bool must_already_exist) : priv(0) {
   TempEncodingHelper k(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   priv = new hash_assignment_priv(*h.priv, k->getBuffer(), must_already_exist);
}

HashAssignmentHelper::HashAssignmentHelper(ExceptionSink *xsink, QoreHashNode &h, const QoreString *key, bool must_already_exist) : priv(0) {
   TempEncodingHelper k(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   priv = new hash_assignment_priv(*h.priv, k->getBuffer(), must_already_exist);
}

HashAssignmentHelper::HashAssignmentHelper(HashIterator &hi) : priv(new hash_assignment_priv(*hi.h->priv, hi.ptr)) {
}

HashAssignmentHelper::~HashAssignmentHelper() {
   delete priv;
}

HashAssignmentHelper::operator bool() const {
   return priv;
}

void HashAssignmentHelper::assign(AbstractQoreNode *v, ExceptionSink *xsink) {
   assert(priv);
   priv->assign(v, xsink);
}

AbstractQoreNode *HashAssignmentHelper::swap(AbstractQoreNode *v, ExceptionSink *xsink) {
   assert(priv);
   return priv->swap(v, xsink);
}

AbstractQoreNode *HashAssignmentHelper::operator*() const {
   assert(priv);
   return **priv;
}

