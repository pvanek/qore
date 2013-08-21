/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreObjectIntern.h

  Qore Programming Language

  Copyright 2003 - 2013 David Nichols

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

#ifndef _QORE_QOREOBJECTINTERN_H

#define _QORE_QOREOBJECTINTERN_H

#include <stdlib.h>
#include <assert.h>

#include <map>
#include <set>
#include <vector>

#include "intern/QoreClassIntern.h"

#define OS_OK            0
#define OS_DELETED      -1

// object access constants
#define QOA_OK           0
#define QOA_PRIV_ERROR   1
#define QOA_PUB_ERROR    2

#ifdef _QORE_CYCLE_CHECK
#define QORE_DEBUG_OBJ_REFS 0
#else
#define QORE_DEBUG_OBJ_REFS 5
#endif

#define QRO_LVL 1

class LValueHelper;

/*
  Qore internal class data is stored against the object with this data structure
  against its qore_classid_t (class ID).  In a class hierarchy, for private data
  that is actually a C++ subclass of Qore parent classes, then we save the same
  private data against the qore class IDs of the parent classes, but we set the
  flag to true, meaning that we will not delete the private data when
  the parent classes' destructors are run, but rather only when the subclass that
  actually owns data has its turn to destroy private object data.

  So basically, the second boolean flag just means - does this class ID actually
  own the private data or not - if it's false, then it does not actually own the data,
  but is compatible with the data, so parent class builtin (C++) methods will get
  passed this private data as if it belonged to this class and as if it were saved
  directly to the object in the class' constructor.

  please note that this flag is called the "virtual" flag elsewhere in the code here,
  meaning that the data only "virtually" belongs to the class
 */
typedef std::pair<AbstractPrivateData*, bool> private_pair_t;

// mapping from qore class ID to the object data
typedef std::map<qore_classid_t, private_pair_t> keymap_t;

// for objects with multiple classes, private data has to be keyed
class KeyList {
private:
   keymap_t keymap;

public:
   DLLLOCAL AbstractPrivateData* getReferencedPrivateData(qore_classid_t key) const {
      keymap_t::const_iterator i = keymap.find(key);
      if (i == keymap.end())
	 return 0;

      AbstractPrivateData* apd = i->second.first;
      apd->ref();
      return apd;
   }

   DLLLOCAL void addToString(QoreString* str) const {
      for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
	 str->sprintf("%d=<0x%p>, ", i->first, i->second.first);
   }

   DLLLOCAL void derefAll(ExceptionSink* xsink) const {
      for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
	 if (!i->second.second)
	    i->second.first->deref(xsink);
   }

   DLLLOCAL AbstractPrivateData* getAndClearPtr(qore_classid_t key) {
      keymap_t::iterator i = keymap.find(key);
      if (i == keymap.end())
	 return 0;

      assert(!i->second.second);
      AbstractPrivateData* rv = i->second.first;
      keymap.erase(i);
      return rv;
   }

   DLLLOCAL void insert(qore_classid_t key, AbstractPrivateData* pd) {
      assert(pd);
      keymap.insert(std::make_pair(key, std::make_pair(pd, false)));
   }

   DLLLOCAL void insertVirtual(qore_classid_t key, AbstractPrivateData* pd) {
      assert(pd);
      if (keymap.find(key) == keymap.end())
	 keymap.insert(std::make_pair(key, std::make_pair(pd, true)));
   }
};

#define ORS_NO_MATCH   -1
#define ORS_LOCK_ERROR -2
#define ORS_IGNORE     -3

/* Qore object recursive reference handling works as follows: objects are sorted into sets making up
   directed cyclic graphs.

   The objects go out of scope when all nodes have references = the number of recursive references.

   If any one member still has valid references (meaning a reference count > # of recursive refs), then *none*
   of the members of the graph can be dereferenced.
 */

// set of objects in recursive directed graph
class ObjectRSet : public obj_set_t {
protected:
   int acnt;

public:
   DLLLOCAL ObjectRSet() : acnt(0) {
   }

   DLLLOCAL ~ObjectRSet() {
      //printd(5, "ObjectRSet::~ObjectRSet() this: %p (acnt: %d)\n", this, acnt);
      assert(!acnt);
   }

