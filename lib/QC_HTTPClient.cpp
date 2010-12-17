/*
  QC_HTTPClient.cpp

  Qore Programming Language

  Copyright (C) 2006 - 2010 Qore Technologies

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
#include <qore/QoreHTTPClient.h>
#include <qore/intern/QC_HTTPClient.h>
#include <qore/intern/QC_Queue.h>
#include <qore/intern/ssl_constants.h>
#include <qore/minitest.hpp>

#ifdef DEBUG_TESTS
#  include "tests/QC_HTTPClient_tests.cpp"
#endif

qore_classid_t CID_HTTPCLIENT;

static void HC_constructor_hash(QoreObject *self, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreHashNode *n = HARD_QORE_HASH(args, 0);

   ReferenceHolder<QoreHTTPClient> client(new QoreHTTPClient(), xsink);
   if (client->setOptions(n, xsink))
      return;

   self->setPrivate(CID_HTTPCLIENT, client.release());
}

static void HC_constructor(QoreObject *self, const QoreListNode *args, ExceptionSink *xsink) {
   self->setPrivate(CID_HTTPCLIENT, new QoreHTTPClient);
}

static void HC_copy(QoreObject *self, QoreObject *old, QoreHTTPClient* client, ExceptionSink *xsink) {
   xsink->raiseException("HTTPCLIENT-COPY-ERROR", "copying HTTPClient objects is not yet supported");
}

static void HC_destructor(QoreObject *self, QoreHTTPClient *client, ExceptionSink *xsink) {
   // have to clear callbacks before destroying
   client->cleanup(xsink);
   client->deref(xsink);
}

static AbstractQoreNode *HC_setHTTPVersion(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *p = HARD_QORE_STRING(args, 0);
   client->setHTTPVersion(p->getBuffer(), xsink);
   return 0;
}

static AbstractQoreNode *HC_getHTTPVersion(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return new QoreStringNode(client->getHTTPVersion());
}

static AbstractQoreNode *HC_setSecure(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setSecure(HARD_QORE_BOOL(args, 0));
   return 0;
}

static AbstractQoreNode *HC_isSecure(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return get_bool_node(client->isSecure());
}

static AbstractQoreNode *HC_verifyPeerCertificate(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const char *c = getSSLCVCode(client->verifyPeerCertificate());
   return c ? new QoreStringNode(c) : 0;
}

static AbstractQoreNode *HC_getSSLCipherName(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const char *str = client->getSSLCipherName();
   return str ? new QoreStringNode(str) : 0;
}

static AbstractQoreNode *HC_getSSLCipherVersion(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const char *str = client->getSSLCipherVersion();
   return str ? new QoreStringNode(str) : 0;
}

static AbstractQoreNode *HC_connect(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->connect(xsink);
   return 0;
}

static AbstractQoreNode *HC_disconnect(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->disconnect();
   return 0;
}

class OptHashRefHelper {
   const ReferenceNode *ref;
   ExceptionSink *xsink;
   ReferenceHolder<QoreHashNode> info;
public:
   DLLLOCAL OptHashRefHelper(const QoreListNode *args, unsigned i, ExceptionSink *n_xsink) : ref(test_reference_param(args, i)), xsink(n_xsink), info(ref ? new QoreHashNode : 0, xsink) {
   }
   DLLLOCAL ~OptHashRefHelper() {
      if (!ref)
	 return;

      AutoVLock vl(xsink);
      QoreTypeSafeReferenceHelper rh(ref, vl, xsink);
      if (!rh)
	 return;

      rh.assign(info.release(), xsink);
   }
   DLLLOCAL QoreHashNode *operator*() {
      return *info;
   }
};

// hash send(data = "", method, path = "", headers = hash(), getbody = False, [info_reference])  
static AbstractQoreNode *HC_send(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const void *ptr = 0;
   qore_size_t size = 0;
   const AbstractQoreNode *p = get_param(args, 0);
   if (p->getType() == NT_STRING) {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p);
      assert(str);
      ptr = str->getBuffer();
      size = str->strlen();
   }
   else {
      assert(p->getType() == NT_BINARY);
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p);
      ptr = b->getPtr();
      size = b->size();
   }

   const QoreStringNode *pstr = HARD_QORE_STRING(args, 1);
   const char *meth = pstr->getBuffer();

   pstr = HARD_QORE_STRING(args, 2);
   const char *path = pstr->strlen() ? pstr->getBuffer() : 0;

   const QoreHashNode *ph = HARD_QORE_HASH(args, 3);
   bool getbody = HARD_QORE_BOOL(args, 4);

   OptHashRefHelper ohrh(args, 5, xsink);
   ReferenceHolder<AbstractQoreNode> rv(client->send(meth, path, ph, ptr, size, getbody, *ohrh, xsink), xsink);

   return *xsink ? 0 : rv.release();
}

// *string HTTPClient::get(string $path, hash $headers = hash(), *reference $ref)  
static AbstractQoreNode *HC_get(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *pstr = HARD_QORE_STRING(args, 0);
   const char *path = pstr->getBuffer();

   const QoreHashNode *ph = HARD_QORE_HASH(args, 1);

   OptHashRefHelper ohrh(args, 2, xsink);
   ReferenceHolder<AbstractQoreNode> rv(client->get(path, ph, *ohrh, xsink), xsink);

   return *xsink ? 0 : rv.release();
}

// head(string $path, hash $headers = hash())
// head(string $path, hash $headers = hash(), reference $info)
static AbstractQoreNode *HC_head(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *pstr = HARD_QORE_STRING(args, 0);
   const char *path = pstr->getBuffer();

   const QoreHashNode *ph = HARD_QORE_HASH(args, 1);

   OptHashRefHelper ohrh(args, 2, xsink);
   ReferenceHolder<AbstractQoreNode> rv(client->head(path, ph, *ohrh, xsink), xsink);

   return *xsink ? 0 : rv.release();
}

// *string HTTPClient::post(string $path, data $data, hash $headers = hash())  
// *string HTTPClient::post(string $path, data $data, hash $headers = hash(), reference $info)  
static AbstractQoreNode *HC_post(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *pstr = HARD_QORE_STRING(args, 0);
   const char *path = pstr->getBuffer();

   const void *ptr = 0;
   qore_size_t size = 0;
   const AbstractQoreNode *p = get_param(args, 1);
   if (p->getType() == NT_STRING) {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p);
      assert(str);
      ptr = str->getBuffer();
      size = str->strlen();
   }
   else {
      assert(p->getType() == NT_BINARY);
      const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p);
      ptr = b->getPtr();
      size = b->size();
   }

   const QoreHashNode *ph = HARD_QORE_HASH(args, 2);

   OptHashRefHelper ohrh(args, 2, xsink);
   ReferenceHolder<AbstractQoreNode> rv(client->post(path, ph, ptr, size, *ohrh, xsink), xsink);

   return *xsink ? 0 : rv.release();
}

// nothing HTTPClient::setTimeout(timeout $timeout_ms = 0)  
static AbstractQoreNode *HC_setTimeout(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setTimeout((int)HARD_QORE_INT(args, 0));
   return 0;
}

static AbstractQoreNode *HC_getTimeout(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return new QoreBigIntNode(client->getTimeout());
}

static AbstractQoreNode *HC_setEncoding(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setEncoding(get_hard_qore_encoding_param(args, 0));
   return 0; 
}

static AbstractQoreNode *HC_getEncoding(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return new QoreStringNode(client->getEncoding()->getCode());
}

static AbstractQoreNode *HC_setURL(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *pstr = HARD_QORE_STRING(args, 0);
   client->setURL(pstr->getBuffer(), xsink);
   return 0;
}

static AbstractQoreNode *HC_getURL(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return client->getURL();
}

static AbstractQoreNode *HC_setProxyURL_nothing(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->clearProxyURL();
   return 0;
}

static AbstractQoreNode *HC_setProxyURL_str(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *pstr = HARD_QORE_STRING(args, 0);
   client->setProxyURL(pstr->getBuffer(), xsink);
   return 0;
}

static AbstractQoreNode *HC_getProxyURL(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return client->getProxyURL();
}

static AbstractQoreNode *HC_clearProxyURL(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->clearProxyURL();
   return 0;
}

static AbstractQoreNode *HC_setProxySecure(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setProxySecure(HARD_QORE_BOOL(args, 0));
   return 0;
}

static AbstractQoreNode *HC_isProxySecure(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return get_bool_node(client->isProxySecure());
}

static AbstractQoreNode *HC_setMaxRedirects(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setMaxRedirects((int)HARD_QORE_INT(args, 0));
   return 0;
}

static AbstractQoreNode *HC_getMaxRedirects(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return new QoreBigIntNode(client->getMaxRedirects());
}

static AbstractQoreNode *HC_setEventQueue_nothing(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
    client->setEventQueue(0, xsink);
    return 0;
}

static AbstractQoreNode *HC_setEventQueue_queue(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   HARD_QORE_OBJ_DATA(q, Queue, args, 0, CID_QUEUE, "Queue", "HTTPClient::setEventQueue", xsink);
   if (*xsink)
      return 0;
   // pass reference from QoreObject::getReferencedPrivateData() to function
   client->setEventQueue(q, xsink);
   return 0;
}

// nothing HTTPClient::setConnectTimeout(timeout $timeout_ms = -1)  
static AbstractQoreNode *HC_setConnectTimeout(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->setConnectTimeout((int)HARD_QORE_INT(args, 0));
   return 0;
}

static AbstractQoreNode *HC_getConnectTimeout(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   return new QoreBigIntNode(client->getConnectTimeout());
}

static AbstractQoreNode *HC_setNoDelay(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
    return new QoreBigIntNode(client->setNoDelay(HARD_QORE_BOOL(args, 0)));
}

static AbstractQoreNode *HC_getNoDelay(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
    return get_bool_node(client->getNoDelay());
}

static AbstractQoreNode *HC_isConnected(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
    return get_bool_node(client->isConnected());
}

// nothing HTTPClient::setUserPassword(string user, string pass)
static AbstractQoreNode *HC_setUserPassword(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *user = HARD_QORE_STRING(args, 0);
   const QoreStringNode *pass = HARD_QORE_STRING(args, 1);

   client->setUserPassword(user->getBuffer(), pass->getBuffer());
   return 0;
}

// nothing HTTPClient::clearUserPassword()
static AbstractQoreNode *HC_clearUserPassword(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->clearUserPassword();
   return 0;
}

// nothing HTTPClient::setProxyUserPassword(string user, string pass)
static AbstractQoreNode *HC_setProxyUserPassword(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   const QoreStringNode *user = HARD_QORE_STRING(args, 0);
   const QoreStringNode *pass = HARD_QORE_STRING(args, 1);

   client->setProxyUserPassword(user->getBuffer(), pass->getBuffer());
   return 0;
}

// nothing HTTPClient::clearProxyUserPassword()
static AbstractQoreNode *HC_clearProxyUserPassword(QoreObject *self, QoreHTTPClient *client, const QoreListNode *args, ExceptionSink *xsink) {
   client->clearProxyUserPassword();
   return 0;
}

QoreClass *initHTTPClientClass() {
   QORE_TRACE("initHTTPClientClass");

   assert(QC_QUEUE);

   // initialize static data structures in the QoreHTTPClient class
   QoreHTTPClient::static_init();

   QoreClass* client = new QoreClass("HTTPClient", QDOM_NETWORK);
   // no need to set the class synchronous flag because the QoreHTTPClient class is already thread-safe
   CID_HTTPCLIENT = client->getID();

   client->setConstructorExtended(HC_constructor, false, QC_NO_FLAGS, QDOM_DEFAULT);
   client->setConstructorExtended(HC_constructor_hash, false, QC_NO_FLAGS, QDOM_DEFAULT, 1, hashTypeInfo, QORE_PARAM_NO_ARG);

   client->setCopy((q_copy_t)HC_copy);
   client->setDestructor((q_destructor_t)HC_destructor);

   client->addMethodExtended("setHTTPVersion",         (q_method_t)HC_setHTTPVersion, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   client->addMethodExtended("getHTTPVersion",         (q_method_t)HC_getHTTPVersion, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo);

   client->addMethodExtended("setSecure",              (q_method_t)HC_setSecure, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, softBoolTypeInfo, &True);

   client->addMethodExtended("isSecure",               (q_method_t)HC_isSecure, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, boolTypeInfo);

   // returns either a string or nothing
   //*string HTTPClient::verifyPeerCertificate()  
   client->addMethodExtended("verifyPeerCertificate",  (q_method_t)HC_verifyPeerCertificate, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo);

   // returns either a string or nothing
   client->addMethodExtended("getSSLCipherName",       (q_method_t)HC_getSSLCipherName, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo);

   // returns either a string or nothing
   client->addMethodExtended("getSSLCipherVersion",    (q_method_t)HC_getSSLCipherVersion, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo);

   client->addMethodExtended("connect",                (q_method_t)HC_connect, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   client->addMethodExtended("disconnect",             (q_method_t)HC_disconnect, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // hash HTTPClient::send([data], string $method, string $path = "", hash headers = hash(), bool $getbody = False)  
   // hash HTTPClient::send([data], string $method, string $path = "", hash headers = hash(), bool $getbody = False, reference $info)  
   client->addMethodExtended("send",                   (q_method_t)HC_send, false, QC_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo, 5, dataTypeInfo, new BinaryNode, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, null_string(), hashTypeInfo, empty_hash(), softBoolTypeInfo, &False);
   client->addMethodExtended("send",                   (q_method_t)HC_send, false, QC_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo, 6, dataTypeInfo, new BinaryNode, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, null_string(), hashTypeInfo, empty_hash(), softBoolTypeInfo, &False, referenceTypeInfo, QORE_PARAM_NO_ARG);

   // *string HTTPClient::get(string $path, hash $headers = hash())  
   // *string HTTPClient::get(string $path, hash $headers = hash(), reference $ref)  
   client->addMethodExtended("get",                    (q_method_t)HC_get, false, QC_NO_FLAGS, QDOM_DEFAULT, stringOrNothingTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash());
   client->addMethodExtended("get",                    (q_method_t)HC_get, false, QC_NO_FLAGS, QDOM_DEFAULT, stringOrNothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash(), referenceTypeInfo, QORE_PARAM_NO_ARG);

   // hash HTTPClient::head(string $path, hash $headers = hash())  
   // hash HTTPClient::head(string $path, hash $headers = hash(), reference $info)  
   client->addMethodExtended("head",                   (q_method_t)HC_head, false, QC_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash());
   client->addMethodExtended("head",                   (q_method_t)HC_head, false, QC_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash(), referenceTypeInfo, QORE_PARAM_NO_ARG);

   // *string HTTPClient::post(string $path, data $data, hash $headers = hash())  
   // *string HTTPClient::post(string $path, data $data, hash $headers = hash(), reference $info)   
  client->addMethodExtended("post",                   (q_method_t)HC_post, false, QC_NO_FLAGS, QDOM_DEFAULT, stringOrNothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, dataTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash());
   client->addMethodExtended("post",                   (q_method_t)HC_post, false, QC_NO_FLAGS, QDOM_DEFAULT, stringOrNothingTypeInfo, 4, stringTypeInfo, QORE_PARAM_NO_ARG, dataTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, empty_hash(), referenceTypeInfo, QORE_PARAM_NO_ARG);

   // nothing HTTPClient::setTimeout(timeout $timeout_ms = 0)  
   client->addMethodExtended("setTimeout",             (q_method_t)HC_setTimeout, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, timeoutTypeInfo, zero());

   client->addMethodExtended("getTimeout",             (q_method_t)HC_getTimeout, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, bigIntTypeInfo);

   client->addMethodExtended("setEncoding",            (q_method_t)HC_setEncoding, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   client->addMethodExtended("getEncoding",            (q_method_t)HC_getEncoding, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo);

   client->addMethodExtended("setURL",                 (q_method_t)HC_setURL, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   client->addMethodExtended("getURL",                 (q_method_t)HC_getURL, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo);

   // nothing HTTPClient::setProxyURL()  
   // clears the proxy URL
   client->addMethodExtended("setProxyURL",            (q_method_t)HC_setProxyURL_nothing, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
   // nothing HTTPClient::setProxyURL(string $url)  
   client->addMethodExtended("setProxyURL",            (q_method_t)HC_setProxyURL_str, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   // *string HTTPClient::getProxyURL()  
   client->addMethodExtended("getProxyURL",            (q_method_t)HC_getProxyURL, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo);

   // nothing HTTPClient::clearProxyURL()  
   client->addMethodExtended("clearProxyURL",          (q_method_t)HC_clearProxyURL, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // nothing HTTPClient::setProxySecure(softbool $b = True)  
   client->addMethodExtended("setProxySecure",         (q_method_t)HC_setProxySecure, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, softBoolTypeInfo, &True);

   // bool HTTPClient::isProxySecure()  
   client->addMethodExtended("isProxySecure",          (q_method_t)HC_isProxySecure, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, boolTypeInfo);

   // nothing HTTPClient::setMaxRedirects(softint $mr = 0)  
   client->addMethodExtended("setMaxRedirects",        (q_method_t)HC_setMaxRedirects, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, softBigIntTypeInfo, zero());

   // int HTTPClient::getMaxRedirects()  
   client->addMethodExtended("getMaxRedirects",        (q_method_t)HC_getMaxRedirects, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, bigIntTypeInfo);

   // nothing HTTPClient::setEventQueue()  
   client->addMethodExtended("setEventQueue",          (q_method_t)HC_setEventQueue_nothing, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
   // nothing HTTPClient::setEventQueue(Queue $queue)  
   client->addMethodExtended("setEventQueue",          (q_method_t)HC_setEventQueue_queue, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, QC_QUEUE->getTypeInfo(), QORE_PARAM_NO_ARG);

   // nothing HTTPClient::setConnectTimeout(timeout $timeout_ms = -1)  
   client->addMethodExtended("setConnectTimeout",      (q_method_t)HC_setConnectTimeout, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, timeoutTypeInfo, new QoreBigIntNode(-1));

   // int HTTPClient::getConnectTimeout()  
   client->addMethodExtended("getConnectTimeout",      (q_method_t)HC_getConnectTimeout, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, bigIntTypeInfo);

   // int HTTPClient::setNoDelay(softbool $b = True)  
   client->addMethodExtended("setNoDelay",             (q_method_t)HC_setNoDelay, false, QC_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 1, softBoolTypeInfo, &True);

   // bool HTTPClient::getNoDelay()  
   client->addMethodExtended("getNoDelay",             (q_method_t)HC_getNoDelay, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, boolTypeInfo);

   // bool HTTPClient::isConnected()  
   client->addMethodExtended("isConnected",            (q_method_t)HC_isConnected, false, QC_RET_VALUE_ONLY, QDOM_DEFAULT, boolTypeInfo);

   // nothing HTTPClient::setUserPassword(string user, string pass)
   client->addMethodExtended("setUserPassword",        (q_method_t)HC_setUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   // nothing HTTPClient::setUserPassword()   
   client->addMethodExtended("setUserPassword",        (q_method_t)HC_clearUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // nothing HTTPClient::clearUserPassword()
   client->addMethodExtended("clearUserPassword",      (q_method_t)HC_clearUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // nothing HTTPClient::setProxyUserPassword(string user, string pass)
   client->addMethodExtended("setProxyUserPassword",   (q_method_t)HC_setProxyUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   // nothing HTTPClient::setProxyUserPassword()   
   client->addMethodExtended("setProxyUserPassword",   (q_method_t)HC_clearProxyUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // nothing HTTPClient::clearProxyUserPassword()
   client->addMethodExtended("clearProxyUserPassword", (q_method_t)HC_clearProxyUserPassword, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   return client;
}
