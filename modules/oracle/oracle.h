/*
  oracle.h

  Qore Programming Language

  Copyright (C) 2003, 2004, 2005, 2006

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

#ifndef QORE_ORACLE_H

#define QORE_ORACLE_H

#include <qore/common.h>
#include <qore/Exception.h>
#include <qore/support.h>
#include <qore/DBI.h>

#include "oracle-config.h"

#include <oci.h>

// define to support old-style exec format
#define ORA_EXEC_COMPAT 1

// with 10g on Linux the streaming *lob callback function would 
// never get more than 1024KB data at a time, however with a 9i
// server and client on Solaris, it would not work unless my 
// buffer size was at least twice as big as my CLOB!
#define LOB_BLOCK_SIZE 128*1024

#define MAXINT32 2147483647   // 2^^32 - 1

// FIXME: do not hardcode byte widths - could be incorrect on some platforms
union ora_value {
      void *ptr;
      int i4;
      int64 i8;
      double f8;
};

class OraColumn {
   public:
      char *name;
      int maxsize;
      ub2 dtype;           // Oracle datatype for column
      OCIDefine *defp;     // define handle
      sb2 ind;             // indicator value

      union ora_value val;

      class OraColumn *next;

      inline OraColumn(char *n, int len, int ms, ub2 dt)
      {
	 name = (char *)malloc(sizeof(char) * (len + 1));
	 strncpy(name, n, len);
	 name[len] = '\0';
	 strtolower(name);
	 maxsize = ms;
	 dtype = dt;
	 defp = NULL;

	 next = NULL;
      }
      inline ~OraColumn()
      {
	 free(name);
	 if (defp)
	 {
	    switch (dtype)
	    {
	       case SQLT_INT:
	       case SQLT_FLT:
		  break;

	       case SQLT_CLOB:
	       case SQLT_BLOB:
		  if (val.ptr)
		     OCIDescriptorFree(val.ptr, OCI_DTYPE_LOB);
		  break;

	       default:	  // for date and string data
		  free(val.ptr);
		  break;
	    }
	    OCIHandleFree(defp, OCI_HTYPE_DEFINE);
	 }	 
      }
      class QoreNode *getValue(class Datasource *ds, class ExceptionSink *xsink);
};

class OraColumns {
  private:
      int len;
      class OraColumn *head, *tail;

   public:
      OraColumns(OCIStmt *stmthp, class Datasource *ds, char *str, ExceptionSink *xsink);
      inline ~OraColumns()
      {
	 class OraColumn *w = head;
	 while (w)
	 {
	    head = w->next;
	    delete w;
	    w = head;
	 }
      }
      inline void add(char *name, int nlen, int maxsize, ub2 dtype)
      {
	 len++;
	 class OraColumn *c = new OraColumn(name, nlen, maxsize, dtype);

	 if (!tail)
	    head = c;
	 else
	    tail->next = c;
	 tail = c;

	 // printd(5, "column: '%s'\n", c->name);
	 printd(5, "OraColumns::add() %2d name='%s' (max %d) type=%d\n", size(), c->name, c->maxsize, dtype);
      }
      inline class OraColumn *getHead() 
      {
	 return head;
      }
      inline int size()
      {
	 return len;
      }

      void define(OCIStmt *stmthp, class Datasource *ds, char *str, ExceptionSink *xsink);
};

union ora_bind {
      struct {
	    char *name;       // name for output hash
	    int maxsize;      // maximum size, -1 = default for type
	    char *type;       // qore datatype for column
      } ph;
      struct {
	    class QoreNode *value;   // value to be bound
	    class QoreString *tstr;   // temporary string to be deleted
      } v;
};

class OraBindNode {
   public:
      int bindtype;
      union ora_bind data;
      ub2 buftype;
      union ora_value buf; // for bind buffers
      sb2 ind;             // NULL indicator for OCI calls
      class OraBindNode *next;

      inline OraBindNode(class QoreNode *v) // for value nodes
      {
	 bindtype = BN_VALUE;
	 data.v.value = v;
	 data.v.tstr = NULL;
	 buftype = 0;
	 next = NULL;
      }
      inline OraBindNode(char *name, int size, char *typ)
      {
	 bindtype = BN_PLACEHOLDER;
	 data.ph.name = name;
	 data.ph.maxsize = size;
	 data.ph.type = typ;
	 buftype = 0;
	 next = NULL;
      }
      inline ~OraBindNode()
      {
	 if (bindtype == BN_PLACEHOLDER)
	 {
	    if (data.ph.name)
	       free(data.ph.name);

	    // free buffer data if any
	    if ((buftype == SQLT_STR || buftype == SQLT_DAT) && buf.ptr)
	       free(buf.ptr);
	    else if (buftype == SQLT_RSET && buf.ptr)
	       OCIHandleFree((OCIStmt *)buf.ptr, OCI_HTYPE_STMT);
	    else if ((buftype == SQLT_BLOB || buftype ==SQLT_CLOB) && buf.ptr)
	       OCIDescriptorFree(buf.ptr, OCI_DTYPE_LOB);
	 }
	 else 
	 {
	    if (data.v.tstr)
	       delete data.v.tstr;
	    if (buftype == SQLT_DAT && buf.ptr)
	       free(buf.ptr);
	 }
      }

      void bindValue(class Datasource *ds, OCIStmt *stmthp, int pos, class ExceptionSink *xsink);
      void bindPlaceholder(class Datasource *ds, OCIStmt *stmthp, int pos, class ExceptionSink *xsink);
      class QoreNode *getValue(class Datasource *ds, class ExceptionSink *xsink);
};

static void ora_checkerr(OCIError *errhp, sword status, char *query_name, Datasource *ds, ExceptionSink *xsink);

class OraBindGroup {
  private:
      int len;
      class OraBindNode *head, *tail;
      class QoreString *str;
      OCIStmt *stmthp;
      class Datasource *ds;
      bool hasOutput;

      void parseOld(class Hash *h, class ExceptionSink *xsink);
      void parseQuery(class List *args, class ExceptionSink *xsink);
      class QoreNode *getOutputHash(class ExceptionSink *xsink);

      inline void add(class OraBindNode *c)
      {
	 len++;
	 if (!tail)
	    head = c;
	 else
	    tail->next = c;
	 tail = c;
      }

   public:
      OraBindGroup(class Datasource *ods, class QoreString *ostr, class List *args, ExceptionSink *xsink);
      inline ~OraBindGroup()
      {
	 // free OCI handle
	 if (stmthp)
	    OCIHandleFree(stmthp, OCI_HTYPE_STMT);

	 if (str)
	    delete str;

	 class OraBindNode *w = head;
	 while (w)
	 {
	    head = w->next;
	    delete w;
	    w = head;
	 }
      }
      inline void add(class QoreNode *v)
      {
	 class OraBindNode *c = new OraBindNode(v);
	 add(c);
	 printd(5, "OraBindGroup::add()\n");
      }
      inline void add(char *name, int size, char *type)
      {
	 class OraBindNode *c = new OraBindNode(name, size, type);
	 add(c);
	 printd(5, "OraBindGroup::add()\n");
	 hasOutput = true;
      }

      class QoreNode *exec(class ExceptionSink *xsink);
      class QoreNode *select(class ExceptionSink *xsink);
      class QoreNode *selectRows(class ExceptionSink *xsink);
};

#endif