   DLLLOCAL void deref() {
      //printd(5, "ObjectRSet::deref() this: %p %d -> %d\n", this, acnt, acnt - 1);
      assert(acnt > 0);
      if (!--acnt)
         delete this;
   }

   DLLLOCAL void ref() {
      ++acnt;
   }

   DLLLOCAL bool canDelete() const;

   DLLLOCAL bool assigned() const {
      return (bool)acnt;
   }
};

typedef std::vector<QoreObject*> obj_vec_t;

class ObjectRSetHelper {
protected:
   typedef std::map<QoreObject*, bool> omap_t;
   // map of all objects scanned to status (true = finalized, false = not finalized, in current list)
   omap_t fomap;

   typedef std::vector<omap_t::iterator> ovec_t;
   // current objects being scanned, used to establish a cycle
   ovec_t ovec;

   // list of fomap iterators to current recursive objects found
   ovec_t frvec;
   
   // set of objects to be rescanned since they were removed from existing recursive graphs
   obj_set_t rescan_set;

   DLLLOCAL void reset();
   DLLLOCAL int checkIntern(QoreObject& obj);
   DLLLOCAL int checkIntern(AbstractQoreNode* n);

public:
   DLLLOCAL ObjectRSetHelper(QoreObject& obj);

   DLLLOCAL ~ObjectRSetHelper() {
      assert(ovec.empty());
      assert(frvec.empty());      
   }
};

class qore_object_private {
public:
   const QoreClass* theclass;
   int status;
   mutable QoreRWLock rwl;
   // used for references, to ensure that assignments will not deadlock when the object is locked for update
   mutable QoreThreadLock ref_mutex;
   QoreThreadLock rmutex;
   KeyList* privateData;
   QoreReferenceCounter tRefs;  // reference-references
   QoreHashNode* data;
   QoreProgram* pgm;
   bool system_object, delete_blocker_run, in_destructor, pgm_ref, recursive_ref_found, is_recursive;
   int in_rsection, rcount;
   // set of objects in a cyclic directed graph
   ObjectRSet* rset;
   QoreObject* obj;

   DLLLOCAL qore_object_private(QoreObject* n_obj, const QoreClass *oc, QoreProgram* p, QoreHashNode* n_data) : 
      theclass(oc), status(OS_OK), 
      privateData(0), data(n_data), pgm(p), system_object(!p), 
      delete_blocker_run(false), in_destructor(false), pgm_ref(true), recursive_ref_found(false), is_recursive(false), in_rsection(0), rcount(0), rset(0),
      obj(n_obj) {

#ifdef QORE_DEBUG_OBJ_REFS
      printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::qore_object_private() obj=%p, pgm=%p, class=%s, references 0->1\n", obj, p, oc->getName());
#endif
      /* instead of referencing the class, we reference the program, because the
	 program contains the namespace that contains the class, and the class'
	 methods may call functions in the program as well that could otherwise
	 disappear when the program is deleted
      */
      if (p) {
	 printd(5, "qore_object_private::qore_object_private() obj=%p (%s) calling QoreProgram::ref() (%p)\n", obj, theclass->getName(), p);
	 //p->depRef();
         p->ref();
      }
   }

   DLLLOCAL ~qore_object_private() {
      assert(!pgm);
      assert(!data);
      assert(!privateData);
      assert(!rset);
   }

