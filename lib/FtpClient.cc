/*
  FtpClient.cc
  
  thread-safe Qore FtpClient object
  
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
#include <qore/QoreLib.h>
#include <qore/FtpClient.h>
#include <qore/QoreURL.h>

class FtpResp
{
   private:
      class QoreString *str;
   
   public:
      DLLLOCAL inline FtpResp() : str(NULL) {}
      DLLLOCAL inline FtpResp(class QoreString *s)
      {
	 str = s;
      }
      DLLLOCAL inline ~FtpResp()
      {
	 if (str)
	    delete str;
      }
      DLLLOCAL inline class QoreString *assign(class QoreString *s)
      {
	 if (str)
	    delete str;
	 str = s;
	 return s;
      }
      DLLLOCAL inline char *getBuffer()
      {
	 return str->getBuffer();
      }
      DLLLOCAL inline class QoreString *getStr()
      {
	 return str;
      }
      DLLLOCAL inline int getCode()
      {
	 if (!str || str->strlen() < 3)
	    return -1;
   
	 char buf[4];
	 buf[0] = str->getBuffer()[0];
	 buf[1] = str->getBuffer()[1];
	 buf[2] = str->getBuffer()[2];
	 buf[3] = '\0';
	 return atoi(buf);
      }
      DLLLOCAL inline void stripEOL()
      {
	 if (!str || !str->strlen())
	    return;
	 if (str->getBuffer()[str->strlen() - 1] == '\n')
	    str->terminate(str->strlen() - 1);
	 if (str->getBuffer()[str->strlen() - 1] == '\r')
	    str->terminate(str->strlen() - 1);
      }
};

FtpClient::FtpClient(class QoreString *url, class ExceptionSink *xsink)
{
   control_connected = loggedin = false;
   mode = FTP_MODE_UNKNOWN;
   transfer_mode = "I";
   port = DEFAULT_FTP_CONTROL_PORT;
   user = pass = host = NULL;
   secure = secure_data = false;

   if (url)
      setURLInternal(url, xsink);
}

FtpClient::~FtpClient()
{
   // clear control buffer
   buffer.clear();
   disconnectInternal();
   if (host)
      free(host);
   if (user)
      free(user);
   if (pass)
      free(pass);
}

// private unlocked
static inline int getFTPCode(QoreString *str)
{
   QoreString *b = str->substr(0, 3);
   if (!b) return -1;
   int rc = atoi(b->getBuffer());
   delete b;
   return rc;
}

// private unlocked
class QoreString *FtpClient::sendMsg(char *cmd, char *arg, class ExceptionSink *xsink)
{
   QoreString c(cmd);
   if (arg)
   {
      c.concat(' ');
      c.concat(arg);
   }
   c.concat("\r\n");
   printd(FTPDEBUG, "FtpClient::sendMsg()> %s", c.getBuffer());
   if (control.send(c.getBuffer(), c.strlen()) < 0)
   {
      xsink->raiseException("FTP-SEND-ERROR", strerror(errno));
      return NULL;
   }
   
   QoreString *resp = getResponse(xsink);
   return resp;
}

// private unlocked
void FtpClient::stripEOL(class QoreString *str)
{
   if (!str || !str->strlen())
      return;
   if (str->getBuffer()[str->strlen() - 1] == '\n')
      str->terminate(str->strlen() - 1);
   if (str->getBuffer()[str->strlen() - 1] == '\r')
      str->terminate(str->strlen() - 1);
}

// private unlocked
int FtpClient::setBinaryMode(bool t, class ExceptionSink *xsink)
{
   // set transfer mode
   QoreString *resp = sendMsg("TYPE", (char *)(t ? "I" : "A"), xsink);
   if (xsink->isEvent())
      return -1;
   int code = getFTPCode(resp);
   if ((code / 100) != 2)
   {
      xsink->raiseException("FTP-ERROR", "can't set mode to '%c', FTP server responded: %s",
			    (t ? 'I' : 'A'), resp->getBuffer());
      delete resp;
      return -1;
   }
   delete resp;
   return 0;
}

// private unlocked
int FtpClient::acceptDataConnection(class ExceptionSink *xsink)
{
   if (data.acceptAndReplace(NULL))
   {
      data.close();
      xsink->raiseException("FTP-CONNECT-ERROR", "error accepting data connection: %s", 
			    strerror(errno));
      return -1;
   }
#ifdef DEBUG
   if (secure_data)
      printd(FTPDEBUG, "FtpClient::connectDataPort() negotiating client SSL connection\n");
#endif
   
   if (secure_data && data.upgradeClientToSSL(NULL, NULL, xsink))
      return -1;      
   
   printd(FTPDEBUG, "FtpClient::acceptDataConnection() accepted PORT data connection\n");
   return 0;
}

// private unlocked
int FtpClient::connectData(class ExceptionSink *xsink)
{
   switch (mode)
   {
      case FTP_MODE_UNKNOWN:
	 if (!connectDataExtendedPassive(xsink))
	    return 0;
	 if (xsink->isEvent())
	    return -1;
	    if (!connectDataPassive(xsink))
	       return 0;
	       if (xsink->isEvent())
		  return -1;
		  if (!connectDataPort(xsink))
		     return 0;
		     
		     if (!xsink->isEvent())
			xsink->raiseException("FTP-CONNECT-ERROR", "Could not negotiate data channel connection with FTP server");
			return -1;
      case FTP_MODE_EPSV:
	 return connectDataExtendedPassive(xsink);
      case FTP_MODE_PASV:
	 return connectDataPassive(xsink);
      case FTP_MODE_PORT:
	 return connectDataPort(xsink);
   }
   return -1;
}

// private unlocked
int FtpClient::disconnectInternal()
{
   control.close();
   control_connected = false;
   mode = FTP_MODE_UNKNOWN;
   data.close();
   return 0;
}

// public locked
int FtpClient::disconnect()
{
   lock();
   int rc = disconnectInternal();
   unlock();
   return rc;
}

// private unlocked
void FtpClient::setURLInternal(class QoreString *url_str, class ExceptionSink *xsink)
{
   QoreURL url(url_str);
   if (!url.isValid())
   {
      xsink->raiseException("FTP-URL-ERROR", "no hostname given in URL '%s'", url_str->getBuffer());
      return;
   }

   // verify protocol
   if (url.getProtocol())
   {
      if (!url.getProtocol()->compare("ftps"))
	 secure = secure_data = true;
      else if (url.getProtocol()->compare("ftp"))
      {
	 xsink->raiseException("UNSUPPORTED-PROTOCOL", "'%s' not supported (expected 'ftp' or 'ftps')", url.getProtocol()->getBuffer());
	 return;
      }
   }

   // set username
   user = url.take_username();   
   // set password
   pass = url.take_password();
   // set host
   host = url.take_host();
   port = url.getPort() ? url.getPort() : DEFAULT_FTP_CONTROL_PORT;
}

// private unlocked
class QoreString *FtpClient::getResponse(class ExceptionSink *xsink)
{
   QoreString *resp;
   int rc;
   // if there is data in the buffer, then take it, otherwise read
   if (!buffer.strlen())
      resp = control.recv(-1, &rc);
   else
   {
      resp = new QoreString(&buffer);
      buffer.clear();
   }
   // see if we got the whole response
   if (resp && resp->getBuffer())
   {
      char *start = resp->getBuffer();
      char *p = start;
      while (1)
      {
	 if ((*p) == '\n')
	 {
	    if (p > (start + 3))
	    {
	       // if we got the whole response
	       if (isdigit(*start) && isdigit(start[1]) && isdigit(start[2]) && start[3] == ' ')
	       {
		  // if we read more data, then store it in the buffer
		  if (p[1] != '\0')
		  {
		     buffer.set(&p[1]);
		     resp->terminate(p - resp->getBuffer() + 1); 
		  }
		  break;
	       }
	    }
	    start = p + 1;
	 }
	 // if we have not got the whole message
	 else if (*p == '\0')
	 {
	    QoreString *r = control.recv(-1, &rc);
	    if (!r)
	    {
	       delete resp;
	       xsink->raiseException("FTP-RECEIVE-ERROR", "short message received on control port");
	       return NULL;
	    }
	    // in case the buffer gets reallocated
	    int pos = p - resp->getBuffer();
	    resp->concat(r);
	    delete r;
	    p = resp->getBuffer() + pos;
	 }
	 p++;
      }
   }
   printd(FTPDEBUG, "FtpClient::getResponse() %s", resp ? resp->getBuffer() : "NULL");
   return resp;
}

/*
// RFC 1639 Long Passive Mode
int FtpClient::connectDataLongPassive(class ExceptionSink *xsink)
{
   // try extended passive mode
   class QoreString *resp = sendMsg("LPSV", NULL, xsink);
   if ((getFTPCode(resp) / 100) != 2)
   {
      delete resp;
      return -1;
   }

   // ex: 228 Entering Long Passive Mode (6,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,247,239)
   // get port for data connection
}
*/

