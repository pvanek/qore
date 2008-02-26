/*
 ParseOptionMap.h
 
 Qore Programming language
 
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

#ifndef _QORE_PARSEOPTIONMAP_H
#define _QORE_PARSEOPTIONMAP_H

#include <qore/Restrictions.h>

typedef std::map<const char *, qore_restrictions_t, ltstr> opt_map_t;
typedef std::map<qore_restrictions_t, const char *> rev_opt_map_t;

//! provides access to parse option information
class ParseOptionMap {
   private:
      DLLLOCAL static opt_map_t map;
      DLLLOCAL static rev_opt_map_t rmap;

      // not implemented
      DLLLOCAL ParseOptionMap(const ParseOptionMap&);
      DLLLOCAL ParseOptionMap& operator=(const ParseOptionMap&);
      
   public:
      DLLLOCAL ParseOptionMap();
      DLLLOCAL static void static_init();

      //! find a parse option name from its code
      DLLEXPORT static const char *find_name(qore_restrictions_t code);

      //! find a parse option code from its name
      DLLEXPORT static qore_restrictions_t find_code(const char *name);

      //! print out all parse optionsto stdout
      DLLEXPORT static void list_options();
};

#endif