   DLLLOCAL void plusEquals(const AbstractQoreNode* v, AutoVLock& vl, ExceptionSink* xsink) {
      if (!v)
         return;

      // do not need ensure_unique() for objects
      if (v->getType() == NT_OBJECT) {
         ReferenceHolder<QoreHashNode> h(const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(v))->copyData(xsink), xsink);
         if (h)
            merge(*h, vl, xsink);
      }
      else if (v->getType() == NT_HASH)
         merge(reinterpret_cast<const QoreHashNode* >(v), vl, xsink);
   }

   DLLLOCAL void merge(const QoreHashNode* h, AutoVLock& vl, ExceptionSink* xsink);

   DLLLOCAL int getLValue(const char* key, LValueHelper& lvh, bool internal, bool for_remove, ExceptionSink* xsink) const;

   DLLLOCAL AbstractQoreNode* *getMemberValuePtr(const char* key, AutoVLock *vl, const QoreTypeInfo*& typeInfo, ExceptionSink* xsink) const;

   DLLLOCAL QoreStringNode* firstKey(ExceptionSink* xsink) {
      QoreAutoRWReadLocker al(rwl);

      if (status == OS_DELETED) {
         makeAccessDeletedObjectException(xsink, theclass->getName());
         return 0;
      }

      if (qore_class_private::runtimeCheckPrivateClassAccess(*theclass)) {
         const char* str = data->getFirstKey();
         printd(0, "qore_object_private::firstKey() got %p (%s)\n", str, str ? str : "<null>");
         return !str ? 0 : new QoreStringNode(str);
      }

      // get first public member
      ConstHashIterator hi(data);
      while (hi.next()) {
         //printd(5, "qore_object_private::firstKey() checking '%s'\n", hi.getKey());
         if (!checkMemberAccessIntern(hi.getKey(), false, false))
            return new QoreStringNode(hi.getKey());
         //printd(5, "qore_object_private::firstKey() skipping '%s' (private)\n", hi.getKey());
      }

      return 0;
   }

   DLLLOCAL QoreStringNode* lastKey(ExceptionSink* xsink) {
      QoreAutoRWReadLocker al(rwl);

      if (status == OS_DELETED) {
         makeAccessDeletedObjectException(xsink, theclass->getName());
         return 0;
      }

      if (qore_class_private::runtimeCheckPrivateClassAccess(*theclass)) {
         const char* str = data->getLastKey();
         return !str ? 0 : new QoreStringNode(str);
      }

      // get first public member
      ReverseConstHashIterator hi(data);
      while (hi.next()) {
         if (!checkMemberAccessIntern(hi.getKey(), false, false))
            return new QoreStringNode(hi.getKey());
      }

      return 0;
   }

   DLLLOCAL QoreHashNode* getSlice(const QoreListNode* l, ExceptionSink* xsink) const {
      QoreSafeRWReadLocker sl(rwl);

      if (status == OS_DELETED) {
	 makeAccessDeletedObjectException(xsink, theclass->getName());
	 return 0;
      }

      bool has_public_members = theclass->runtimeHasPublicMembersInHierarchy();
      bool private_access_ok = qore_class_private::runtimeCheckPrivateClassAccess(*theclass);

      // check key list if necessary
      if (has_public_members || !private_access_ok) {
	 ReferenceHolder<QoreListNode> nl(new QoreListNode, xsink);
	 ReferenceHolder<QoreListNode> mgl(theclass->hasMemberGate() ? new QoreListNode : 0, xsink);

	 ConstListIterator li(l);
	 while (li.next()) {
	    QoreStringValueHelper key(li.getValue(), QCS_DEFAULT, xsink);
	    if (*xsink)
	       return 0;

	    int rc = checkMemberAccessIntern(key->getBuffer(), has_public_members, private_access_ok);
	    if (!rc)
	       nl->push(new QoreStringNode(*key));
	    else {
	       if (theclass->hasMemberGate())
	          mgl->push(new QoreStringNode(*key));
	       else if (rc == QOA_PUB_ERROR) {
	          doPublicException(key->getBuffer(), xsink);
	          return 0;
	       }
	       else {
	          doPrivateException(key->getBuffer(), xsink);
	          return 0;
	       }
	    }
	 }

	 ReferenceHolder<QoreHashNode> rv(data->getSlice(*nl, xsink), xsink);
	 if (*xsink)
	    return 0;
	 if (mgl && !mgl->empty()) {
	    // unlock lock and execute memberGate() method for each method in the memger gate list (mgl)
	    sl.unlock();

	    ConstListIterator mgli(*mgl);
	    while (mgli.next()) {
	       const QoreStringNode* k = reinterpret_cast<const QoreStringNode*>(mgli.getValue());
	       AbstractQoreNode* n = theclass->evalMemberGate(obj, k, xsink);
	       if (*xsink)
	          return 0;
	       rv->setKeyValue(k->getBuffer(), n, xsink);
	    }
	 }
	 return rv.release();
      }

      return data->getSlice(l, xsink);
   }

   DLLLOCAL int checkMemberAccessIntern(const char* mem, bool has_public_members, bool private_access_ok) const {
      // check public access
      if (has_public_members) {
	 bool priv_member;
	 if (!theclass->isPublicOrPrivateMember(mem, priv_member))
	    return QOA_PUB_ERROR;

	 if (priv_member && !private_access_ok)
	    return QOA_PRIV_ERROR;

	 return QOA_OK;
      }

      // if accessed outside the class and the member is a private member
      return (!private_access_ok && theclass->isPrivateMember(mem)) ? QOA_PRIV_ERROR : QOA_OK;
   }

   DLLLOCAL int checkMemberAccess(const char* mem) const {
      // check public access
      if (theclass->runtimeHasPublicMembersInHierarchy()) {
	 bool priv_member;
         const qore_class_private* qc = qore_class_private::isPublicOrPrivateMember(*theclass, mem, priv_member);
	 if (!qc)
	    return QOA_PUB_ERROR;

	 if (priv_member && !qc->runtimeCheckPrivateClassAccess())
	    return QOA_PRIV_ERROR;

	 return QOA_OK;
      }

      // if accessed outside the class and the member is a private member
      return (!qore_class_private::runtimeCheckPrivateClassAccess(*theclass) && theclass->isPrivateMember(mem)) ? QOA_PRIV_ERROR : QOA_OK;
   }

   DLLLOCAL int checkMemberAccess(const char* mem, ExceptionSink* xsink) const {
      int rc = checkMemberAccess(mem);
      if (!rc)
	 return 0;

      if (rc == QOA_PRIV_ERROR)
	 doPrivateException(mem, xsink);
      else
	 doPublicException(mem, xsink);
      return -1;
   }

   DLLLOCAL int checkMemberAccessGetTypeInfo(ExceptionSink* xsink, const char* mem, const QoreTypeInfo*& typeInfo, bool check_access = true) const {
      bool priv;
      if (theclass->runtimeGetMemberInfo(mem, typeInfo, priv)) {
	 if (priv && check_access && !qore_class_private::runtimeCheckPrivateClassAccess(*theclass)) {
	    doPrivateException(mem, xsink);
	    return -1;
	 }

	 return 0;
      }

      // member is not declared
      if (theclass->runtimeHasPublicMembersInHierarchy()) {
	 doPublicException(mem, xsink);
	 return -1;
      }
      return 0;
   }

   DLLLOCAL AbstractQoreNode* takeMember(ExceptionSink* xsink, const char* mem, bool check_access = true);

   // lock not held on entry
   DLLLOCAL void doDeleteIntern(ExceptionSink* xsink) {
      printd(5, "qore_object_private::doDeleteIntern() execing destructor() obj=%p\n", obj);

      // increment reference count temporarily for destructor
      {
	 AutoLocker slr(ref_mutex);
	 ++obj->references;
      }

      theclass->execDestructor(obj, xsink);

      QoreHashNode* td;
      {
         QoreAutoRWWriteLocker al(rwl);
	 assert(status != OS_DELETED);
	 assert(data);
	 status = OS_DELETED;
	 td = data;
	 data = 0;
      }
      cleanup(xsink, td);

      if (rset) {
         rset->deref();
         rset = 0;
      }

      obj->deref(xsink);
   }

   DLLLOCAL void cleanup(ExceptionSink* xsink, QoreHashNode* td) {
      if (privateData) {
	 delete privateData;
#ifdef DEBUG
	 privateData = 0;
#endif
      }

      {
         QoreAutoRWWriteLocker al(rwl);

         if (pgm) {
            if (pgm_ref) {
               printd(5, "qore_object_private::cleanup() obj=%p (%s) calling QoreProgram::depDeref() (%p)\n", obj, theclass->getName(), pgm);
               //pgm->depDeref(xsink);
               pgm->deref(xsink);
            }
            pgm = 0;
         }
      }

      td->deref(xsink);
   }

   DLLLOCAL void derefProgramCycle(QoreProgram* p) {
      QoreAutoRWWriteLocker al(rwl);

      if (pgm && pgm_ref) {
         //pgm->depDeref(0);
         pgm->deref(0);
         pgm_ref = 0;
      }
   }

   // this method is called when there is an exception in a constructor and the object should be deleted
   DLLLOCAL void obliterate(ExceptionSink* xsink) {
      printd(5, "qore_object_private::obliterate() obj=%p class=%s %d->%d\n", obj, theclass->getName(), obj->references, obj->references - 1);

#ifdef QORE_DEBUG_OBJ_REFS
      printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::obliterate() obj=%p class=%s: references %d->%d\n", obj, theclass->getName(), obj->references, obj->references - 1);
#endif

      {
	 AutoLocker slr(ref_mutex);
	 if (--obj->references)
	    return;
      }

      {
         QoreSafeRWWriteLocker sl(rwl);

	 if (in_destructor || status != OS_OK) {
	    printd(5, "qore_object_private::obliterate() obj=%p data=%p in_destructor=%d status=%d\n", obj, data, in_destructor, status);
	    //printd(0, "Object lock %p unlocked (safe)\n", &rwl);
	    sl.unlock();
	    tDeref();
	    return;
	 }

	 //printd(5, "Object lock %p locked   (safe)\n", &rwl);
	 printd(5, "qore_object_private::obliterate() obj=%p class=%s\n", obj, theclass->getName());

	 status = OS_DELETED;
	 QoreHashNode* td = data;
	 data = 0;
	 //printd(0, "Object lock %p unlocked (safe)\n", &rwl);
	 sl.unlock();

         if (rset) {            
            rset->deref();
            rset = 0;
         }

	 if (privateData)
	    privateData->derefAll(xsink);

	 cleanup(xsink, td);
      }
      tDeref();
   }

   DLLLOCAL void doPrivateException(const char* mem, ExceptionSink* xsink) const {
      xsink->raiseException("PRIVATE-MEMBER", "'%s' is a private member of class '%s'", mem, theclass->getName());
   }

   DLLLOCAL void doPublicException(const char* mem, ExceptionSink* xsink) const {
      xsink->raiseException("INVALID-MEMBER", "'%s' is not a registered member of class '%s'", mem, theclass->getName());
   }

   DLLLOCAL void tRef() const {
#ifdef QORE_DEBUG_OBJ_REFS
      printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::tRef() obj=%p class=%s: tref %d->%d\n", obj, theclass->getName(), tRefs.reference_count(), tRefs.reference_count() + 1);
#endif
      tRefs.ROreference();
   }

   DLLLOCAL void tDeref() {
#ifdef QORE_DEBUG_OBJ_REFS
      printd(QORE_DEBUG_OBJ_REFS, "qore_object_private::tDeref() obj=%p class=%s: tref %d->%d\n", obj, status == OS_OK ? theclass->getName() : "<deleted>", tRefs.reference_count(), tRefs.reference_count() - 1);
#endif
      if (tRefs.ROdereference())
	 delete obj;
   }

   // add virtual IDs for private data to class list
   DLLLOCAL void addVirtualPrivateData(qore_classid_t key, AbstractPrivateData* apd) {
      // first get parent class corresponding to "key"
      QoreClass *qc = theclass->getClass(key);
      //printd(5, "qore_object_private::addVirtualPrivateData() this: %p key: %d apd: %p qc: %p\n", this, key, apd, qc);
      assert(qc);
      BCSMList *sml = qc->getBCSMList();
      //printd(5, "qore_object_private::addVirtualPrivateData() this: %p qc: %p '%s' sml: %p\n", this, qc, qc->getName(), sml);
      if (!sml)
	 return;

      for (class_list_t::const_iterator i = sml->begin(), e = sml->end(); i != e; ++i) {
         //printd(5, "qore_object_private::addVirtualPrivateData() this: %p i: %p '%s' %s\n", this, i->first, i->first->getName(), i->second ? "true" : "false");
	 if ((*i).second)
	    privateData->insertVirtual((*i).first->getID(), apd);
      }
   }

   DLLLOCAL int enterRSection() {
      int tid = gettid();
      
      {
         AutoLocker al(rmutex);
         if (!in_rsection) {
            if (rwl.tryrdlock())
               return -1;

            in_rsection = tid;
         }
         else if (in_rsection != tid) {
            rwl.unlock();
            return -1;
         }
      }

      return 0;
   }

   DLLLOCAL void exitRSection() {
      // first unset the in_rsection flag
      {
         AutoLocker al(rmutex);
         assert(in_rsection == gettid());
         in_rsection = 0;
      }
      
      // now release the read lock
      rwl.unlock();
   }

   DLLLOCAL void setRSet(ObjectRSet* rs) {
      assert(!rset);
      rset = rs;
      rs->ref();
   }

   DLLLOCAL void clearRSet() {
      if (rset) {
         rset->deref();
         rset = 0;
      }
   }

   DLLLOCAL static bool isRecursive(QoreObject& obj) {
      return obj.priv->is_recursive;
   }

   DLLLOCAL static void setRecursive(QoreObject& obj) {
      if (!obj.priv->is_recursive) {
         obj.priv->is_recursive = true;
         printd(0, "qore_object_private::setRecursive() obj: %p '%s'\n", &obj, obj.priv->theclass->getName());
      }
   }

   DLLLOCAL static AbstractQoreNode* takeMember(QoreObject& obj, ExceptionSink* xsink, const char* mem, bool check_access = true) {
      return obj.priv->takeMember(xsink, mem, check_access);
   }

   DLLLOCAL static int getLValue(const QoreObject& obj, const char* key, LValueHelper& lvh, bool internal, bool for_remove, ExceptionSink* xsink) {
      return obj.priv->getLValue(key, lvh, internal, for_remove, xsink);
   }

   DLLLOCAL static AbstractQoreNode* *getMemberValuePtr(const QoreObject* obj, const char* key, AutoVLock *vl, const QoreTypeInfo*& typeInfo, ExceptionSink* xsink) {
      return obj->priv->getMemberValuePtr(key, vl, typeInfo, xsink);
   }

   DLLLOCAL static void plusEquals(QoreObject* obj, const AbstractQoreNode* v, AutoVLock& vl, ExceptionSink* xsink) {
      obj->priv->plusEquals(v, vl, xsink);
   }

   DLLLOCAL static void derefProgramCycle(QoreObject* obj, QoreProgram* p) {
      obj->priv->derefProgramCycle(p);
   }

   DLLLOCAL static QoreStringNode* firstKey(QoreObject* obj, ExceptionSink* xsink) {
      return obj->priv->firstKey(xsink);
   }

   DLLLOCAL static QoreStringNode* lastKey(QoreObject* obj, ExceptionSink* xsink) {
      return obj->priv->lastKey(xsink);
   }
};