// private unlocked
// RFC 2428 Extended Passive Mode
int FtpClient::connectDataExtendedPassive(class ExceptionSink *xsink)
{
   // try extended passive mode
   class FtpResp resp(sendMsg("EPSV", NULL, xsink));
   if ((resp.getCode() / 100) != 2)
      return -1;

   // ex: 229 Entering Extended Passive Mode (|||63519|)
   // get port for data connection
   char *s = strstr(resp.getBuffer(), "|||");
   if (!s)
   {
      resp.stripEOL();
      xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp.getBuffer());
      return -1;
   }
   s += 3;
   char *end = strchr(s, '|');
   if (!end)
   {
      resp.stripEOL();
      xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp.getBuffer());
      return -1;
   }
   *end = '\0';

   int data_port = atoi(s);
   if (data.connectINET(host, data_port))
   {
      xsink->raiseException("FTP-CONNECT-ERROR", "could not connect to passive data port (%s:%d): %s", host, data_port,
		     strerror(errno));
      return -1;
   }
   printd(FTPDEBUG, "EPSV connected to %s:%d\n", host, data_port);

   mode = FTP_MODE_EPSV;
   return 0;
}

// private unlocked
int FtpClient::connectDataPassive(class ExceptionSink *xsink)
{
   // try passive mode
   class FtpResp resp;
   resp.assign(sendMsg("PASV", NULL, xsink));
   if ((resp.getCode() / 100) != 2)
      return -1;

   // reply ex: 227 Entering passive mode (127,0,0,1,28,46)  
   // get port for data connection
   char *s = strstr(resp.getBuffer(), "(");
   if (!s)
   {
      resp.stripEOL();
      xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp.getBuffer());
      return -1;
   }
   int num[5];
   s++;
   char *comma;
   for (int i = 0; i < 5; i++)
   {
      comma = strchr(s, ',');
      if (!comma)
      {
	 resp.stripEOL();
	 xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp.getBuffer());
	 return -1;
      }
      num[i] = atoi(s);
      s = comma + 1;
   }
   int dataport = (num[4] << 8) + atoi(s);
   class QoreString ip;
   ip.sprintf("%d.%d.%d.%d", num[0], num[1], num[2], num[3]);
   printd(FTPDEBUG,"FtpClient::connectPassive() address: %s:%d\n", ip.getBuffer(), dataport);

   if (data.connectINET(ip.getBuffer(), dataport))
   {
      xsink->raiseException("FTP-CONNECT-ERROR", "could not connect to passive data port (%s:%d): %s", 
			    ip.getBuffer(), dataport, strerror(errno));
      return -1;
   }

   if (secure_data && data.upgradeClientToSSL(NULL, NULL, xsink))
      return -1;      

   mode = FTP_MODE_PASV;
   return 0;
}

