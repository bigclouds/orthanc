/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "ReusableDicomUserConnection.h"

#include "../../Core/OrthancException.h"

#include <glog/logging.h>

namespace Orthanc
{
  static boost::posix_time::ptime Now()
  {
    return boost::posix_time::microsec_clock::local_time();
  }

  void ReusableDicomUserConnection::Open(const std::string& remoteAet,
                                         const std::string& address,
                                         int port,
                                         ModalityManufacturer manufacturer)
  {
    if (connection_ != NULL &&
        connection_->GetRemoteApplicationEntityTitle() == remoteAet &&
        connection_->GetRemoteHost() == address &&
        connection_->GetRemotePort() == port &&
        connection_->GetRemoteManufacturer() == manufacturer)
    {
      // The current connection can be reused
      LOG(INFO) << "Reusing the previous SCU connection";
      return;
    }

    Close();

    connection_ = new DicomUserConnection();
    connection_->SetLocalApplicationEntityTitle(localAet_);
    connection_->SetRemoteApplicationEntityTitle(remoteAet);
    connection_->SetRemoteHost(address);
    connection_->SetRemotePort(port);
    connection_->SetRemoteManufacturer(manufacturer);
    connection_->Open();
  }
    
  void ReusableDicomUserConnection::Close()
  {
    if (connection_ != NULL)
    {
      delete connection_;
      connection_ = NULL;
    }
  }

  void ReusableDicomUserConnection::CloseThread(ReusableDicomUserConnection* that)
  {
    for (;;)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      if (!that->continue_)
      {
        //LOG(INFO) << "Finishing the thread watching the global SCU connection";
        return;
      }

      {
        boost::mutex::scoped_lock lock(that->mutex_);
        if (that->connection_ != NULL &&
            Now() >= that->lastUse_ + that->timeBeforeClose_)
        {
          LOG(INFO) << "Closing the global SCU connection after timeout";
          that->Close();
        }
      }
    }
  }
    
  ReusableDicomUserConnection::Locker::Locker(ReusableDicomUserConnection& that,
                                              const std::string& aet,
                                              const std::string& address,
                                              int port,
                                              ModalityManufacturer manufacturer) :
    ::Orthanc::Locker(that)
  {
    that.Open(aet, address, port, manufacturer);
    connection_ = that.connection_;
  }


  ReusableDicomUserConnection::Locker::Locker(ReusableDicomUserConnection& that,
                                              const RemoteModalityParameters& remote) :
    ::Orthanc::Locker(that)
  {
    that.Open(remote.GetApplicationEntityTitle(), remote.GetHost(), 
              remote.GetPort(), remote.GetManufacturer());
    connection_ = that.connection_;    
  }


  DicomUserConnection& ReusableDicomUserConnection::Locker::GetConnection()
  {
    if (connection_ == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    return *connection_;
  }      

  ReusableDicomUserConnection::ReusableDicomUserConnection() : 
    connection_(NULL), 
    timeBeforeClose_(boost::posix_time::seconds(5)),  // By default, close connection after 5 seconds
    localAet_("ORTHANC")
  {
    lastUse_ = Now();
    continue_ = true;
    closeThread_ = boost::thread(CloseThread, this);
  }

  ReusableDicomUserConnection::~ReusableDicomUserConnection()
  {
    continue_ = false;
    closeThread_.join();
    Close();
  }

  void ReusableDicomUserConnection::SetMillisecondsBeforeClose(uint64_t ms)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (ms == 0)
    {
      ms = 1;
    }

    timeBeforeClose_ = boost::posix_time::milliseconds(ms);
  }

  void ReusableDicomUserConnection::SetLocalApplicationEntityTitle(const std::string& aet)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Close();
    localAet_ = aet;
  }

  void ReusableDicomUserConnection::Lock()
  {
    mutex_.lock();
  }

  void ReusableDicomUserConnection::Unlock()
  {
    if (connection_ != NULL &&
        connection_->GetRemoteManufacturer() == ModalityManufacturer_StoreScp)
    {
      // "storescp" from DCMTK has problems when reusing a
      // connection. Always close.
      Close();
    }

    lastUse_ = Now();
    mutex_.unlock();
  }
}

