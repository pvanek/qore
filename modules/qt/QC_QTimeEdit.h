/*
 QC_QTimeEdit.h
 
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

#ifndef _QORE_QT_QC_QTIMEEDIT_H

#define _QORE_QT_QC_QTIMEEDIT_H

#include <QTimeEdit>
#include "QoreAbstractQDateTimeEdit.h"
#include "qore-qt-events.h"

DLLLOCAL extern int CID_QTIMEEDIT;
DLLLOCAL extern class QoreClass *QC_QTimeEdit;

DLLLOCAL class QoreClass *initQTimeEditClass(QoreClass *);

class myQTimeEdit : public QTimeEdit, public QoreQWidgetExtension
{
#define QOREQTYPE QTimeEdit
#include "qore-qt-metacode.h"
#include "qore-qt-widget-events.h"
#undef QOREQTYPE

   public:
      DLLLOCAL myQTimeEdit(Object *obj, QWidget* parent = 0) : QTimeEdit(parent), QoreQWidgetExtension(obj->getClass())
      {
         init(obj);
      }
      DLLLOCAL myQTimeEdit(Object *obj, const QTime& time, QWidget* parent = 0) : QTimeEdit(time, parent), QoreQWidgetExtension(obj->getClass())
      {
         init(obj);
      }
};

class QoreQTimeEdit : public QoreAbstractQDateTimeEdit
{
   public:
      QPointer<myQTimeEdit> qobj;

      DLLLOCAL QoreQTimeEdit(Object *obj, QWidget* parent = 0) : qobj(new myQTimeEdit(obj, parent))
      {
      }
      DLLLOCAL QoreQTimeEdit(Object *obj, const QTime& time, QWidget* parent = 0) : qobj(new myQTimeEdit(obj, time, parent))
      {
      }
      DLLLOCAL virtual class QObject *getQObject() const
      {
         return static_cast<QObject *>(&(*qobj));
      }
      DLLLOCAL virtual class QWidget *getQWidget() const
      {
         return static_cast<QWidget *>(&(*qobj));
      }
      DLLLOCAL virtual QPaintDevice *getQPaintDevice() const
      {
         return static_cast<QPaintDevice *>(&(*qobj));
      }
      DLLLOCAL virtual class QDateTimeEdit *getQDateTimeEdit() const
      {
         return static_cast<QDateTimeEdit *>(&(*qobj));
      }
      DLLLOCAL virtual class QAbstractSpinBox *getQAbstractSpinBox() const
      {
         return static_cast<QAbstractSpinBox *>(&(*qobj));
      }
      QORE_VIRTUAL_QWIDGET_METHODS
};

#endif // _QORE_QT_QC_QTIMEEDIT_H