// private unlocked
int FtpClient::connectDataPort(class ExceptionSink *xsink)
{
   // get address for interface of control connection
   struct sockaddr_in add;
#ifdef HPUX_ACC_SOCKLEN_HACK
   int socksize = sizeof(struct sockaddr_in);
#else
   socklen_t socksize = sizeof(struct sockaddr_in);
#endif
   
   if (getsockname(control.getSocket(), (struct sockaddr *)&add, &socksize) < 0)
   {
      xsink->raiseException("FTP-CONNECT-ERROR", "cannot determine local interface address for data port connection: %s",
		     strerror(errno));
      return -1;
   }
   // bind to any port on local interface
   add.sin_port = 0;
   if (data.bind((struct sockaddr *)&add, sizeof (struct sockaddr_in)))
   {
      xsink->raiseException("FTP-CONNECT-ERROR", "could not bind to any port on local interface: %s", 
		     strerror(errno));
      return -1;
   }
   // get port number
   int dataport = data.getPort();

   // get ip address
   char ifname[80];
   if (!inet_ntop(AF_INET, &((struct sockaddr_in *)&add)->sin_addr, ifname, sizeof(ifname)))
   {
      data.close();
      xsink->raiseException("FTP-CONNECT-ERROR", "cannot determine local interface address for data port connection: %s",
		     strerror(errno));
      return -1;
   }
   printd(FTPDEBUG, "FtpClient::connectDataPort() requesting connection to %s:%d\n", ifname, dataport);
   // change dots to commas for PORT message
   for (int i = 0; ifname[i]; i++)
      if (ifname[i] == '.')
	 ifname[i] = ',';

   QoreString pconn;
   pconn.sprintf("%s,%d,%d", ifname, dataport >> 8, dataport & 255);
   FtpResp resp(sendMsg("PORT", pconn.getBuffer(), xsink));
   if (xsink->isEvent())
   {
      data.close();
      return -1;
   }

   // ex: 200 PORT command successful.
   if ((resp.getCode() / 100) != 2)
   {
      data.close();
      return -1;
   }
   
   if (data.listen())
   {
      data.close();
      xsink->raiseException("FTP-CONNECT-ERROR", "error listening on data connection: %s", 
			    strerror(errno));
      return -1;
   }
   printd(FTPDEBUG, "FtpClient::connectDataPort() listening on port %d\n", dataport);

   mode = FTP_MODE_PORT;
   return 0;
}

