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
#include "tunnel.h"
#include "tincan_exception.h"
#include "tincan_control.h"

namespace tincan
{
Tunnel::Tunnel() :
  preferred_(-1),
  is_valid_(false)
{
  id_.fill(0);
}

void
tincan::Tunnel::Transmit(
  TapFrame & frame)
{
  if (is_valid_)
    vlinks_[preferred_]->Transmit(frame);
}

void
Tunnel::AddVlinkEndpoint(
  shared_ptr<VirtualLink> vlink)
{
  cricket::IceRole role = vlink->IceRole();
  vlinks_[role] = vlink;
  vlinks_[role]->SignalLinkUp.connect(this, &Tunnel::SetPreferredLink);

  if(preferred_ < 0)
    preferred_ = role;
}

void
Tunnel::QueryCas(
  Json::Value & cas_info)
{
  if (vlinks_[cricket::ICEROLE_CONTROLLING])
    cas_info[TincanControl::Controlling.c_str()] =
    vlinks_[cricket::ICEROLE_CONTROLLING]->Candidates();
  
  if(vlinks_[cricket::ICEROLE_CONTROLLED])
    cas_info[TincanControl::Controlled.c_str()] =
    vlinks_[cricket::ICEROLE_CONTROLLED]->Candidates();
}

void
Tunnel::Id(
  MacAddressType id)
{
  id_ = id;
}

MacAddressType
Tunnel::Id()
{
  return id_;
}

void 
Tunnel::ReleaseLink(
  int role)
{
  if(!(role < 0 || role > 1))
    vlinks_[role].reset();
  else
    throw TCEXCEPT("ReleaseLink failed, an invalid vlink role was specifed");
  if(vlinks_[role ^ 1] && vlinks_[role ^ 1]->IsReady())
  {
    preferred_ = role ^ 1;
  }
  else is_valid_ = false;
}

/*
Set this link role to be preferred for transmits if there is not an already
writable links.
*/
void
Tunnel::SetPreferredLink(
  int role)
{
  if(vlinks_[role ^ 1] && !vlinks_[role ^ 1]->IsReady())
  {
    preferred_ = role;
    is_valid_ = true;
  }    
  //else
  //  vlinks_[role].reset();
}
} // end namespace tincan
