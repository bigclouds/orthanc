/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "OrthancConnection.h"

#include "../Core/OrthancException.h"

namespace OrthancClient
{
  void OrthancConnection::ReadPatients()
  {
    client_.SetUrl(orthancUrl_ + "/patients");
    Json::Value v;
    if (!client_.Apply(content_))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol);
    }
  }

  Orthanc::IDynamicObject* OrthancConnection::GetFillerItem(size_t index)
  {
    Json::Value::ArrayIndex tmp = static_cast<Json::Value::ArrayIndex>(index);
    return new Patient(*this, content_[tmp].asString());
  }

  Patient& OrthancConnection::GetPatient(unsigned int index)
  {
    return dynamic_cast<Patient&>(patients_.GetItem(index));
  }

  OrthancConnection::OrthancConnection(const char* orthancUrl) : 
    orthancUrl_(orthancUrl), patients_(*this)
  {
    ReadPatients();
  }
  
  OrthancConnection::OrthancConnection(const char* orthancUrl,
                                       const char* username, 
                                       const char* password) : 
    orthancUrl_(orthancUrl), patients_(*this)
  {
    client_.SetCredentials(username, password);
    ReadPatients();
  }


}