// private unlocked
int FtpClient::connectIntern(class FtpResp *resp, class ExceptionSink *xsink)
{
   // connect to FTP port on remote machine
   if (control.connectINET(host, port))
   {
      if (port != DEFAULT_FTP_CONTROL_PORT)
	 xsink->raiseException("FTP-CONNECT-ERROR", "could not connect to ftp%s://%s:%d", secure ? "s" : "", host, port);
      else
	 xsink->raiseException("FTP-CONNECT-ERROR", "could not connect to ftp%s://%s", secure ? "s" : "", host);

      return -1;
   }

   control_connected = 1;

   int rc;
   resp->assign(control.recv(-1, &rc));
   if (xsink->isEvent())
      return -1;

   printd(FTPDEBUG, "FtpClient::connectIntern() %s", resp->getBuffer());

   // ex: 220 (vsFTPd 2.0.1)
   // ex: 220 localhost FTP server (tnftpd 20040810) ready.
   // etc
   if ((resp->getCode() / 100) != 2)
   {
      resp->stripEOL();
      xsink->raiseException("FTP-CONNECT-ERROR", "FTP server reported the following error: %s",
			    resp->getBuffer());
      return -1;
   }

   return 0;
}

// do PBSZ and PROT commands
int FtpClient::doProt(class FtpResp *resp, class ExceptionSink *xsink)
{
   // RFC-4217: PBSZ 0 for streaming data
   resp->assign(sendMsg("PBSZ", "0", xsink));
   if (xsink->isEvent())
      return -1;
   int code = resp->getCode();
   if (code != 200)
   {
      resp->stripEOL();
      xsink->raiseException("FTPS-SECURE-DATA-ERROR", "response from FTP server to PBSZ 0 command: %s", resp->getBuffer());
      return -1;
   }

   resp->assign(sendMsg("PROT", "P", xsink));
   if (xsink->isEvent())
      return -1;
   code = resp->getCode();
   if (code != 200)
   {
      resp->stripEOL();
      xsink->raiseException("FTPS-SECURE-DATA-ERROR", "response from FTP server to PROT P command: %s", resp->getBuffer());
      return -1;
   }

   return 0;
}

