/*
 QC_QTextBlockFormat.cc
 
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

#include "QC_QTextBlockFormat.h"

int CID_QTEXTBLOCKFORMAT;
class QoreClass *QC_QTextBlockFormat = 0;

//QTextBlockFormat ()
static void QTEXTBLOCKFORMAT_constructor(Object *self, QoreNode *params, ExceptionSink *xsink)
{
   self->setPrivate(CID_QTEXTBLOCKFORMAT, new QoreQTextBlockFormat());
   return;
}

static void QTEXTBLOCKFORMAT_copy(class Object *self, class Object *old, class QoreQTextBlockFormat *qtbf, ExceptionSink *xsink)
{
   xsink->raiseException("QTEXTBLOCKFORMAT-COPY-ERROR", "objects of this class cannot be copied");
}

//Qt::Alignment alignment () const
static QoreNode *QTEXTBLOCKFORMAT_alignment(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((int64)qtbf->alignment());
}

//qreal bottomMargin () const
static QoreNode *QTEXTBLOCKFORMAT_bottomMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((double)qtbf->bottomMargin());
}

//int indent () const
static QoreNode *QTEXTBLOCKFORMAT_indent(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((int64)qtbf->indent());
}

//bool isValid () const
static QoreNode *QTEXTBLOCKFORMAT_isValid(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qtbf->isValid());
}

//qreal leftMargin () const
static QoreNode *QTEXTBLOCKFORMAT_leftMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((double)qtbf->leftMargin());
}

//bool nonBreakableLines () const
static QoreNode *QTEXTBLOCKFORMAT_nonBreakableLines(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode(qtbf->nonBreakableLines());
}

//PageBreakFlags pageBreakPolicy () const
static QoreNode *QTEXTBLOCKFORMAT_pageBreakPolicy(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((int64)qtbf->pageBreakPolicy());
}

//qreal rightMargin () const
static QoreNode *QTEXTBLOCKFORMAT_rightMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((double)qtbf->rightMargin());
}

//void setAlignment ( Qt::Alignment alignment )
static QoreNode *QTEXTBLOCKFORMAT_setAlignment(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   Qt::Alignment alignment = (Qt::Alignment)(p ? p->getAsInt() : 0);
   qtbf->setAlignment(alignment);
   return 0;
}

//void setBottomMargin ( qreal margin )
static QoreNode *QTEXTBLOCKFORMAT_setBottomMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   qreal margin = p ? p->getAsFloat() : 0.0;
   qtbf->setBottomMargin(margin);
   return 0;
}

//void setIndent ( int indentation )
static QoreNode *QTEXTBLOCKFORMAT_setIndent(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   int indentation = p ? p->getAsInt() : 0;
   qtbf->setIndent(indentation);
   return 0;
}

//void setLeftMargin ( qreal margin )
static QoreNode *QTEXTBLOCKFORMAT_setLeftMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   qreal margin = p ? p->getAsFloat() : 0.0;
   qtbf->setLeftMargin(margin);
   return 0;
}

//void setNonBreakableLines ( bool b )
static QoreNode *QTEXTBLOCKFORMAT_setNonBreakableLines(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   bool b = p ? p->getAsBool() : false;
   qtbf->setNonBreakableLines(b);
   return 0;
}

//void setPageBreakPolicy ( PageBreakFlags policy )
static QoreNode *QTEXTBLOCKFORMAT_setPageBreakPolicy(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   QTextBlockFormat::PageBreakFlags policy = (QTextBlockFormat::PageBreakFlags)(p ? p->getAsInt() : 0);
   qtbf->setPageBreakPolicy(policy);
   return 0;
}

//void setRightMargin ( qreal margin )
static QoreNode *QTEXTBLOCKFORMAT_setRightMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   qreal margin = p ? p->getAsFloat() : 0.0;
   qtbf->setRightMargin(margin);
   return 0;
}

//void setTextIndent ( qreal indent )
static QoreNode *QTEXTBLOCKFORMAT_setTextIndent(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   qreal indent = p ? p->getAsFloat() : 0.0;
   qtbf->setTextIndent(indent);
   return 0;
}

//void setTopMargin ( qreal margin )
static QoreNode *QTEXTBLOCKFORMAT_setTopMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   QoreNode *p = get_param(params, 0);
   qreal margin = p ? p->getAsFloat() : 0.0;
   qtbf->setTopMargin(margin);
   return 0;
}

//qreal textIndent () const
static QoreNode *QTEXTBLOCKFORMAT_textIndent(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((double)qtbf->textIndent());
}

//qreal topMargin () const
static QoreNode *QTEXTBLOCKFORMAT_topMargin(Object *self, QoreQTextBlockFormat *qtbf, QoreNode *params, ExceptionSink *xsink)
{
   return new QoreNode((double)qtbf->topMargin());
}

QoreClass *initQTextBlockFormatClass(QoreClass *qtextformat)
{
   QC_QTextBlockFormat = new QoreClass("QTextBlockFormat", QDOM_GUI);
   CID_QTEXTBLOCKFORMAT = QC_QTextBlockFormat->getID();

   QC_QTextBlockFormat->addBuiltinVirtualBaseClass(qtextformat);

   QC_QTextBlockFormat->setConstructor(QTEXTBLOCKFORMAT_constructor);
   QC_QTextBlockFormat->setCopy((q_copy_t)QTEXTBLOCKFORMAT_copy);

   QC_QTextBlockFormat->addMethod("alignment",                   (q_method_t)QTEXTBLOCKFORMAT_alignment);
   QC_QTextBlockFormat->addMethod("bottomMargin",                (q_method_t)QTEXTBLOCKFORMAT_bottomMargin);
   QC_QTextBlockFormat->addMethod("indent",                      (q_method_t)QTEXTBLOCKFORMAT_indent);
   QC_QTextBlockFormat->addMethod("isValid",                     (q_method_t)QTEXTBLOCKFORMAT_isValid);
   QC_QTextBlockFormat->addMethod("leftMargin",                  (q_method_t)QTEXTBLOCKFORMAT_leftMargin);
   QC_QTextBlockFormat->addMethod("nonBreakableLines",           (q_method_t)QTEXTBLOCKFORMAT_nonBreakableLines);
   QC_QTextBlockFormat->addMethod("pageBreakPolicy",             (q_method_t)QTEXTBLOCKFORMAT_pageBreakPolicy);
   QC_QTextBlockFormat->addMethod("rightMargin",                 (q_method_t)QTEXTBLOCKFORMAT_rightMargin);
   QC_QTextBlockFormat->addMethod("setAlignment",                (q_method_t)QTEXTBLOCKFORMAT_setAlignment);
   QC_QTextBlockFormat->addMethod("setBottomMargin",             (q_method_t)QTEXTBLOCKFORMAT_setBottomMargin);
   QC_QTextBlockFormat->addMethod("setIndent",                   (q_method_t)QTEXTBLOCKFORMAT_setIndent);
   QC_QTextBlockFormat->addMethod("setLeftMargin",               (q_method_t)QTEXTBLOCKFORMAT_setLeftMargin);
   QC_QTextBlockFormat->addMethod("setNonBreakableLines",        (q_method_t)QTEXTBLOCKFORMAT_setNonBreakableLines);
   QC_QTextBlockFormat->addMethod("setPageBreakPolicy",          (q_method_t)QTEXTBLOCKFORMAT_setPageBreakPolicy);
   QC_QTextBlockFormat->addMethod("setRightMargin",              (q_method_t)QTEXTBLOCKFORMAT_setRightMargin);
   QC_QTextBlockFormat->addMethod("setTextIndent",               (q_method_t)QTEXTBLOCKFORMAT_setTextIndent);
   QC_QTextBlockFormat->addMethod("setTopMargin",                (q_method_t)QTEXTBLOCKFORMAT_setTopMargin);
   QC_QTextBlockFormat->addMethod("textIndent",                  (q_method_t)QTEXTBLOCKFORMAT_textIndent);
   QC_QTextBlockFormat->addMethod("topMargin",                   (q_method_t)QTEXTBLOCKFORMAT_topMargin);

   return QC_QTextBlockFormat;
}
