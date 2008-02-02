/*
 WhileStatement.cc
 
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
#include <qore/intern/WhileStatement.h>
#include <qore/intern/StatementBlock.h>

WhileStatement::WhileStatement(int start_line, int end_line, AbstractQoreNode *c, class StatementBlock *cd) : AbstractStatement(start_line, end_line)
{
   cond = c;
   code = cd;
   lvars = NULL;
}

WhileStatement::~WhileStatement()
{
   cond->deref(NULL);
   if (code)
      delete code;
   if (lvars)
      delete lvars;
}

int WhileStatement::execImpl(AbstractQoreNode **return_value, ExceptionSink *xsink)
{
   int i, rc = 0;
   
   tracein("WhileStatement::execWhileImpl()");
   // instantiate local variables
   for (i = 0; i < lvars->num_lvars; i++)
      instantiateLVar(lvars->ids[i], NULL);
   
   while (cond->boolEval(xsink) && !xsink->isEvent())
   {
      if (code && (((rc = code->execImpl(return_value, xsink)) == RC_BREAK) || xsink->isEvent()))
      {
	 rc = 0;
	 break;
      }
      if (rc == RC_RETURN)
	 break;
      else if (rc == RC_CONTINUE)
	 rc = 0;
   }
   // uninstantiate local variables
   for (i = 0; i < lvars->num_lvars; i++)
      uninstantiateLVar(xsink);
   traceout("WhileStatement::execWhile()");
   return rc;
}

int WhileStatement::parseInitImpl(lvh_t oflag, int pflag)
{
   int lvids = 0;
   
   lvids += process_node(&cond, oflag, pflag);
   if (code)
      code->parseInitImpl(oflag, pflag);
   
   // save local variables
   lvars = new LVList(lvids);

   return 0;
}