// private unlocked
int FtpClient::doAuth(class FtpResp *resp, class ExceptionSink *xsink)
{
   resp->assign(sendMsg("AUTH", "TLS", xsink));
   if (xsink->isEvent())
      return -1;
   int code = resp->getCode();

   if (code != 234)
   {
      // RFC-2228 ADAT exchange not supported
      if (code == 334)
	 xsink->raiseException("FTPS-AUTH-ERROR", "server requires unsupported ADAT exchange");
      else
      {
	 resp->stripEOL();
	 xsink->raiseException("FTPS-AUTH-ERROR", "response from FTP server: %s", resp->getBuffer());
      }
      return -1;
   }
   
   if (control.upgradeClientToSSL(NULL, NULL, xsink))
      return -1;

   if (secure_data)
      return doProt(resp, xsink);

   return 0;
}

// public locked
int FtpClient::connect(class ExceptionSink *xsink)
{
   lock();

   disconnectInternal();

   if (!host)
   {
      unlock();
      xsink->raiseException("FTP-CONNECT-ERROR", "no hostname set");
      return -1;
   }

   FtpResp resp;
   if (connectIntern(&resp, xsink))
   {
      unlock();
      return -1;
   }

   if (secure && doAuth(&resp, xsink))
   {
      unlock();
      return -1;
   }

   resp.assign(sendMsg("USER", user ? user : (char *)DEFAULT_USERNAME, xsink));
   if (xsink->isEvent())
   {
      unlock();
      return -1;
   }
   int code = resp.getCode();

   // if user not logged in immediately, continue
   if ((code / 100) != 2)
   {
      // if there is an error, then exit
      if (code != 331)
      {
	 unlock();
	 resp.stripEOL();
	 xsink->raiseException("FTP-LOGIN-ERROR", "response from FTP server: %s", resp.getBuffer());
	 return -1;
      }

      // send password
      resp.assign(sendMsg("PASS", pass ? pass : (char *)DEFAULT_PASSWORD, xsink));
      if (xsink->isEvent())
	 return -1;

      code = resp.getCode();

      // if user not logged in for whatever reason, then exit
      if ((code / 100) != 2)
      {
	 unlock();
	 resp.stripEOL();
	 xsink->raiseException("FTP-LOGIN-ERROR", "response from FTP server: %s", resp.getBuffer());
	 return -1;
      }
   }

   loggedin = true;

   unlock();
   return 0;
}

