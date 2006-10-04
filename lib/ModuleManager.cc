/*
  ModuleManager.cc

  Qore module manager

  Qore Programming Language

  Copyright (C) 2003, 2004, 2005, 2006 David Nichols

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

#include <qore/config.h>
#include <qore/ModuleManager.h>
#include <qore/support.h>
#include <qore/QoreString.h>
#include <qore/List.h>
#include <qore/Hash.h>
#include <qore/QoreLib.h>
#include <qore/AutoNamespaceList.h>
#include <qore/QoreProgram.h>

#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <glob.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define AUTO_MODULE_DIR MODULE_DIR "/auto"

class ModuleManager MM;

#ifdef QORE_MONOLITHIC
// for non-shared builds of the qore library, initialize all optional components here
# ifdef TIBRV
#  include "../modules/TIBCO/tibrv-module.h"
# endif
# ifdef TIBAE
#  include "../modules/TIBCO/tibae-module.h"
# endif
# ifdef ORACLE
#  include "../modules/oracle/oracle-module.h"
# endif
# ifdef QORE_MYSQL
#  include "../modules/mysql/qore-mysql-module.h"
# endif
# ifdef NCURSES
# include "../modules/ncurses/ncurses-module.h"
# endif
#endif

// builtin module info node - when features are compiled into the library
inline ModuleInfo::ModuleInfo(char *fn, qore_module_delete_t del)
{
   filename = "<builtin>";
   name = fn;
   api_major = QORE_MODULE_API_MAJOR;
   api_minor = QORE_MODULE_API_MINOR;
   module_init = NULL;
   module_ns_init = NULL;
   module_delete = del;
   version = desc = author = url = "<builtin>";
   dlptr = NULL;
}

ModuleInfo::~ModuleInfo()
{
   printd(5, "ModuleInfo::~ModuleInfo() '%s': %s calling module_delete=%08p\n", name, filename, module_delete);
   module_delete();
   if (dlptr)
   {
      printd(5, "calling dlclose(%08p)\n", dlptr);
#ifndef DEBUG
      // do not close modules when debugging
      dlclose(dlptr);
#endif
      free(filename);
   }
}

class Hash *ModuleInfo::getHash()
{
   class Hash *h = new Hash();
   h->setKeyValue("filename", new QoreNode(filename), NULL);
   h->setKeyValue("name", new QoreNode(name), NULL);
   h->setKeyValue("desc", new QoreNode(desc), NULL);
   h->setKeyValue("version", new QoreNode(version), NULL);
   h->setKeyValue("author", new QoreNode(author), NULL);
   h->setKeyValue("api_major", new QoreNode((int64)api_major), NULL);
   h->setKeyValue("api_minor", new QoreNode((int64)api_minor), NULL);
   if (url)
      h->setKeyValue("url", new QoreNode(url), NULL);
   return h;
}

ModuleManager::ModuleManager()
{
   head = NULL;
   num = 0;
}

inline void ModuleManager::addBuiltin(char *fn, qore_module_init_t init, qore_module_ns_init_t ns_init, qore_module_delete_t del)
{
   class QoreString *str = init();
   if (str)
   {
      fprintf(stderr, "WARNING! cannot initialize builtin feature '%s': %s\n", fn, str->getBuffer());
      delete str;
      return;
   }
   // "num" is not incremented here - only incremented for real modules
   add(new ModuleInfo(fn, del));
   ANSL.add(ns_init);
}

// to add a directory to the module directory search list, can only be called before init()
void ModuleManager::addModuleDir(char *dir)
{
   moduleDirList.push_back(strdup(dir));
}

// to add a directory to the auto module directory search list, can only be called before init()
void ModuleManager::addAutoModuleDir(char *dir)
{
   autoDirList.push_back(strdup(dir));
}

// to add a list of directories to the module directory search list, can only be called before init()
void ModuleManager::addModuleDirList(char *strlist)
{
   moduleDirList.addDirList(strlist);
}

// to add a list of directories to the auto module directory search list, can only be called before init()
void ModuleManager::addAutoModuleDirList(char *strlist)
{
   autoDirList.addDirList(strlist);
}

void ModuleManager::init(bool se)
{
   show_errors = se;

   // set up auto-load list from QORE_AUTO_MODULE_DIR (if it hasn't already been manually set up)
   if (autoDirList.empty())
      autoDirList.addDirList(getenv("QORE_AUTO_MODULE_DIR"));

   // setup module directory list from QORE_MODULE_DIR (if it hasn't already been manually set up)
   if (moduleDirList.empty())
      moduleDirList.addDirList(getenv("QORE_MODULE_DIR"));

   // append standard directories to the end of the list
   autoDirList.push_back(strdup(AUTO_MODULE_DIR));
   moduleDirList.push_back(strdup(MODULE_DIR));

#ifdef QORE_MONOLITHIC
# ifdef NCURSES
   addBuiltin("ncurses", ncurses_module_init, ncurses_module_ns_init, ncurses_module_delete);
# endif
# ifdef ORACLE
   addBuiltin("oracle", oracle_module_init, oracle_module_ns_init, oracle_module_delete);
# endif
# ifdef QORE_MYSQL
   addBuiltin("mysql", qore_mysql_module_init, qore_mysql_module_ns_init, qore_mysql_module_delete);
# endif
# ifdef TIBRV
   addBuiltin("tibrv", tibrv_module_init, tibrv_module_ns_init, tibrv_module_delete);
# endif
# ifdef TIBAE
   addBuiltin("tibae", tibae_module_init, tibae_module_ns_init, tibae_module_delete);
# endif
#endif
   // autoload modules
   // try to load all modules in each directory in the autoDirList
   QoreString gstr;

   StringList::iterator w = autoDirList.begin();
   while (w != autoDirList.end())
   {
      // make new string for glob
      gstr.terminate(0);
      gstr.concat(*w);
      gstr.concat("/*.qmod");

      glob_t globbuf;
      if (!glob(gstr.getBuffer(), 0, NULL, &globbuf))
      {
	 for (int i = 0; i < (int)globbuf.gl_pathc; i++)
	 {
	    char *name = q_basename(globbuf.gl_pathv[i]);
	    // delete ".qmod" from module name for feature matching
	    char *p = strrchr(name, '.');
	    if (p)
	      *p = '\0';
	    class QoreString *errstr = loadModuleFromPath(globbuf.gl_pathv[i], name);
	    if (!errstr)
	       qoreFeatureList.push_back(name);
	    else
	    {
	       if (show_errors)
		  fprintf(stderr, "error loading %s\n", errstr->getBuffer());
	       delete errstr;
	    }
	    free(name);
	 }
      }
      else
	 printd(1, "ModuleManager::init(): glob returned an error: %s\n", strerror(errno));
      globfree(&globbuf);
      w++;
   }
}

class QoreString *ModuleManager::loadModule(char *name, class QoreProgram *pgm)
{
   // if the feature already exists in this program, then return
   if (pgm && !pgm->featureList.find(name))
      return NULL;

   // if the feature already exists, then load the namespace changes into this program and register the feature
   lock(); // make sure checking and loading are atomic
   class ModuleInfo *mi = find(name);
   if (mi)
   {
      if (pgm)
      {
	 mi->ns_init(pgm->getRootNS(), pgm->getQoreNS());
	 pgm->featureList.push_back(mi->getName());
      }
      unlock();
      return NULL;
   }

   // otherwise, try to find module in the module path
   class QoreString str;
   struct stat sb;
   class QoreString *errstr;

   StringList::iterator w = moduleDirList.begin();
   while (w != moduleDirList.end())
   {
      str.terminate(0);
      str.sprintf("%s/%s.qmod", *w, name);
      //printd(5, "ModuleManager::loadModule(%s) trying %s\n", name, str.getBuffer());

      if (!stat(str.getBuffer(), &sb))
      {
	 errstr = loadModuleFromPath(str.getBuffer(), name, &mi);
	 unlock();
	 
	 if (errstr)
	    return errstr;

	 if (pgm)
	 {
	    mi->ns_init(pgm->getRootNS(), pgm->getQoreNS());
	    pgm->featureList.push_back(mi->getName());
	 }
	 return NULL;
      }
      w++;
   }
   unlock();
   
   errstr = new QoreString;
   errstr->sprintf("feature '%s' is not builtin and no module with this name could be found in the module path", name);
   return errstr;
}

class QoreString *ModuleManager::loadModuleFromPath(char *path, char *feature, class ModuleInfo **mip)
{
   class ModuleInfo *mi = NULL;
   if (mip)
      *mip = NULL;

   class QoreString *str = NULL;
   void *ptr = dlopen(path, RTLD_LAZY);
   if (!ptr)
   {
      str = new QoreString();
      str->sprintf("error loading qore module '%s': %s\n", path, dlerror());
      printd(5, "%s\n", str->getBuffer());
      return str;
   }
   // get module name
   char *name = (char *)dlsym(ptr, "qore_module_name");
   if (!name)
   {
      str = new QoreString();
      str->sprintf("module '%s': no feature name present in module", path);
      dlclose(ptr);
      printd(5, "%s\n", str->getBuffer());
      return str;
   }
   // ensure provided feature matches with expected feature
   if (feature && strcmp(feature, name))
   {
      str = new QoreString();
      str->sprintf("module '%s': provides feature '%s', expecting feature '%s', skipping, rename module to %s.qmod to load", path, name, feature, feature);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // see if a module with this name is already registered
   if ((mi = find(name)))
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s' already registered by '%s'", path, name, mi->getFileName());
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }
   // get qore module API major number
   int *api_major = (int *)dlsym(ptr, "qore_module_api_major");
   if (!api_major)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': no qore module API major number", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get qore module API minor number
   int *api_minor = (int *)dlsym(ptr, "qore_module_api_minor");
   if (!api_minor)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': no qore module API minor number", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   if (*api_major != QORE_MODULE_API_MAJOR || (!(*api_major) && *api_minor != QORE_MODULE_API_MINOR))
   {
      str = new QoreString();
#if QORE_MODULE_API_MAJOR > 0
      str->sprintf("module '%s': feature '%s': API mismatch, module supports API %d.%d, however the minimum supported version is %d.0 is required", path, name, *api_major, *api_minor, QORE_MODULE_API_MAJOR);
#else
      str->sprintf("module '%s': feature '%s': API mismatch, module supports API %d.%d, however %d.%d is required", path, name, *api_major, *api_minor, QORE_MODULE_API_MAJOR, QORE_MODULE_API_MINOR);
#endif
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get initialization function
   qore_module_init_t *module_init = (qore_module_init_t *)dlsym(ptr, "qore_module_init");
   if (!module_init)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': claims API major=%d, %d required", path, name, *api_major, QORE_MODULE_API_MAJOR);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get namespace initialization function
   qore_module_ns_init_t *module_ns_init = (qore_module_ns_init_t *)dlsym(ptr, "qore_module_ns_init");
   if (!module_ns_init)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': missing namespace init method", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get deletion function
   qore_module_delete_t *module_delete = (qore_module_delete_t *)dlsym(ptr, "qore_module_delete");
   if (!module_delete)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': missing delete method", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get qore module description
   char *desc = (char *)dlsym(ptr, "qore_module_description");
   if (!desc)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': missing description", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get qore module version
   char *version = (char *)dlsym(ptr, "qore_module_version");
   if (!version)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': missing version", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get qore module author
   char *author = (char *)dlsym(ptr, "qore_module_author");
   if (!author)
   {
      str = new QoreString();
      str->sprintf("module '%s': feature '%s': missing author", path, name);
      dlclose(ptr);
      printd(5, "ModuleManager::loadModuleFromPath() error: %s\n", str->getBuffer());
      return str;
   }

   // get qore module URL (optional)
   char *url = (char *)dlsym(ptr, "qore_module_url");

   printd(5, "ModuleManager::loadModuleFromPath(%s) %s: calling module_init@%08p\n", path, name, *module_init);

   str = (*module_init)();
   // run initialization
   if (str) 
   {
      printd(5, "ModuleManager::loadModuleFromPath(%s): '%s': qore_module_init returned error: %s", path, name, str->getBuffer());
      QoreString desc;
      desc.sprintf("module '%s': feature '%s': ", path, name);
      dlclose(ptr);
      // insert text at beginning of string
      str->replace(0, 0, desc.getBuffer());
      return str;
   }

   mi = MM.add(path, name, *api_major, *api_minor, *module_init, *module_ns_init, *module_delete, desc, version, author, url, ptr);
   // add to auto namespace list
   ANSL.add(*module_ns_init);
   printd(5, "ModuleManager::loadModuleFromPath(%s) registered '%s'\n", path, name);
   if (mip)
      *mip = mi;
   return NULL;
}

void ModuleManager::cleanup()
{
   tracein("ModuleManager::cleanup()");

   while (head)
   {
      ModuleInfo *w = head->next;
      delete head;
      head = w;
   }

   traceout("ModuleManager::cleanup()");
}

class List *ModuleManager::getModuleList()
{
   if (!num)
      return NULL;

   class List *l = new List();
   class ModuleInfo *w = head;
   while (w)
   {
      if (!w->isBuiltin())
	 l->push(new QoreNode(w->getHash()));
      w = w->next;
   }
   return l;
}