class qore_object_lock_handoff_helper {
private:
   qore_object_private* pobj;
   AutoVLock& vl;

public:
   DLLLOCAL qore_object_lock_handoff_helper(qore_object_private* n_pobj, AutoVLock& n_vl) : pobj(n_pobj), vl(n_vl) {
      if (pobj->obj == vl.getObject()) {
	 assert(vl.getRWL() == &pobj->rwl);
	 vl.clear();
	 return;
      }

      // reference current object
      pobj->obj->tRef();

      // unlock previous lock and release from AutoVLock structure
      vl.del();

      // lock current object
      pobj->rwl.wrlock();
   }

   DLLLOCAL ~qore_object_lock_handoff_helper() {
      // unlock if lock not saved in AutoVLock structure
      if (pobj) {
	 //printd(0, "Object lock %p unlocked (handoff)\n", &pobj->rwl);
	 pobj->rwl.unlock();
	 pobj->obj->tDeref();
      }
   }

   DLLLOCAL void stay_locked() {
      vl.set(pobj->obj, &pobj->rwl);
      pobj = 0;
   }
};

class qore_object_recursive_lock_handoff_helper {
private:
   qore_object_private* pobj;
   bool locked;

public:
   DLLLOCAL qore_object_recursive_lock_handoff_helper(qore_object_private* n_pobj, AutoVLock& n_vl) : pobj(n_pobj) /*, vl(n_vl)*/ {
      // try to lock current object
      locked = !pobj->rwl.trywrlock();
   }

   DLLLOCAL ~qore_object_recursive_lock_handoff_helper() {
      // unlock current object
      if (locked)
         pobj->rwl.unlock();
   }

   DLLLOCAL operator bool() const {
      return locked;
   }
};

#endif