// public locked
class QoreString *FtpClient::list(char *path, bool long_list, class ExceptionSink *xsink)
{
   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before FtpClient::%s()",
		     (long_list ? "list" : "nlst"));
      return NULL;
   }

   if (setBinaryMode(false, xsink) || connectData(xsink))
   {
      unlock();
      return NULL;
   }

   FtpResp resp(sendMsg((char *)(long_list ? "LIST" : "NLST"), path, xsink));
   if (xsink->isEvent())
   {
      unlock();
      return NULL;
   }

   int code = resp.getCode();
   //printf("LIST: %s", resp->getBuffer());
   // file not found or similar
   if ((code / 100 == 5))
   {
      unlock();
      data.close();
      return NULL;
   }

   if ((code / 100 != 1))
   {
      unlock();
      data.close();
      resp.stripEOL();
      xsink->raiseException("FTP-LIST-ERROR", "FTP server returned an error to the %s command: %s",
			    (long_list ? "LIST" : "NLST"), resp.getBuffer());
      return NULL;
   }

   if ((mode == FTP_MODE_PORT && acceptDataConnection(xsink)) || xsink->isEvent())
   {
      unlock();
      data.close();
      return NULL;
   }
   else if (secure_data && data.upgradeClientToSSL(NULL, NULL, xsink))
      return NULL;

   QoreString *l = new QoreString();

   // read until done
   while (1)
   {
      int rc;
      if (!resp.assign(data.recv(-1, &rc)))
	 break;
      //printf("%s", resp->getBuffer());
      l->concat(resp.getStr());
   }
   data.close();
   resp.assign(getResponse(xsink));
   unlock();
   if (xsink->isEvent())
      return NULL;

   code = resp.getCode();

   //printf("LIST: %s", resp->getBuffer());
   if ((code / 100 != 2))
   {
      resp.stripEOL();
      xsink->raiseException("FTP-LIST-ERROR", "FTP server returned an error to the %s command: %s", 
		     (long_list ? "LIST" : "NLST"), resp.getBuffer());
      delete l;
      return NULL;
   }
   return l;
}

// public locked
int FtpClient::put(char *localpath, char *remotename, class ExceptionSink *xsink)
{
   printd(5, "FtpClient::put(%s, %s)\n", localpath, remotename ? remotename : "NULL");

   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before the FtpClient::put()");
      return -1;
   }

   int fd = open(localpath, O_RDONLY, 0);
   if (fd < 0)
   {
      unlock();
      xsink->raiseException("FTP-FILE-OPEN-ERROR", "%s: %s", localpath, strerror(errno));
      return -1;
   }

   // set binary mode and establish data connection
   if (setBinaryMode(true, xsink) || connectData(xsink))
   {
      unlock();
      close(fd);
      return -1;
   }

   // get file size
   struct stat file_info;
   if (fstat(fd, &file_info) == -1)
   {
      unlock();
      close(fd);
      xsink->raiseException("FTP-FILE-PUT-ERROR", "could not get file size: %s", strerror(errno));
      return -1;
   }

   // get remote file name
   char *rn;
   if (remotename)
      rn = remotename;
   else
      rn = q_basename(localpath);

   // transfer file
   FtpResp resp(sendMsg("STOR", rn, xsink));
   if (rn != remotename)
      free(rn);
   if (xsink->isEvent())
   {
      unlock();
      data.close();
      close(fd);
      return -1;
   }
   //printf("%s", resp->getBuffer());

   if ((resp.getCode() / 100) != 1)
   {
      unlock();
      data.close();
      resp.stripEOL();
      xsink->raiseException("FTP-PUT-ERROR", "could not put file, FTP server replied: %s", 
			    resp.getBuffer());
      close(fd);
      return -1;
   }

   if ((mode == FTP_MODE_PORT && acceptDataConnection(xsink)) || xsink->isEvent())
   {
      unlock();
      data.close();
      close(fd);
      return -1;
   }
   else if (secure_data && data.upgradeClientToSSL(NULL, NULL, xsink))
      return -1;      

   int rc = data.send(fd, file_info.st_size ? file_info.st_size : -1);
   data.close();
   close(fd);

   resp.assign(getResponse(xsink));
   unlock();
   if (xsink->isEvent())
      return -1;

   //printf("PUT: %s", resp->getBuffer());
   if ((resp.getCode() / 100 != 2))
   {
      resp.stripEOL();
      xsink->raiseException("FTP-PUT-ERROR", "FTP server returned an error to the PUT command: %s", resp.getBuffer());
      return -1;
   }   

   if (rc)
   {
      xsink->raiseException("FTP-PUT-ERROR", "error sending file, may not be complete on target");
      return -1;
   }
   return 0;
}

