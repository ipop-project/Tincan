/*
* ipop-project
* Copyright 2016, University of Florida
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/
#include "control_dispatch.h"
#include "webrtc/base/json.h"
#include "tincan_exception.h"
#include "vnet_descriptor.h"

namespace tincan
{
using namespace rtc;
ControlDispatch::ControlDispatch() :
  dtol_(nullptr),
  ctrl_link_(make_shared<DisconnectedControllerHandle>())
{
  control_map_ = {
    { "ConfigureLogging", &ControlDispatch::ConfigureLogging },
    { "CreateCtrlRespLink", &ControlDispatch::CreateIpopControllerRespLink },
    { "CreateLink", &ControlDispatch::CreateLink },
    { "CreateOverlay", &ControlDispatch::CreateOverlay },
    { "Echo", &ControlDispatch::Echo },
    { "SendIcc", &ControlDispatch::SendIcc },
    { "InjectFrame", &ControlDispatch::InjectFrame },
    { "QueryCandidateAddressSet", &ControlDispatch::QueryCandidateAddressSet },
    { "QueryLinkStats", &ControlDispatch::QueryLinkStats },
    { "QueryOverlayInfo", &ControlDispatch::QueryOverlayInfo },
    { "RemoveOverlay", &ControlDispatch::RemoveOverlay },
    { "RemoveLink", &ControlDispatch::RemoveLink },
    { "SetIgnoredNetInterfaces", &ControlDispatch::SetNetworkIgnoreList },
    //{ "UpdateMap", &ControlDispatch::UpdateRoutes },
  };
}
ControlDispatch::~ControlDispatch()
{
  LogMessage::RemoveLogToStream(log_sink_.get());
}

void
ControlDispatch::operator () (TincanControl & control)
{
  try {
    switch(control.GetControlType()) {
    case TincanControl::CTTincanRequest:
      (this->*control_map_.at(control.GetCommand()))(control);
      break;
    case TincanControl::CTTincanResponse:
    // todo: A controller response to something sent earlier
      break;
    default:
      LOG(LS_WARNING) << "Unrecognized control type received and discarded.";
      break;
    }
  }
  catch(out_of_range & e) {
    LOG(LS_WARNING) << "An invalid IPOP control operation was received and "
      "discarded: " << control.StyledString() << "Exception=" << e.what();
  }
  catch(exception & e)
  {
    LOG(LS_WARNING) << e.what();
  }
}
void
ControlDispatch::SetDispatchToTincanInf(
  TincanDispatchInterface * dtot)
{
  tincan_ = dtot;
}
void
ControlDispatch::SetDispatchToListenerInf(
  DispatchToListenerInf * dtol)
{
  dtol_ = dtol;
}

void
ControlDispatch::ConfigureLogging(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  string log_lvl = req["Level"].asString();
  string msg("Tincan logging successfully configured.");
  bool status = true;
  try
  {
    ostringstream oss;
    std::transform(log_lvl.begin(), log_lvl.end(), log_lvl.begin(), ::tolower);
    oss << "tstamp " << "thread " << log_lvl.c_str();
    LogMessage::ConfigureLogging(oss.str().c_str());
    LogMessage::LogToDebug(LS_WARNING);
    LogMessage::SetLogToStderr(true);
    if(req["Device"].asString() == "All" || req["Device"].asString() == "File")
    {
      LogMessage::SetLogToStderr(false);
      string dir = req["Directory"].asString();
      rtc::Pathname pn(dir);
      if(!Filesystem::IsFolder(pn))
        Filesystem::CreateFolder(pn);
      string fn = req["Filename"].asString();
      size_t max_sz = req["MaxFileSize"].asUInt64();
      size_t num_fls = req["MaxArchives"].asUInt64();
      log_sink_ = make_unique<FileRotatingLogSink>(dir, fn, max_sz, num_fls);
      log_sink_->Init();
      log_lvl = req["Level"].asString();
      LogMessage::AddLogToStream(log_sink_.get(), GetLogLevel(log_lvl));
    }
    if(req["Device"].asString() == "All" ||
      req["Device"].asString() == "Console")
    {
      if(req["ConsoleLevel"].asString().length() > 0)
        log_lvl = req["ConsoleLevel"].asString();
      else if(req["Level"].asString().length() > 0)
        log_lvl = req["Level"].asString();
      LogMessage::LogToDebug(GetLogLevel(log_lvl));
      LogMessage::SetLogToStderr(true);
    }
  } catch(exception &)
  {
    LogMessage::LogToDebug(LS_WARNING);
    LogMessage::SetLogToStderr(true);
    msg = "The configure logging operation failed. It defaults to Console/WARNING";
    LOG(LS_WARNING) << msg;
    status = false;
  }
  control.SetResponse(msg, status);
  ctrl_link_->Deliver(control);
}

void
ControlDispatch::CreateLink(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  string msg("Connection to peer node in progress.");
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->CreateVlink(req, control);
    status = true;
  } catch(exception & e)
  {
    msg = "The ConnectTunnel operation failed.";
    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  if(!status)
  {
    control.SetResponse(msg, status);
    ctrl_link_->Deliver(control);
  } //else respond when CAS is available
}

void ControlDispatch::CreateIpopControllerRespLink(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  string ip = req["IP"].asString();
  int port = req["Port"].asInt();
  string msg("Controller endpoint successfully created.");
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    unique_ptr<SocketAddress> ctrl_addr(new SocketAddress(ip, port));
    dtol_->CreateIpopControllerLink(move(ctrl_addr));
    ctrl_link_.reset(&dtol_->GetIpopControllerLink());
    tincan_->SetIpopControllerLink(ctrl_link_);
    control.SetResponse(msg, true);
    ctrl_link_->Deliver(control);
  }
  catch(exception & e)
  {
    //if this fails we can't indicate this to the controller so log with
    //high severity
    LOG(LS_ERROR) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
}

//void
//ControlDispatch::CreateTunnel(
//  TincanControl & control)
//{
//  Json::Value & req = control.GetRequest();
//  lock_guard<mutex> lg(disp_mutex_);
//  try
//  {
//    tincan_->CreateTunnel(req); 
//  } catch(exception & e)
//  {
//    string msg = "The CreateTunnel operation failed.";
//    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
//      control.StyledString();
//    //send fail here, send the cas when the op completes
//    control.SetResponse(msg, false);
//    ctrl_link_->Deliver(control);
//  }
//}

void
ControlDispatch::CreateOverlay(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest(), resp;
  string msg = "The CreateOverlay succeeded.";
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->CreateOverlay(req);
    status = true;
    msg = resp.toStyledString();
  } catch(exception & e)
  {
    msg = "The CreateOverlay operation failed.";
    LOG(LS_ERROR) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(msg, status);
  ctrl_link_->Deliver(control);
}

void ControlDispatch::Echo(TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  string msg = req[TincanControl::Message].asString();
  control.SetResponse(msg, true);
  control.SetControlType(TincanControl::CTTincanResponse);
  ctrl_link_->Deliver(control);
}

LoggingSeverity
ControlDispatch::GetLogLevel(
  const string & log_level)
{
  LoggingSeverity lv = LS_WARNING;
  lock_guard<mutex> lg(disp_mutex_);
  if(log_level == "NONE")
    lv = rtc::LS_NONE;
  else if(log_level == "ERROR")
    lv = rtc::LS_ERROR;
  else if(log_level == "WARNING")
    lv = rtc::LS_WARNING;
  else if(log_level == "INFO")
    lv = rtc::LS_INFO;
  else if(log_level == "VERBOSE" || log_level == "DEBUG")
    lv = rtc::LS_VERBOSE;
  else if(log_level == "SENSITIVE")
    lv = rtc::LS_SENSITIVE;
  else
  {
    string msg = "An invalid log level was specified =  ";
    LOG(LS_WARNING) << msg << log_level << ". Defaulting to WARNING";
  }
  return lv;
}

void
ControlDispatch::InjectFrame(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->InjectFrame(req);
  } catch(exception & e)
  {
    string msg = "The Inject Frame operation failed - ";
    LOG(LS_WARNING) << msg << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
}

void
ControlDispatch::QueryCandidateAddressSet(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest(), cas_info;
  string mac = req[TincanControl::MAC].asString();
  string tap_name = req[TincanControl::TapName].asString();
  string resp;
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->QueryLinkCas(req, cas_info);
    resp = cas_info.toStyledString();
    status = true;
  } catch(exception & e)
  {
    resp = "The QueryCandidateAddressSet operation failed. ";
    LOG(LS_WARNING) << resp << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(resp, status);
  ctrl_link_->Deliver(control);
}

void
ControlDispatch::QueryLinkStats(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest(), node_info;
  string mac = req[TincanControl::MAC].asString();
  string tap_name = req[TincanControl::TapName].asString();
  string resp;
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->QueryLinkStats(req, node_info);
    resp = node_info.toStyledString();
    status = true;
  } catch(exception & e)
  {
    resp = "The QueryLinkStats operation failed. ";
    LOG(LS_WARNING) << resp << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(resp, status);
  ctrl_link_->Deliver(control);
}

void
ControlDispatch::QueryOverlayInfo(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest(), node_info;
  string resp;
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->QueryOverlayInfo(req, node_info);
    resp = node_info.toStyledString();
    status = true;
  } catch(exception & e)
  {
    resp = "The QueryOverlayInfo operation failed. ";
    resp.append(e.what());
    LOG(LS_WARNING) << resp << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(resp, status);
  ctrl_link_->Deliver(control);
}


void
ControlDispatch::RemoveLink(
  TincanControl & control)
{
  bool status = false;
  Json::Value & req = control.GetRequest();
  const string tap_name = req[TincanControl::TapName].asString();
  const string mac = req[TincanControl::MAC].asString();
  string msg("The virtual link to ");
  msg.append(mac).append(" has been removed.");
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    if(!tap_name.empty() && !mac.empty())
    {
      tincan_->RemoveVlink(req);
      status = true;
    }
    else
    {
      ostringstream oss;
      oss << "Invalid parameters in request to remove link to peer node. " <<
        "Received: TAP Name=" << tap_name << " MAC=" << mac;
      msg = oss.str();
      throw TCEXCEPT(msg.c_str());
    }
  } catch(exception & e)
  {
    msg = "The RemoveLink operation failed.";
    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(msg, status);
  ctrl_link_->Deliver(control);
}

void
ControlDispatch::RemoveOverlay(
  TincanControl & control)
{
  bool status = false;
  Json::Value & req = control.GetRequest();
  string msg("The RemoveOverlay operation ");
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->RemoveOverlay(req);
    status = true;
    msg.append("succeeded.");
  } catch(exception & e)
  {
    msg = "failed.";
    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(msg, status);
  ctrl_link_->Deliver(control);
}

void
ControlDispatch::SendIcc(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->SendIcc(req);
  } catch(exception & e)
  {
    string msg = "The ICC operation failed.";
    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
      control.StyledString();
    //send fail here, ack success when send completes
    control.SetResponse(msg, false);
    ctrl_link_->Deliver(control);
  }
}

void
ControlDispatch::SetNetworkIgnoreList(
  TincanControl & control)
{
  Json::Value & req = control.GetRequest();
  string resp;
  bool status = false;
  lock_guard<mutex> lg(disp_mutex_);
  try
  {
    tincan_->SetIgnoredNetworkInterfaces(req);
    status = true;
  } catch(exception & e)
  {
    resp = "The SetNetworkIgnoreList operation failed.";
    LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
      control.StyledString();
  }
  control.SetResponse(resp, status);
  ctrl_link_->Deliver(control);
}

//void
//ControlDispatch::UpdateRoutes(
//  TincanControl & control)
//{
//  bool status = false;
//  Json::Value & req = control.GetRequest();
//  const string tap_name = req[TincanControl::TapName].asString();
//  string msg = "The Add Routes operation failed.";
//  lock_guard<mutex> lg(disp_mutex_);
//  Json::Value rts = req[TincanControl::Routes];
//  if(rts.isArray())
//  {
//    for(uint32_t i = 0; i < rts.size(); i++)
//    {
//      try
//      {
//        string route = rts[i].asString();
//        string dest_mac = route.substr(0, 12);
//        string path_mac = route.substr(13, 24);
//        tincan_->UpdateRoute(tap_name, dest_mac, path_mac);
//      } catch(exception & e)
//      {
//        LOG(LS_WARNING) << e.what() << ". Control Data=\n" <<
//          control.StyledString();
//      }
//    }
//    status = true;
//    msg = "The Add Routes opertation completed successfully.";
//  }
//  else
//  {
//    msg += "The routes parameter is not an array. ";
//  }
//  control.SetResponse(msg, status);
//  ctrl_link_->Deliver(control);
//}
}  // namespace tincan
