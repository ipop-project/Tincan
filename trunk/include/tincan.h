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
#ifndef TINCAN_TINCAN_H_
#define TINCAN_TINCAN_H_
#include "tincan_base.h"
#include "webrtc/base/event.h"
#include "control_listener.h"
#include "control_dispatch.h"
#include "overlay.h"
#include "tunnel.h"
#include "virtual_network.h"

namespace tincan {
class Tincan :
  public TincanDispatchInterface,
  public sigslot::has_slots<>
{
public:
  Tincan();
  ~Tincan();
  //
  //TincanDispatchInterface interface

  void CreateVlink(
    const Json::Value & link_desc,
    const TincanControl & control) override;

  void CreateOverlay(
    const Json::Value & olay_desc,
    Json::Value & olay_info) override;
  
  void InjectFrame(
    const Json::Value & frame_desc) override;

  void QueryLinkStats(
    const Json::Value & link_desc,
    Json::Value & node_info) override;

  void QueryOverlayInfo(
    const Json::Value & olay_desc,
    Json::Value & node_info) override;

  void RemoveOverlay(
    const Json::Value & tnl_desc) override;

  void RemoveVlink(
    const Json::Value & link_desc) override;

  void SendIcc(
    const Json::Value & icc_desc) override;

  void SetIpopControllerLink(
    IpopControllerLink * ctrl_handle) override;

  void UpdateRouteTable(
    const Json::Value & rts_desc) override;
//
//
  void OnLocalCasUpdated(
    string link_id,
    string lcas);

  void QueryLinkCas(
    const Json::Value & link_desc,
    Json::Value & cas_info);

  void Run();
private:
  bool IsOverlayExisit(
    const string & oid);

  Overlay & OverlayFromId(
    const string & oid);

  void OnStop();
  void Shutdown();
  //TODO:Code cleanup
#if defined(_IPOP_WIN)
  static BOOL __stdcall ControlHandler(
    DWORD CtrlType);
#endif // _IPOP_WIN

  vector<unique_ptr<Overlay>> ovlays_;
  IpopControllerLink * ctrl_link_;
  map<string, unique_ptr<TincanControl>> inprogess_controls_;
  Thread ctl_thread_;
  shared_ptr<ControlListener> ctrl_listener_; //must be destroyed before ctl_thread
  static Tincan * self_;
  std::mutex ovlays_mutex_;
  std::mutex inprogess_controls_mutex_;
  rtc::Event exit_event_;

};
} //namespace tincan
#endif //TINCAN_TINCAN_H_