// public locked
int FtpClient::get(char *remotepath, char *localname, class ExceptionSink *xsink)
{
   printd(5, "FtpClient::get(%s, %s)\n", remotepath, localname ? localname : "NULL");

   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before the FtpClient::get()");
      return -1;
   }

   // get local file name
   char *ln;
   if (localname)
      ln = localname;
   else
      ln = q_basename(remotepath);

   printd(FTPDEBUG, "FtpClient::get(%s) %s\n", remotepath, ln);
   // open local file
   int fd = open(ln, O_WRONLY|O_CREAT, 0644);
   if (fd < 0)
   {
      unlock();
      xsink->raiseException("FTP-FILE-OPEN-ERROR", "%s: %s", ln, strerror(errno));
      if (ln != localname)
	 free(ln);
      return -1;
   }

   // set binary mode and establish data connection
   if (setBinaryMode(true, xsink) || connectData(xsink))
   {
      unlock();
      // delete temporary file
      unlink(ln);
      if (ln != localname)
	 free(ln);
      close(fd);
      return -1;
   }

   // transfer file
   FtpResp resp(sendMsg("RETR", remotepath, xsink));
   if (xsink->isEvent())
   {
      unlock();
      // delete temporary file
      unlink(ln);
      if (ln != localname)
	 free(ln);
      data.close();
      close(fd);
      return -1;
   }
   //printf("%s", resp->getBuffer());

   if ((resp.getCode() / 100) != 1)
   {
      unlock();
      // delete temporary file
      unlink(ln);
      if (ln != localname)
	 free(ln);
      data.close();
      close(fd);
      resp.stripEOL();
      xsink->raiseException("FTP-GET-ERROR", "could not retrieve file, FTP server replied: %s", 
			    resp.getBuffer());
      return -1;
   }

   if ((mode == FTP_MODE_PORT && acceptDataConnection(xsink)) || xsink->isEvent())
   {
      unlock();
      // delete temporary file
      unlink(ln);
      if (ln != localname)
	 free(ln);
      data.close();
      close(fd);
      return -1;
   }
   else if (secure_data && data.upgradeClientToSSL(NULL, NULL, xsink))
      return -1;      

   if (ln != localname)
      free(ln);

   data.recv(fd, -1, -1);
   data.close();
   close(fd);

   resp.assign(getResponse(xsink));
   unlock();
   if (xsink->isEvent())
      return -1;

   //printf("PUT: %s", resp->getBuffer());
   if ((resp.getCode() / 100 != 2))
   {
      resp.stripEOL();
      xsink->raiseException("FTP-GET-ERROR", "FTP server returned an error to the RETR command: %s", 
			    resp.getBuffer());
      return -1;
   }
   return 0;
}

// public locked
int FtpClient::cwd(char *dir, class ExceptionSink *xsink)
{
   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before the FtpClient::cwd()");
      return -1;
   }
   class QoreString *p = sendMsg("CWD", dir, xsink);
   unlock();
   if (xsink->isEvent())
      return -1;

   if ((getFTPCode(p) / 100) == 2)
   {
      delete p;
      return 0;
   }
   stripEOL(p);
   xsink->raiseException("FTP-CWD-ERROR", "FTP server returned an error to the CWD command: %s", p->getBuffer());
   delete p;
   return -1;
}

// public locked
class QoreString *FtpClient::pwd(class ExceptionSink *xsink)
{
   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before the FtpClient::pwd()");
      return NULL;
   }

   class QoreString *p = sendMsg("PWD", NULL, xsink);
   unlock();
   if ((getFTPCode(p) / 100) == 2)
   {
      QoreString *rv = p->substr(4);
      stripEOL(rv);
      delete p;
      return rv;
   }
   stripEOL(p);
   xsink->raiseException("FTP-PWD-ERROR", "FTP server returned an error response to the PWD command: %s", p->getBuffer());
   delete p;
   return NULL;
}

// public locked
int FtpClient::del(char *file, class ExceptionSink *xsink)
{
   lock();
   if (!loggedin)
   {
      unlock();
      xsink->raiseException("FTP-NOT-CONNECTED", "FtpClient::connect() must be called before the FtpClient::delete()");
      return -1;
   }
   class QoreString *p = sendMsg("DELE", file, xsink);
   unlock();
   if (xsink->isEvent())
      return -1;

   if ((getFTPCode(p) / 100) == 2)
   {
      delete p;
      return 0;
   }
   stripEOL(p);
   xsink->raiseException("FTP-DELETE-ERROR", "FTP server returned an error to the DELE command: %s", p->getBuffer());
   delete p;
   return -1;
}

void FtpClient::setURL(class QoreString *url, class ExceptionSink *xsink)
{
   lock();
   setURLInternal(url, xsink);
   unlock();
}

class QoreString *FtpClient::getURL() const
{
   class QoreString *url = new QoreString("ftp://");
   if (user)
   {
      url->concat(user);
      if (pass)
	 url->sprintf(":%s", pass);
      url->concat('@');
   }
   if (host)
      url->concat(host);
   if (port)
      url->sprintf(":%d", port);
   return url;
}

void FtpClient::setPort(int p)
{ 
   port = p; 
}

void FtpClient::setUserName(char *u) 
{ 
   lock();
   if (user) 
      free(user); 
   user = u ? strdup(u) : NULL;
   unlock();
}

void FtpClient::setPassword(char *p) 
{ 
   lock();
   if (pass)
      free(pass); 
   pass = p ? strdup(p) : NULL;
   unlock();
}

void FtpClient::setHostName(char *h) 
{ 
   lock();
   if (host) 
      free(host); 
   host = h ? strdup(h) : NULL;
   unlock();
}

int FtpClient::setSecure()
{
   lock();
   if (control_connected)
   {
      unlock();
      return -1;
   }
   secure = secure_data = true;
   unlock();
   return 0;
}

int FtpClient::setInsecure()
{
   lock();
   if (control_connected)
   {
      unlock();
      return -1;
   }
   secure = secure_data = false;
   unlock();
   return 0;
}

int FtpClient::setInsecureData()
{
   lock();
   if (control_connected)
   {
      unlock();
      return -1;
   }
   secure_data = false;
   unlock();
   return 0;
}

// returns true if the control connection can only be established with a secure connection
bool FtpClient::isSecure() const
{
   return secure;
}

// returns true if data connections can only be established with a secure connection
bool FtpClient::isDataSecure() const
{
   return secure_data;
}

const char *FtpClient::getSSLCipherName() const
{
   return control.getSSLCipherName();
}

const char *FtpClient::getSSLCipherVersion() const
{
   return control.getSSLCipherVersion();
}

long FtpClient::verifyPeerCertificate() const
{
   return control.verifyPeerCertificate();
}	 

void FtpClient::setModeAuto()
{
   lock();
   mode = FTP_MODE_UNKNOWN;
   unlock();
}

void FtpClient::setModeEPSV()
{
   lock();
   mode = FTP_MODE_EPSV;
   unlock();
}

void FtpClient::setModePASV()
{
   lock();
   mode = FTP_MODE_PASV;
   unlock();
}

void FtpClient::setModePORT()
{
   lock();
   mode = FTP_MODE_PORT;
   unlock();
}

int FtpClient::getPort() const 
{
   return port; 
}

char *FtpClient::getUserName() const 
{ 
   return user;
}

char *FtpClient::getPassword() const 
{ 
   return pass; 
}

char *FtpClient::getHostName() const 
{ 
   return host; 
}
