// SPDX-License-Identifier: ISC
// Native AWDL host coexistence backend. BCM submits a persistent firmware
// schedule; MT7922 maps the accepted Family sequence to bounded ROC grants.
// Configuration acknowledgement is separate from verified radio-window TX.
// The primary BSS/STA/key/BA context survives every window. A prepared
// secondary context may be parked only after a correlated grant expires and
// every token is retired; abnormal exits use confirmed full teardown.
// This is a single-radio host policy, not a claim of BCM firmware/TSF support.

#include "../AWDL/AvailabilityEnd.hpp"

static UInt64 mt7922AwUptimeUs(){
 UInt64 ticks=0,ns=0;clock_get_uptime(&ticks);absolutetime_to_nanoseconds(ticks,&ns);return ns/1000;
}

extern "C" kern_return_t thread_terminate(thread_t);

// Read-only pool diagnostics for the AWDL handoff research: publish the mgmt
// pool class and vtable slot 0x118 value without calling either. The b245
// panic showed slot 0x118 was not a no-arg allocator in our context.
extern "C" void *MT7922GetMgmtFramePool(IO80211VirtualInterface*)
 __asm("__ZN23IO80211VirtualInterface16getMgmtFramePoolEv") LIBKERN_RETURNS_NOT_RETAINED;

// b248: the mgmt pool is IO80211MallocBufferPool (b247 hardware). Its
// getBuffer() and the buffer's own methods are the allocation ABI BCM uses.
// b245 crashed because it wrote through IO80211Buffer::writeBytes, which
// reads the slab pointer at buffer+0x10; a malloc buffer keeps its slab at
// +0x18 and must use IO80211MallocBuffer::writeBytes.
extern "C" void *MT7922PoolGetBuffer(void*)
 __asm("__ZN23IO80211MallocBufferPool9getBufferEv") LIBKERN_RETURNS_NOT_RETAINED;
extern "C" UInt64 MT7922MallocBufferCapacity(const void*)
 __asm("__ZNK19IO80211MallocBuffer11getCapacityEv");
extern "C" UInt64 MT7922MallocBufferWrite(void*,UInt64,const void*,UInt64)
 __asm("__ZN19IO80211MallocBuffer10writeBytesEmPKvm");
extern "C" void MT7922MallocBufferSetLength(void*,UInt64)
 __asm("__ZN19IO80211MallocBuffer9setLengthEm");
extern "C" void MT7922PoolReturnBuffer(void*,void*)
 __asm("__ZN23IO80211MallocBufferPool12returnBufferEP13IO80211Buffer");
// Driver-facing RX ingestion of the AWDL peer manager. Contract reversed from
// IO80211Family 1200.13.1 (AWDL/1.2.1-rx-handoff-mi-tx-20261030.txt):
// (buffer, channel, apple80211_act_frm_hdr*, headerLen=0x48). The buffer holds
// the Action body starting at the vendor category 0x7f; on the enqueue path
// ownership transfers to the family queue, on every other path the family
// returns the buffer to the pool itself. Never touch it after the call.
extern "C" UInt32 MT7922ActionFrameInput(OSObject*,void*,UInt32,void*,UInt64)
 __asm("__ZN22IO80211AWDLPeerManager16actionFrameInputEP13IO80211BufferjPvm");
// 1.2.4: driver->family datapath events. Event 0x29 (reversed from the
// reportDataPathEvents jump tables) = APPLE80211_M_AWDL_AVAILABILITY_WINDOW_START:
// it runs monitorTimeline, sets ivars+0x304 (the "in AW" gate used by
// handleUmiTimer and sendUnicastMI) and stores the AW channel info at
// ivars+0xa748/0xa750. 0x2b clears 0x304. Both are required for the family's
// unicast MI timer to run at all.
extern "C" UInt32 MT7922AwdlReportEvents(OSObject*,UInt32,void*,UInt64)
 __asm("__ZN22IO80211AWDLPeerManager20reportDataPathEventsEjPvm");
// Read-only peer-state probe: did the family really create a peer for this
// address? Returns an unretained pointer or null.
extern "C" OSObject *MT7922PeekPeer(OSObject*,UInt8*)
 __asm("__ZN18IO80211PeerManager8findPeerEPh") LIBKERN_RETURNS_NOT_RETAINED;
// 1.2.9 read-only UMI diagnostics (see handleUmiTimer/sendUnicastMI RE).
extern "C" OSObject *MT7922UmiTimer(OSObject*)
 __asm("__ZN22IO80211AWDLPeerManager11getUMITimerEv") LIBKERN_RETURNS_NOT_RETAINED;
extern "C" UInt32 MT7922PeerUmiAttempts(OSObject*)
 __asm("__ZN15IO80211AWDLPeer17getNumUMIAttemptsEv");
extern "C" UInt64 MT7922PeerUmiStamp(OSObject*)
 __asm("__ZN15IO80211AWDLPeer15getUMITimeStampEv");
extern "C" bool MT7922PeerSupportsUmi(OSObject*)
 __asm("__ZN15IO80211AWDLPeer11supportsUMIEv");
extern "C" bool MT7922PeerFastDisc(OSObject*)
 __asm("__ZN15IO80211AWDLPeer22isfastDiscoveryEnabledEv");
// Read-only policy evaluation; never use a setter to force a peer routable.
extern "C" bool MT7922PeerRoutable(OSObject*)
 __asm("__ZN15IO80211AWDLPeer14isPeerRoutableEv");
// Registered AWDL service table size (source of the MI service TLVs).
extern "C" UInt32 MT7922ServiceCount(OSObject*)
 __asm("__ZN22IO80211AWDLPeerManager26getRegisteredServicesCountEv");

void MT7922AngusSWCore::inspectMgmtPool(){
 void *pool=proximity?MT7922GetMgmtFramePool(proximity):nullptr;
 setProperty("AWD6PoolPresent",pool!=nullptr);
 if(!pool)return;
 auto *object=static_cast<OSObject*>(pool);
 const OSMetaClass *meta=object->getMetaClass();
 const char *name=meta?meta->getClassName():nullptr;
 setProperty("AWD6PoolClass",name?name:"?");
 void **vtable=*reinterpret_cast<void***>(pool);
 void *slot=vtable?vtable[0x118/8]:nullptr;
 char text[32];snprintf(text,sizeof text,"%p",slot);
 setProperty("AWD6PoolSlot118",text);
 // One probe allocation: proves getBuffer() works and exposes the capacity
 // before any frame write. The buffer goes straight back to the pool.
 void *probe=MT7922PoolGetBuffer(pool);
 setProperty("AWD6PoolProbe",probe!=nullptr);
 if(probe){
  const UInt64 capacity=MT7922MallocBufferCapacity(probe);
  setProperty("AWD6PoolCapacity",capacity,64);
  MT7922PoolReturnBuffer(pool,probe);
 }
}

// 1.2.1 RX handoff, contract verified from IO80211Family 1200.13.1.
// actionFrameInput(buffer, channel, hdr, 0x48): the 5th argument is the
// act-frame header length, not the frame length (b248 passed the frame length
// and trapped at 0x220e8a5 when it reached 0x49). The buffer carries only the
// Action body (vendor category 0x7f, OUI 00:17:f2, type 0x08, version != 0);
// the header is the 0x48-byte metadata captured with this frame. RSSI 0
// cannot create a family peer (determinePeerReachability returns state 3), so
// that capture is not handed off. Ownership: the family queues the buffer on
// success and returns it to the pool on every other path; this driver must not
// touch it after the call.
IOReturn MT7922AngusSWCore::nativeAwdlHandoffAction(OSObject *owner,void*,void*,void*,void*){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 if(!core->nativeWork||!core->nativeWork->inGate()||!core->nativeWork->onThread())return kIOReturnNotPermitted;
 if(!__atomic_load_n(&core->nativeAwdlEngineRun,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE))return kIOReturnNotReady;
 core->handoffActionFrame();return kIOReturnSuccess;
}
void MT7922AngusSWCore::handoffActionFrame(){
 if(!nativeWork||!nativeWork->inGate()||!nativeWork->onThread()||!proximity)return;
 mt7922anguscore::awdl::ActionRxRecord record{};
 for(unsigned budget=0;budget<8&&nativeActionRx.take(record);++budget){
  if(!__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)||__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))break;
  handoffCapturedAwdlAction(record);
 }
 setProperty("AWD6ActionInboxAccepted",nativeActionRx.acceptedCount(),64);
 setProperty("AWD6ActionInboxDropped",nativeActionRx.droppedCount(),64);
}
void MT7922AngusSWCore::handoffCapturedAwdlAction(const mt7922anguscore::awdl::ActionRxRecord &record){
 if(!nativeWork||!nativeWork->inGate()||!nativeWork->onThread()||!proximity)return;
 ++nativeAwdlHandoffAttempts;
 setProperty("AWD6HandoffAttempts",nativeAwdlHandoffAttempts,32);
 UInt64 ticks=0,ns=0;clock_get_uptime(&ticks);absolutetime_to_nanoseconds(ticks,&ns);
 const UInt64 nowUs=ns/1000;
 if(nowUs<record.capturedUs||nowUs-record.capturedUs>2000000){++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","stale");return;}
 auto *manager=MT7922GetNativePeerManager(proximity);
 if(!manager||!manager->metaCast("IO80211AWDLPeerManager")){
  ++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","manager");return;
 }
 void *pool=MT7922GetMgmtFramePool(proximity);
 if(!pool){setProperty("AWD6HandoffSkip","pool");++nativeAwdlHandoffSkipped;return;}
 const UInt8 *body=record.body;const UInt64 bodyLength=record.length;
 const UInt32 channel=mt7922anguscore::awdl::roc::le32(record.metadata+16);
 const SInt32 rssi=static_cast<SInt32>(mt7922anguscore::awdl::roc::le32(record.metadata+44));
 if(bodyLength<16||bodyLength>mt7922anguscore::awdl::NativeActionBodyCapacity){++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","length");return;}
 // actionFrameHasCorrectOUI: 7f | 00 17 f2 | 08 | version != 0.
 if(body[0]!=0x7f||body[1]!=0x00||body[2]!=0x17||body[3]!=0xf2||body[4]!=0x08||!body[5]){
  ++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","not-awdl");return;
 }
 if(!rssi){++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","rssi");return;}
 void *buffer=MT7922PoolGetBuffer(pool);
 const UInt32 calls=++nativeAwdlHandoffCalls;
 setProperty("AWD6HandoffCalls",calls,32);
 if(!buffer){++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","buffer");return;}
 const UInt64 capacity=MT7922MallocBufferCapacity(buffer);
 setProperty("AWD6HandoffCapacity",capacity,64);
 if(capacity<bodyLength){
  MT7922PoolReturnBuffer(pool,buffer);
  ++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","capacity");return;
 }
 const UInt64 written=MT7922MallocBufferWrite(buffer,0,body,bodyLength);
 if(written!=bodyLength){
  MT7922PoolReturnBuffer(pool,buffer);
  ++nativeAwdlHandoffSkipped;setProperty("AWD6HandoffSkip","write");
  setProperty("AWD6HandoffWrittenBytes",written,64);return;
 }
 MT7922MallocBufferSetLength(buffer,bodyLength);
 UInt8 hdr[0x48];
 mt7922anguscore::awdl::familyHostActionMetadata(record.metadata,record.capturedUs,hdr);
 setProperty("AWD6FamilyRxRawMacClock",mt7922anguscore::awdl::metadataClock(record.metadata),32);
 setProperty("AWD6FamilyRxProtocolClock",mt7922anguscore::awdl::familyMetadataClock(hdr),32);
 setProperty("AWD6FamilyRxClockSource","host-protocol-rx-capture");
 setProperty("AWD6FamilyRxClockEncoding","native-le32-at-0x30");
 // Only the copied time field changes; SA/channel/RSSI/raw RXD stay intact.
 const UInt32 result=MT7922ActionFrameInput(manager,buffer,channel,hdr,0x48);
 // The buffer now belongs to the family queue or back to its pool.
 nativeAwdlHandoffLastResult=result;
 // 1.2.7: direct proof of peer creation in the family peer list (read-only).
 OSObject *peer=MT7922PeekPeer(manager,hdr);
 const bool found=peer!=nullptr;
 setProperty("AWD6PeerPolicyObserved",found);
 // Family applies its peer-admission/RSSI policy asynchronously. Do not
 // elect a distant frame which that policy never admitted into a real peer.
 if(found){
  IOLockLock(bus->dataPathLock);
  // Keep every admitted peer, including followers. The immediate master
  // selected by Family and the destination of a data packet need not be the
  // strongest directly observed root.
  awdlPeerSchedules.offer(record.body,record.length,record.metadata,record.capturedUs,nowUs);
  const bool selected=enginePeerScheduleState.offer(record.body,record.length,record.metadata,record.capturedUs,nowUs);
  if(selected)++awdlScheduleOfferSerial;else ++enginePeerScheduleRejected;
  IOLockUnlock(bus->dataPathLock);
 }else{
  ++enginePeerScheduleRejected;setProperty("AWD6PeerScheduleRejected",enginePeerScheduleRejected,32);
  setProperty("AWD6PeerScheduleLastReject","family-peer-not-found");
  // 1.2.147 (independent implementation): record the Family-elected parent's
  // own schedule even before the Family admits it as a peer. Without it the
  // window can only self-promote, so the peer's AW timeline never locks. Only
  // the elected-parent MAC is added here; other unadmitted frames stay ignored.
  bool parentSeen=false;
  IOLockLock(bus->dataPathLock);
  if(awdlHostControl.syncValid){awdlParentScheduleSeen+=
   !memcmp(record.metadata,awdlHostControl.parent(),6)?1U:0U;
   parentSeen=!memcmp(record.metadata,awdlHostControl.parent(),6)&&
    awdlPeerSchedules.offer(record.body,record.length,record.metadata,record.capturedUs,nowUs);}
  IOLockUnlock(bus->dataPathLock);
  setProperty("AWD6ParentScheduleSeen",parentSeen);
  setProperty("AWD6ParentScheduleHits",awdlParentScheduleSeen,32);
 }
 if(found)++nativeAwdlPeerFoundCount;else ++nativeAwdlPeerMissingCount;
 setProperty("AWD6PeerFound",found);
 setProperty("AWD6PeerFoundCount",nativeAwdlPeerFoundCount,32);
 setProperty("AWD6PeerMissingCount",nativeAwdlPeerMissingCount,32);
 if(found){
  // 1.2.9 read-only UMI state: is the UMI timer scheduled, is the peer in the
  // UMI chain (ivars+0x668/0x670), does it support UMI, and has a UMI ever
  // been attempted/sent? handleUmiTimer only sends when all of this holds.
  const auto *peerIvars=*reinterpret_cast<const UInt8*const*>(
   reinterpret_cast<const UInt8*>(peer)+0x20);
  if(peerIvars){
   UInt32 error=0;memcpy(&error,peerIvars+0xc0,4);
   setProperty("AWD6PeerTimelineErrorRaw",error,32);
   setProperty("AWD6PeerSynced",(peerIvars[0x683]&1)!=0);
   setProperty("AWD6PeerRoutableByPolicy",MT7922PeerRoutable(peer));
   // The method above re-evaluates observation/RSSI policy; it is not a
   // direct read of the already-published peer state. Keep both facts.
   UInt32 presence=0,observations=0;memcpy(&presence,peerIvars+0x78,4);memcpy(&observations,peerIvars+0x7c,4);
   setProperty("AWD6PeerPresenceState",presence,32);
   setProperty("AWD6PeerObservationCount",observations,32);
   const auto *base=*reinterpret_cast<const UInt8*const*>(reinterpret_cast<const UInt8*>(peer)+0x18);
   if(base){
    SInt32 average=0,last=0;memcpy(&average,base+0x4a4,4);memcpy(&last,base+0x49c,4);
    setProperty("AWD6PeerAverageRssiRaw",static_cast<UInt32>(average),32);
    setProperty("AWD6PeerLastRssiRaw",static_cast<UInt32>(last),32);
   }
   char address[18];snprintf(address,sizeof address,"%02x:%02x:%02x:%02x:%02x:%02x",hdr[0],hdr[1],hdr[2],hdr[3],hdr[4],hdr[5]);
   setProperty("AWD6PeerPolicyAddress",address);
  }
  bool linked=false;
  if(peerIvars){
   UInt64 a=0,b=0;
   memcpy(&a,peerIvars+0x668,8);memcpy(&b,peerIvars+0x670,8);
   linked=(a|b)!=0;
  }
  setProperty("AWD6PeerUmiLinked",linked);
  // 1.2.10 gates for addPeerToUmiChain (0x220ee97..0x220f020):
  // manager fast-discovery (ivars+0xfea9, set only by awdlCATsPolicy from
  // setAwdlPeerTrafficRegistration) and the peer's fast-discovery bit
  // (peer ivars+0x6dc, from the peer's TLV 12 flags bit10).
  const auto *mgrIvars=*reinterpret_cast<const UInt8*const*>(reinterpret_cast<const UInt8*>(manager)+0x20);
  if(mgrIvars){
   UInt32 routeRssi=0,bubbleRssi=0;memcpy(&routeRssi,mgrIvars+0x2820,4);memcpy(&bubbleRssi,mgrIvars+0x2824,4);
   setProperty("AWD6PeerRouteRssiThresholdRaw",routeRssi,32);
   setProperty("AWD6PeerBubbleRssiThresholdRaw",bubbleRssi,32);
  }
  setProperty("AWD6FastDisc",mgrIvars&&(mgrIvars[0xfea9]&1));
  setProperty("AWD6PeerFastDisc",MT7922PeerFastDisc(peer));
  if(peerIvars){UInt32 dpWord=0;memcpy(&dpWord,peerIvars+0x6d8,4);setProperty("AWD6PeerDpWord",dpWord,32);}
  setProperty("AWD6Services",MT7922ServiceCount(manager),32);
  // 1.2.18 read-only discovery probes. actionFrameProcessInput pushes the
  // peer's service responses to userland (WCL) with
  // _serviceResponseSendToUserLand only when manager ivars+0xa5c0 (response
  // queue) is non-empty, and only if manager ivars+0x4004 bit0 is set.
  // Both reads are diagnostic; nothing is written to the family object.
  {
   UInt32 responseCount=0;
   if(mgrIvars){
    const auto *responseQueue=*reinterpret_cast<const UInt8*const*>(mgrIvars+0xa5c0);
    if(responseQueue){
     const auto *queueState=*reinterpret_cast<const UInt8*const*>(responseQueue+0x10);
     if(queueState)memcpy(&responseCount,queueState+0x24,4);
    }
   }
   setProperty("AWD6ServiceResp",responseCount,32);
   setProperty("AWD6SvcPushGate",mgrIvars&&(mgrIvars[0x4004]&1));
   setProperty("AWD6BonjourOffload",mgrIvars&&(mgrIvars[0x4005]&1));
  }
  setProperty("AWD6PeerUmiAttempts",MT7922PeerUmiAttempts(peer),32);
  setProperty("AWD6PeerUmiStamp",MT7922PeerUmiStamp(peer),64);
  setProperty("AWD6PeerSupportsUmi",MT7922PeerSupportsUmi(peer));
  setProperty("AWD6UmiTimer",MT7922UmiTimer(manager)!=nullptr);
 }
 ++nativeAwdlHandoffWritten;
 if(bodyLength>awdlHandoffMaxBody)awdlHandoffMaxBody=static_cast<UInt32>(bodyLength);
 if(bodyLength>488)++awdlHandoffLargeBodies; // would not fit the old512-byte 802.11 diagnostic copy
 setProperty("AWD6HandoffMaxBody",awdlHandoffMaxBody,32);
 setProperty("AWD6HandoffLargeBodies",awdlHandoffLargeBodies,32);
 setProperty("AWD6HandoffWritten",nativeAwdlHandoffWritten,32);
 setProperty("AWD6HandoffSkipped",nativeAwdlHandoffSkipped,32);
 setProperty("AWD6HandoffLastResult",result,32);
 char text[96];
 snprintf(text,sizeof text,"handed-off len=%llu ch=%u rssi=%d result=%u",
  static_cast<unsigned long long>(bodyLength),
  static_cast<unsigned>(channel),
  static_cast<int>(rssi),result);
 setProperty("AWD6HandoffLast",text);
 IOLog("MT7922_AWD6 handoff %s\n",text);
}

// 1.2.1 family MI TX (slots 41/50). sendUnicastMI/updatePrimaryPayloadMI build
// the MI TLV payload at ivars+0x2358 (length ivars+0x2808) and pass a 0x38-byte
// request: version@0, peer@4, options@0x16/@0x1a, length@0x28, pointer@0x30.
// The pointer must equal the live manager's inline MI buffer. The ioctl
// acknowledges ownership of a bounded template copy, not an over-air ACK.
// Only the worker may submit it, in a real grant after peer provisioning.
SInt32 MT7922AngusSWCore::nativeAwdlTransmitOob(const UInt8 *raw,bool autoRequest){
 using namespace mt7922anguscore::awdl;
 if(!raw)return EINVAL;
 const UInt32 calls=__atomic_add_fetch(&awdlMiCalls,1U,__ATOMIC_RELAXED);
 setProperty("AWD6MiCalls",calls,32);
 auto reject=[&](SInt32 code,const char *why){
  ++awdlMiRejected;
  setProperty("AWD6MiRejected",awdlMiRejected,32);
  setProperty("AWD6MiLastReason",why);
  setProperty("AWD6MiLastResult",static_cast<UInt32>(code),32);
  awdlMiLastResult=static_cast<UInt32>(code);
  return code;
 };
 if(!proximity||!bus||!bus->dataPathLock)return reject(ENODEV,"not-ready");
 if(!autoRequest&&!__atomic_load_n(&nativeAwdlEngineStarted,__ATOMIC_ACQUIRE))return reject(ENXIO,"engine-stopped");
 const UInt32 version=roc::le32(raw),delayUs=roc::le32(raw+12),bitmap=roc::le32(raw+16);
 const UInt16 length=roc::le16(raw+0x28);
 UInt8 peer[6];memcpy(peer,raw+4,sizeof peer);
 bool broadcast=true;for(auto byte:peer)broadcast&=byte==0xff;
 if(version>1)return reject(EINVAL,"version");
 if(!broadcast&&!dataSource(peer))return reject(EINVAL,"destination");
 if(autoRequest){
  if(!broadcast||(bitmap&&bitmap!=PeriodicMif::EverySlot))return reject(ENOTSUP,"periodic-profile");
  if(!bitmap||!length){
   IOLockLock(bus->dataPathLock);awdlPeriodicMif.disable();IOLockUnlock(bus->dataPathLock);
   setProperty("AWD6MiPeriodicBytes",static_cast<UInt32>(0),32);
   setProperty("AWD6MiLastReason","periodic-disabled");setProperty("AWD6MiLastResult",static_cast<UInt32>(0),32);
   awdlMiLastResult=0;return 0;
  }
  if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return reject(ENXIO,"stopping");
 }
 auto *manager=MT7922GetNativePeerManager(proximity);
 if(!manager||!manager->metaCast("IO80211AWDLPeerManager"))return reject(ENODEV,"no-manager");
 const auto *managerBytes=reinterpret_cast<const UInt8*>(manager);
 const auto *ivars=*reinterpret_cast<const UInt8*const*>(managerBytes+0x20);
 if(!ivars)return reject(ENODEV,"no-ivars");
 UInt64 framePointer=0;memcpy(&framePointer,raw+0x30,8);
 memcpy(awdlMiLastPeer,peer,6);awdlMiLastLength=length;
 char peerText[18];
 snprintf(peerText,sizeof peerText,"%02x:%02x:%02x:%02x:%02x:%02x",peer[0],peer[1],peer[2],peer[3],peer[4],peer[5]);
 setProperty("AWD6MiLastPeer",peerText);
 setProperty("AWD6MiLastLength",static_cast<UInt32>(length),32);
 if(!length||length>MasterIndicationPayloadCapacity)return reject(EINVAL,"length");
 if(framePointer!=reinterpret_cast<UInt64>(ivars+0x2358))return reject(EINVAL,"frame-pointer");
 UInt32 managerLength=0;memcpy(&managerLength,ivars+0x2808,4);
 if((managerLength&0xffffU)!=length)return reject(EINVAL,"length-mismatch");
 UInt8 payload[MasterIndicationPayloadCapacity];memcpy(payload,ivars+0x2358,length);
 // 1.2.14 diagnostic: TLV types the family put into this MI (service response
 // TLVs from its registered service table appear here when present).
 {
  char summary[144];size_t pos=0;summary[0]=0;
  for(UInt32 at=0;at+3<=length;){
   const UInt32 type=payload[at];
   const UInt32 n=static_cast<UInt32>(payload[at+1])|(static_cast<UInt32>(payload[at+2])<<8);
   if(at+3+n>length)break;
   if(pos+8<sizeof summary)pos+=static_cast<size_t>(snprintf(summary+pos,sizeof summary-pos,
    "%s%u:%u",pos?",":"",type,n));
   at+=3+n;
  }
  setProperty("AWD6MiTlvs",summary[0]?summary:"none");
 }
 if(!masterIndicationPayloadValid(payload,length))return reject(EINVAL,"tlv-length");
 if(autoRequest){
  IOLockLock(bus->dataPathLock);
  const bool configured=awdlPeriodicMif.configure(payload,length,delayUs,mt7922AwUptimeUs());
  IOLockUnlock(bus->dataPathLock);
  if(!configured)return reject(EINVAL,"periodic-delay");
  setProperty("AWD6MiPeriodicBytes",static_cast<UInt32>(length),32);
  setProperty("AWD6MiPeriodicDelayUs",delayUs,32);
  setProperty("AWD6MiLastReason","periodic-configured");setProperty("AWD6MiLastResult",static_cast<UInt32>(0),32);
  awdlMiLastResult=0;return 0;
 }
 // Unknown native +0x14/+0x16 fields are deliberately not interpreted as
 // channels. Use a captured channel only when its source is this exact peer.
 UInt8 peerChannel=0;bool queued=false,running=false;UInt32 depth=0;
 IOLockLock(bus->dataPathLock);
 if(!broadcast&&!autoRequest&&awdlRxActionFrameLength>=16&&
    !memcmp(awdlRxActionFrame+10,peer,6)){
  const UInt64 now=radioClock(nullptr)*1000;
  if(now>=awdlRxActionFrameCapturedUs&&now-awdlRxActionFrameCapturedUs<=100000)
   peerChannel=awdlRxActionFrameChannel;
 }
 if(!(peerChannel==6||peerChannel==44||peerChannel==149))peerChannel=0;
 running=__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
  !__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
 if(running){
  awdlMiDropped+=awdlMiQueue.expire(radioClock(nullptr)*1000);
  queued=awdlMiQueue.push(peer,peerChannel,payload,length,radioClock(nullptr)*1000);
  if(queued){++awdlMiStagedCount;if(!broadcast)++awdlMiUnicastQueued;}
  else ++awdlMiQueueFull;
 }
 depth=awdlMiQueue.count();
 IOLockUnlock(bus->dataPathLock);
 if(!queued)return reject(running?ENOBUFS:ENXIO,running?"queue-full":"engine-stopped");
 setProperty("AWD6MiStaged",depth!=0);setProperty("AWD6MiQueueDepth",depth,32);
 setProperty("AWD6MiStagedCount",awdlMiStagedCount,32);
 setProperty("AWD6MiUnicastQueued",awdlMiUnicastQueued,32);
 setProperty("AWD6MiQueueFull",awdlMiQueueFull,32);
 setProperty("AWD6MiLastResult",static_cast<UInt32>(0),32);
 setProperty("AWD6MiLastReason","queued-owned-template");awdlMiLastResult=0;
 IOLog("MT7922_AWD6 MI queued peer=%s len=%u ch=%u depth=%u\n",peerText,
  static_cast<unsigned>(length),static_cast<unsigned>(peerChannel),depth);
 return 0;
}

__attribute__((noinline)) void MT7922AngusSWCore::servicePeriodicMif(
 const mt7922anguscore::awdl::TxContext &context,const mt7922anguscore::awdl::PeerSchedule &plan,UInt8 channel){
 using namespace mt7922anguscore::awdl;
 const UInt64 now=mt7922AwUptimeUs();PeerWindow slot{};
 if(!plan.window(now,slot)||!slotAdmits(now,slot,channel,false))return;
 IOLockLock(bus->dataPathLock);
 bool sent=false;size_t bytes=0;
 if(awdlPeriodicMif.due(now)&&radioSession.state()==roc::State::Granted&&!radioSession.tick(now/1000)){
  const DiscoveryTiming timing{now,now,context.deadlineUs,channel,3,++awdlMiSequence};
  if(buildScheduledMif(context,timing,plan,awdlPeriodicMif.data(),awdlPeriodicMif.size(),
    awdlMifWireScratch,sizeof awdlMifWireScratch,bytes))
   sent=bus->submitAwdlDiscovery(awdlMifWireScratch,static_cast<UInt32>(bytes),context);
  if(sent){awdlPeriodicMif.submitted(slot.endUs);++awdlPeriodicMifSent;++awdlMiSubmitted;awdlMiBytes+=bytes;}
 }
 IOLockUnlock(bus->dataPathLock);
 if(sent){
  setProperty("AWD6MiPeriodicSent",awdlPeriodicMifSent,32);
  setProperty("AWD6MiPeriodicWireBytes",static_cast<UInt32>(bytes),32);
  setProperty("AWD6MiSubmitted",awdlMiSubmitted,32);setProperty("AWD6MiBytes",awdlMiBytes,64);
 }
}

bool MT7922AngusSWCore::nativeAwdlSetSequence(const UInt8 *bytes,size_t length){
 using namespace mt7922anguscore::awdl;
 if(!bytes||length!=sizeof nativeAwdlSeqBytes){
  setProperty("AWD6NativeAwdlSeqAccepted",false);
  setProperty("AWD6NativeAwdlSeqReason","length");
  return false;
 }
 ChannelSequence parsed{};
 const bool decoded=ChannelSequence::decode(bytes,length,parsed);
 if(bus)IOLockLock(bus->dataPathLock);
 if(decoded){
  // A repeated configure sends the same plan; keep the serial so slot 37
  // stays idempotent instead of running another first window.
  const bool same=nativeAwdlSeqValid&&!memcmp(nativeAwdlSeqBytes,bytes,length);
  if(!same){
   memcpy(nativeAwdlSeqBytes,bytes,length);
   __atomic_add_fetch(&nativeAwdlSeqSerial,1U,__ATOMIC_RELEASE);
   if(!coexPlanEpochUs)coexPlanEpochUs=radioClock(nullptr)*1000; // plan changes preserve the AW clock
  }
  nativeAwdlSeq=parsed;
 }
 nativeAwdlSeqValid=decoded;
 if(bus)IOLockUnlock(bus->dataPathLock);
 setProperty("AWD6NativeAwdlSeqSerial",nativeAwdlSeqSerial,32);
 char full[sizeof nativeAwdlSeqBytes*2+1];size_t at=0;full[0]=0;
 for(size_t i=0;i<length&&at+2<sizeof full;++i)
  at+=static_cast<size_t>(snprintf(full+at,sizeof full-at,"%02x",bytes[i]));
 full[at<sizeof full?at:sizeof full-1]=0;
 setProperty("AWD6NativeAwdlSeqFull",full);
 setProperty("AWD6NativeAwdlSeqAccepted",decoded);
 if(!decoded){
  setProperty("AWD6NativeAwdlSeqReason","decode-failed");
  return false;
 }
 char channels[96*32];size_t used=0;channels[0]=0;
 for(unsigned i=0;i<parsed.count&&i<32;++i){
  const auto &e=parsed.entries[i];
  if(!e.channel)continue;
  const int n=snprintf(channels+used,sizeof channels-used,"%s%u/%u/0x%x",used?",":"",
   static_cast<unsigned>(e.channel),static_cast<unsigned>(e.band),static_cast<unsigned>(e.flags));
  if(n<=0||static_cast<size_t>(n)>=sizeof channels-used)break;
  used+=static_cast<size_t>(n);
 }
 setProperty("AWD6NativeAwdlSeqChannels",channels[0]?channels:"none");
 setProperty("AWD6NativeAwdlSeqCount",parsed.count,32);
 setProperty("AWD6NativeAwdlSeqStructured",parsed.structured);
 UInt8 channel=0,band=0;
 const bool usable=nativeAwdlEngineChannel(channel,band);
 setProperty("AWD6NativeAwdlSeqUsableChannel",static_cast<UInt32>(channel),32);
 setProperty("AWD6NativeAwdlSeqReason",usable?"accepted":"no-social-channel");
 return usable;
}

bool MT7922AngusSWCore::nativeAwdlEngineChannel(UInt8 &channel,UInt8 &band)const{
 if(!nativeAwdlSeqValid)return false;
 for(unsigned i=0;i<nativeAwdlSeq.count&&i<32;++i){
  const auto &e=nativeAwdlSeq.entries[i];
  if(e.channel!=6&&e.channel!=44&&e.channel!=149)continue;
  channel=e.channel;
  band=e.band?e.band:(e.channel<=14?1:2);
  return true;
 }
 return false;
}

// 1.2.39: decode the last captured external PSF into the peer's AWDL
// schedule. It carries the slot phase (down counter), AW counter and the
// 16-entry channel sequence; data may only go to a peer in a slot where both
// are on the same channel (AWDL-11/16, OWL awdl::sync/unicast_wait).
void MT7922AngusSWCore::refreshEnginePeerSchedule(){
 using namespace mt7922anguscore::awdl;
 PeerSchedule candidate{};UInt32 serial=0;
 IOLockLock(bus->dataPathLock);
 serial=awdlScheduleOfferSerial;candidate=enginePeerScheduleState.get();
 IOLockUnlock(bus->dataPathLock);
 if(!serial||serial==enginePeerScheduleSerial)return;
 enginePeerScheduleSerial=serial;
 UInt64 ticks=0,ns=0;clock_get_uptime(&ticks);absolutetime_to_nanoseconds(ticks,&ns);
 const UInt64 nowUs=ns/1000;
 if(!candidate.fresh(nowUs))return;
 enginePeerSchedule=candidate;enginePeerScheduleValid=true;
 ++enginePeerScheduleAccepted;
 setProperty("AWD6PeerScheduleAccepted",enginePeerScheduleAccepted,32);
 setProperty("AWD6PeerScheduleMetric",candidate.metric,32);
 setProperty("AWD6PeerScheduleBaseAW",candidate.baseAW,32);
 if(auto *value=OSData::withBytes(candidate.master,6)){setProperty("AWD6PeerScheduleMaster",value);value->release();}
 if(auto *value=OSData::withBytes(candidate.channels,16)){setProperty("AWD6PeerScheduleChannels",value);value->release();}
 PeerWindow window{};
 const UInt8 current=candidate.window(nowUs,window)?window.channel:0;
 setProperty("AWD6PeerScheduleChannel",static_cast<UInt32>(current),32);
 IOLog("MT7922_AWD6 peer-schedule master=%02x:%02x:%02x:%02x:%02x:%02x metric=%u baseAW=%u ch=%u\n",
  candidate.master[0],candidate.master[1],candidate.master[2],candidate.master[3],
  candidate.master[4],candidate.master[5],candidate.metric,candidate.baseAW,
  static_cast<unsigned>(current));
}

UInt8 MT7922AngusSWCore::nativeAwdlEngineNextChannel(UInt8 &band){
 UInt8 channel=0;return coexChooseWindow(channel,band,coexWindowMs)?channel:0;
}

// One bounded window. The caller owns nothing; this takes the control lock,
// provisions the secondary BSS, ROC-acquires the channel, waits for a
// captured grant and submits native-template frames. Natural expiry parks
// the secondary context; early exit confirms teardown. The primary BSS/STA
// is preserved and firmware ROC release returns RF ownership. relaxFilter also
// relaxes the probe RX filter for the window and restores it afterwards, and
// counts host-delivered external AWDL action frames.
bool MT7922AngusSWCore::nativeAwdlRunWindow(UInt8 channel,UInt8 band,bool relaxFilter,
 UInt32 &outGrants,UInt32 &outTx,UInt32 &outMs,bool &outRestored){
 using namespace mt7922anguscore::awdl;
 outGrants=outTx=outMs=0;outRestored=false;coexLastPauseMs=0;
 awdlWindowBlocked=nullptr;awdlWindowTransient=false;
 removeProperty("AWD6WindowBlocked");setProperty("AWD6WindowBlockTransient",false);
 auto blocked=[&](const char *reason,bool transient){
  awdlWindowBlocked=reason;awdlWindowTransient=transient;
  setProperty("AWD6WindowBlocked",reason);setProperty("AWD6WindowBlockTransient",transient);
  return false;
 };
 if(!bus||!proximity||!channel)return blocked("missing-interface-or-channel",false);
 if(radioClock(nullptr)<__atomic_load_n(&awdlInfraPriorityUntilMs,__ATOMIC_ACQUIRE))return blocked("infra-priority",true);
 if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return blocked("stopping",false);
 Mt7922zOWControlGuard control(bus->controlLock);
 if(!control)return blocked("control-lock-busy",true);
 if(radioClock(nullptr)<__atomic_load_n(&awdlInfraPriorityUntilMs,__ATOMIC_ACQUIRE))return blocked("infra-priority",true);
 if(__atomic_load_n(&bus->powerBlocked,__ATOMIC_ACQUIRE)||!bus->firmwareReady||!bus->firmwareRunning||!bus->wfdmaReady)return blocked("hardware-unavailable",false);
 if((radioSession.ownsResources()&&!radioSession.contextParked())||awd6FilterRestorePending||!asyncWindow.idle())return blocked("resources-held",false);
 if(!nativeTimingReady())return blocked("sync-timing-backend-unsupported",false);
 bool infra=false,scanning=false,joining=false,bootstrap=false;UInt16 staChannel=0;UInt8 staBand=0;
 IOLockLock(bus->dataPathLock);
 infra=bus->networkVisibleLink()||bus->connectionPending||bus->connected;
 scanning=bus->scanInProgress||bus->scanCancelPending;
 joining=bus->connectionPending||bus->networkControlCleanup||bus->networkAbortWork||bus->networkRadioPaused||bus->eapolCount||bus->enterpriseKeyCount;
 bootstrap=bus->networkVisibleLink()&&radioClock(nullptr)<bus->networkAwdlHoldUntilMs;
 staChannel=bus->networkChannel;staBand=bus->networkBand;
 IOLockUnlock(bus->dataPathLock);
 // Automatic AWDL creation makes this entry reachable during normal joins.
 // Same channel does not make auxiliary BSS/filter commands safe during
 // EAPOL, and DHCP gets five seconds of infrastructure priority after link-up.
 if(scanning)return blocked("scan-busy",true);
 if(joining)return blocked("join-cleanup-busy",true);
 if(bootstrap)return blocked("dhcp-bootstrap",true);
 // Same primary channel at 20MHz can carry both interfaces without pausing STA.
 const bool sameChannel=infra&&channel==staChannel&&band==(staBand==5?2:staBand);
 ether_addr address{};
 if(proximity->getHardwareAddress(&address)!=kIOReturnSuccess)return false;
 if(!nativeTemplateInfo.bytes||nativeTemplateInfo.bytes>sizeof nativeTemplateBytes)return false;
 UInt8 nextBssid[6]{};
 IOLockLock(bus->dataPathLock);
 const bool nextBssidValid=nativeConfiguration.bssid(nextBssid);
 IOLockUnlock(bus->dataPathLock);
 if(!nextBssidValid)return blocked("bssid-not-configured",false);
 if(radioSession.contextParked()&&
    (memcmp(radioConfiguration.address,address.octet,6)||memcmp(radioConfiguration.bssid,nextBssid,6))){
  if(!coexReleaseContext())return blocked("address-context-held",false);
 }
 IOLockLock(bus->dataPathLock);
 memcpy(radioConfiguration.address,address.octet,6);
 UInt64 receiveIdentity=0;memcpy(&receiveIdentity,address.octet,6);
 __atomic_store_n(&awd6RxLocal,receiveIdentity,__ATOMIC_RELEASE);
 memcpy(radioConfiguration.bssid,nextBssid,6);
 IOLockUnlock(bus->dataPathLock);
 radioConfiguration.role=wire::Role::P2PDevice;radioConfiguration.wireRoleKnown=true;
 // 1.2.4: family datapath AW events (start after a real grant, end before the
 // ROC is released). The manager is the same peer manager used by the RX
 // handoff; the handler only takes family-internal locks and calls the
 // AWDL vif's setAwdlCurrentChannelSequenceIndex setter.
 auto *awdlManager=MT7922GetNativePeerManager(proximity);
 if(awdlManager&&!awdlManager->metaCast("IO80211AWDLPeerManager"))awdlManager=nullptr;
 const auto *awIvars=awdlManager?*reinterpret_cast<const UInt8*const*>(
  reinterpret_cast<const UInt8*>(awdlManager)+0x20):nullptr;
 // Mirror the family's own AW-start guards before calling in: AWDL enabled
 // (0x3ee0 bit1), not in enable/off transition (0x19e bit0), not resetting
 // (0x10246 bit0). The handler checks the same bits.
 const bool awEventsAllowed=awIvars&&(awIvars[0x3ee0]&2)&&!(awIvars[0x19e]&1)&&
  !(awIvars[0x10246]&1);
 // 1.2.7 diagnostic: the family's installed AW channel table (channel/flags).
 {
  char table[16*12+1];size_t pos=0;table[0]=0;
  if(awIvars)for(unsigned i=0;i<16&&pos+12<sizeof table;++i){
   UInt32 entryChannel=0,entryFlags=0;
   memcpy(&entryChannel,awIvars+0xa774+i*12+4,4);
   memcpy(&entryFlags,awIvars+0xa774+i*12+8,4);
   if(!entryChannel)continue;
   pos+=static_cast<size_t>(snprintf(table+pos,sizeof table-pos,"%s%u/%x",
    pos?",":"",entryChannel,entryFlags));
  }
  setProperty("AWD6AwTable",table[0]?table:"none");
 }
 bool awReported=false;
 bool rxFilterReady=!relaxFilter,rxBitmapQueued=false,rxFilterSampled=false;UInt32 rxFilterLastRead=0xffffffffU;
 if(relaxFilter)setProperty("AWD6RxFilterReady",false);
 bool filterOwned=false;
 if(relaxFilter){
  if(!bus->getProperty("MT7922RxFilterLastFif")&&!infra){
   if(!bus->mcuConfigureDataRxFilter())return blocked("rx-filter-baseline",false);
  }
  const auto *last=OSDynamicCast(OSNumber,bus->getProperty("MT7922RxFilterLastFif"));
  const auto *queued=OSDynamicCast(OSBoolean,bus->getProperty("MT7922RxFilterLastQueued"));
  if(!last||!queued||!queued->isTrue()||!(last->unsigned32BitValue()&(1U<<31)))return blocked("rx-filter-baseline",false);
  awd6FilterSavedFif=last->unsigned32BitValue();
  IOLockLock(bus->dataPathLock);const UInt32 baseline=bus->probeRfcr();IOLockUnlock(bus->dataPathLock);
  if(baseline==0xffffffffU)return blocked("rx-filter-readback-unavailable",false);
  awdlRxDropSaved=baseline&AwdlRxDropMask;
  // The first FIF command can rewrite RFCR even if ROC never grants.
  awdlRxDropOwned=true;
  awd6BeaconDropWasSet=(baseline&(1U<<11))!=0;
  awd6FilterRestorePending=true;filterOwned=true;
  // Clear OTHER_UC too: peer PSF/MIF frames come from BSSs that are not the
  // associated AP. mt76 clears the beacon drop through its bitmap command.
  if(!bus->mcuQueueProbeRxFilterFif(awd6FilterSavedFif|(1U<<6),false)||
     !bus->mcuSetProbeBeaconFilter(true)){
   restoreProbeFilter();
   return false;
  }
 }
 TxContext txContext{};PeerSchedule windowPlan{};bool windowPlanReady=false;
 memcpy(txContext.source,address.octet,6);
 memcpy(txContext.bssid,nextBssid,6);
 txContext.actionFramePeriodTU=__atomic_load_n(&nativeActionFramePeriodTU,__ATOMIC_ACQUIRE);
 discoveryCadence.configure(txContext.actionFramePeriodTU);
 UInt8 rateSetting[5]{};if(readNativeSetting(213,rateSetting,sizeof rateSetting)==0)txContext.minimumRateMbps=rateSetting[4];
 const UInt64 startClock=radioClock(nullptr);
 const UInt64 externalStart=__atomic_load_n(&awd6RxExternal,__ATOMIC_RELAXED);
 roc::CapturedGrant event{};for(unsigned n=0;n<8&&bus->awd4GrantInbox.take(event);++n){}
 if(relaxFilter)__atomic_store_n(&awd6RxProbeActive,1U,__ATOMIC_RELEASE);
 const UInt64 rxAllStart=__atomic_load_n(&awd6RxAll,__ATOMIC_RELAXED);
 coexTrace("window-prepare-enter",(UInt32(band)<<8)|channel);
 const auto started=radioSession.start(0,channel,band,coexWindowMs,
  startClock,infra?40U:300U,true,nullptr,[](void *context,const Lease &lease,UInt8 channel,UInt8 band)->bool{
    auto *core=static_cast<MT7922AngusSWCore*>(context);
    const UInt64 begin=radioClock(nullptr);
    if(core->coexPreparedGeneration!=lease.generation||core->coexPreparedChannel!=channel||core->coexPreparedBand!=band){
    UInt8 basic[wire::DataBssBytes];
    const bool built=wire::dataBss(lease,core->radioConfiguration,
        channel,basic,sizeof basic);
    const bool activated=built&&core->radioConfirmed(2,basic,sizeof basic,115);
    UInt8 rlm[wire::BssRlmBytes];
    const bool rlmBuilt=activated&&wire::bssRlm(lease,channel,band,
        rlm,sizeof rlm);
    const bool rlmOk=rlmBuilt&&core->radioConfirmed(2,rlm,sizeof rlm,116);
    core->setProperty("AWD6DataBssBuilt",built);
    core->setProperty("AWD6DataBssConfirmed",activated);
    core->setProperty("AWD6DataBssRlm",rlmOk);
    if(!rlmOk){
     ++core->awdlDataBssFailures;
     core->setProperty("AWD6DataBssFailures",core->awdlDataBssFailures,32);
    } else {
     ++core->awdlDataBssReadyWindows;
     core->setProperty("AWD6DataBssReady",core->awdlDataBssReadyWindows,32);
     core->setProperty("AWD6DataBssPhy",channel==6?0x0eU:0x31U,32);
    }
    if(!rlmOk)return false;
    core->coexPreparedGeneration=lease.generation;core->coexPreparedChannel=channel;core->coexPreparedBand=band;
    }
    // All peer-table changes precede ROC. The prior context is parked only
    // after its grant expired and outstanding TX tokens retired.
    auto send=[&](const UInt8 *bytes,size_t length){return core->radioConfirmed(3,bytes,length,118);};
    using Cache=PeerCacheControl;
    Cache::Entry requests[Cache::Capacity]{};
    UInt8 pinned[Cache::Capacity][6]{};unsigned pinCount=0;
    IOLockLock(core->packetLock);
    const unsigned aged=core->awdlPeerCache.expirePending(radioClock(nullptr)*1000);
    for(unsigned i=0;i<Cache::Capacity;++i)if(const auto *e=core->awdlPeerCache.at(i)){
     requests[i]=*e;if(e->add())memcpy(pinned[pinCount++],e->mac(),6);
    }
    IOLockUnlock(core->packetLock);
    if(aged){core->awdlPeerCacheAged+=aged;core->setProperty("AWD6PeerCacheAged",core->awdlPeerCacheAged,32);}
    for(unsigned i=0;i<Cache::Capacity;++i){
     const auto &e=requests[i];if(!e.used||e.add()||!e.pending)continue;
     const bool removed=core->radioPeer.remove(lease,core->radioConfiguration,e.mac(),send);
     if(removed){
      IOLockLock(core->packetLock);core->awdlPeerCache.confirmed(i,e.serial,lease.generation,0);IOLockUnlock(core->packetLock);
     }else ++core->awdlPeerCacheProgramFailed;
    }
    // Confirm requested cache peers first and pin them against opportunistic
    // discovery eviction. Advertised cache capacity equals this four-slot bank.
    for(unsigned i=0;i<Cache::Capacity;++i){
     const auto &e=requests[i];if(!e.used||!e.add())continue;
     if(!core->radioPeer.find(lease,e.mac())&&core->radioPeer.count(lease)>=4&&core->radioSession.contextParked())
      core->radioPeer.evictUncached(lease,core->radioConfiguration,pinned,pinCount,nullptr,send);
     const bool ready=core->radioPeer.open(lease,core->radioConfiguration,e.mac(),send);
     if(ready&&e.pending){
      IOLockLock(core->packetLock);
      const bool confirmed=core->awdlPeerCache.confirmed(i,e.serial,lease.generation,core->radioPeer.find(lease,e.mac()));
      IOLockUnlock(core->packetLock);if(confirmed)++core->awdlPeerCacheProgrammed;
     }else if(!ready){
      ++core->awdlPeerCacheProgramFailed;
      // Release the slot now so the Family can retry; a stuck pending entry
      // would otherwise return EBUSY until the aging timeout.
      IOLockLock(core->packetLock);
      const bool rolled=core->awdlPeerCache.rollback(i,e.serial);
      IOLockUnlock(core->packetLock);
      if(rolled){++core->awdlPeerCacheRolledBack;core->setProperty("AWD6PeerCacheRolledBack",core->awdlPeerCacheRolledBack,32);}
     }
    }
    UInt8 candidates[4][6]{};unsigned count=0;
    auto offer=[&](const UInt8 *mac){
     if(!mac||!dataSource(mac)||!memcmp(mac,core->radioConfiguration.address,6))return;
     for(const auto &e:requests)if(e.used&&!e.add()&&!memcmp(e.mac(),mac,6))return;
     for(unsigned i=0;i<count;++i)if(!memcmp(candidates[i],mac,6))return;
     if(count<4)memcpy(candidates[count++],mac,6);
    };
    UInt8 headPeer[6]{};bool hasHead=false;
    IOLockLock(core->packetLock);
    if(core->awdlDataTxCount&&!(core->awdlDataTxQueue[core->awdlDataTxHead].dst[0]&1)){
     memcpy(headPeer,core->awdlDataTxQueue[core->awdlDataTxHead].dst,6);hasHead=true;
    }
    IOLockUnlock(core->packetLock);
    if(hasHead)offer(headPeer);
    // OOB callbacks only enqueue. WTBL preparation for a directed MI must
    // happen here, never inside a Family callback or an already-live grant.
    IOLockLock(core->bus->dataPathLock);
    core->awdlMiDropped+=core->awdlMiQueue.expire(radioClock(nullptr)*1000);
    for(unsigned i=0;i<MasterIndicationQueue::Capacity;++i)
     if(const auto *e=core->awdlMiQueue.at(i))if(e->unicast)offer(e->peer);
    const UInt32 captured=core->awdlRxActionFrameLength;
    if(captured>=16&&captured<=sizeof(core->awdlRxActionFrame))offer(core->awdlRxActionFrame+10);
    IOLockUnlock(core->bus->dataPathLock);
    UInt8 protectedPeers[Cache::Capacity+4][6]{};unsigned protectedCount=0;
    for(unsigned i=0;i<pinCount;++i)memcpy(protectedPeers[protectedCount++],pinned[i],6);
    for(unsigned i=0;i<count;++i)memcpy(protectedPeers[protectedCount++],candidates[i],6);
    UInt32 opened=0,failed=0;
    for(unsigned i=0;i<count;++i){
     if(core->radioPeer.find(lease,candidates[i])){++opened;continue;}
     const bool head=hasHead&&!memcmp(headPeer,candidates[i],6);
     if(core->radioPeer.count(lease)>=4&&core->radioSession.contextParked())
      core->radioPeer.evictUncached(lease,core->radioConfiguration,protectedPeers,protectedCount,nullptr,send);
     const bool ready=core->radioPeer.open(lease,core->radioConfiguration,candidates[i],send);
     if(ready)++opened;else ++failed;
     if(head){
      IOLockLock(core->packetLock);
      if(core->awdlDataTxCount&&!memcmp(core->awdlDataTxQueue[core->awdlDataTxHead].dst,headPeer,6)){
       auto &entry=core->awdlDataTxQueue[core->awdlDataTxHead];
       if(ready)entry.tries=0;else if(entry.tries<3)++entry.tries;
      }
      IOLockUnlock(core->packetLock);
     }
    }
    core->setProperty("AWD6PeerCacheProgrammed",core->awdlPeerCacheProgrammed,32);
    core->setProperty("AWD6PeerCacheProgramFailed",core->awdlPeerCacheProgramFailed,32);
    core->awdlDataPeersPreopened+=opened;core->awdlDataPeerPreopenFails+=failed;
    core->setProperty("AWD6DataPeersPreopened",core->awdlDataPeersPreopened,32);
    core->setProperty("AWD6DataPeerPreopenFails",core->awdlDataPeerPreopenFails,32);
    core->setProperty("AWD6PreGrantSetupMs",radioClock(nullptr)-begin,64);
    if(__atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE)||!core->admissionPowerOn||
       __atomic_load_n(&core->bus->powerBlocked,__ATOMIC_ACQUIRE))return false;
    IOLockLock(core->bus->dataPathLock);
    const bool busy=core->bus->connectionPending||core->bus->networkControlCleanup||
     core->bus->scanInProgress||core->bus->scanCancelPending;
    IOLockUnlock(core->bus->dataPathLock);
    return !busy;
  },[](void *context,UInt8 channel,UInt8 band,UInt32 &duration)->bool{
    return static_cast<MT7922AngusSWCore*>(context)->coexQualifyWindow(channel,band,duration);
  });
 coexTrace("roc-submit-result",static_cast<UInt32>(started));
 setProperty("AWD6EngineStartResult",static_cast<UInt32>(started),32);
 if(started==RadioResult::Stale){awdlWindowTransient=true;awdlWindowBlocked="slot-ended-before-roc";setProperty("AWD6WindowBlocked",awdlWindowBlocked);setProperty("AWD6WindowBlockTransient",true);}
 if(started==RadioResult::Success)setProperty("AWD6CoexPhase","await-grant");
 if(sameChannel){++awdlSameChannelWindows;setProperty("AWD6SameChannelWindows",awdlSameChannelWindows,32);}
 UInt32 grants=0,accepted=0,txSubmitted=0,txAttempted=0,restores=0;
 UInt32 buildRejected=0,submitRejected=0,lastTimingStatus=0;
 UInt32 followerBuilt=0,followerDeferred=0;UInt64 firstAttemptDelayUs=0;
 UInt64 grantedAt=0,firstGrant=0;UInt32 grantedMs=0;
 bool stopped=false;bool restoredOk=false;
 auto yieldWindow=[&](const char *reason){
  awdlWindowTransient=true;awdlWindowBlocked=reason;
  setProperty("AWD6WindowBlocked",reason);setProperty("AWD6WindowBlockTransient",true);
 };
 // 1.2.38: the per-window secondary BSS is created by uni_add_dev
 // (conn_state=1, no PHY) so management PSF/MIF can be sent. Data use needs
 // mt76's uni_add_bss transition (conn_state=0 + phymode) and the BSS RLM on
 // the selected channel BEFORE requesting the grant; otherwise the firmware keeps the BSS on
 // its old/no context and peer data never reaches the host (b202 evidence).
 bool dataActivationFailed=started!=RadioResult::Success,dataPlaneReady=started==RadioResult::Success;
 if(started!=RadioResult::Success){
  // A qualified slot can expire during synchronous provisioning. No ROC/TX
  // was submitted on Stale, so keep only its prepared context and return
  // home. Uncertain submissions/failures still require full teardown.
  const bool retained=started==RadioResult::Stale&&radioSession.retainPrepared();
  const bool cleaned=retained?coexReturnHome(true):coexReleaseContext();
  setProperty("AWD6PreparedContextRetained",retained);
  stopped=restoredOk=cleaned;
  setProperty("AWD6EngineCleanup",cleaned);
 } else {
  // Setup latency is not part of the grant-wait budget.
  const UInt64 end=radioClock(nullptr)+(infra?40U:300U)+coexWindowMs+50;
  while(radioClock(nullptr)<end){
   if(!__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)||__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)){yieldWindow("stop-requested");break;}
   if(coexWindowSequenceSerial!=__atomic_load_n(&nativeAwdlSeqSerial,__ATOMIC_ACQUIRE)){yieldWindow("configuration-changed");break;}
   if(radioClock(nullptr)<__atomic_load_n(&awdlInfraPriorityUntilMs,__ATOMIC_ACQUIRE)){yieldWindow("infra-priority");break;}
   IOLockLock(bus->dataPathLock);
   const bool primaryUrgent=bus->networkRadioPaused||bus->eapolCount||bus->enterpriseKeyCount||bus->connectionPending||bus->networkControlCleanup;
   if(primaryUrgent){IOLockUnlock(bus->dataPathLock);yieldWindow("primary-urgent");break;}
   bus->processRxRing(&bus->rxMcuRing);bus->processRxRing(&bus->rxMcuWaRing);
   bus->processRxRing(&bus->rxDataRing);
   const bool primaryChanged=bus->networkAbortWork||bus->networkRadioPaused||bus->eapolCount||bus->enterpriseKeyCount||bus->connectionPending||bus->networkControlCleanup;
   IOLockUnlock(bus->dataPathLock);
   if(primaryChanged){yieldWindow("primary-changed");break;}
   if(relaxFilter)__atomic_fetch_add(&awd6RxPolls,1ULL,__ATOMIC_RELAXED);
   refreshEnginePeerSchedule();
   // A scan or an STA channel change invalidates a same-channel window: the
   // secondary BSS/RLM is pinned to the old channel. Stop transmitting and
   // let the normal cleanup release it.
   if(sameChannel){
    bool still=false;UInt16 nowChannel=0;
    IOLockLock(bus->dataPathLock);
    still=bus->networkVisibleLink()&&!bus->scanInProgress;
    nowChannel=bus->networkChannel;
    IOLockUnlock(bus->dataPathLock);
    if(!still||nowChannel!=channel){yieldWindow("channel-changed");break;}
   }
   for(unsigned n=0;n<8&&bus->awd4GrantInbox.take(event);++n){
    ++grants;awd4LastGrantCapturedAt=event.receivedAt;
    if(radioSession.grant(event.tlv,sizeof event.tlv,event.receivedAt,radioClock(nullptr))==RadioResult::Success){
     ++accepted;coexTrace("grant-accepted",roc::le32(event.tlv+16));
     IOLockLock(bus->dataPathLock);
     const UInt32 rf0=bus->probeRfcrBand(0),rf1=bus->probeRfcrBand(1);
     IOLockUnlock(bus->dataPathLock);
     setProperty("AWD6RxRfcrGrantBand0",rf0,32);setProperty("AWD6RxRfcrGrantBand1",rf1,32);
     IOLog("MT7922_COEX_DIAG rx-filter grant b0=0x%08x b1=0x%08x A3_BSSID_mask=0x200\n",rf0,rf1);
     setProperty("AWD6CoexPhase","awdl");
     txContext.lease=radioSecondary.lease();
     const UInt64 actualEndUs=(event.receivedAt+roc::le32(event.tlv+16))*1000;
     const UInt64 scheduledEndUs=__atomic_load_n(&coexWindowDeadlineUs,__ATOMIC_ACQUIRE);
     txContext.deadlineUs=actualEndUs<scheduledEndUs?actualEndUs:scheduledEndUs;
     ChannelSequence seq{};UInt64 epoch=0;HostControl control{};PeerSchedule parent{};bool parentFound=false;
     IOLockLock(bus->dataPathLock);seq=nativeAwdlSeq;epoch=coexPlanEpochUs;control=awdlHostControl;
     parentFound=awdlPeerSchedules.get(control.parent(),event.receivedAt*1000,parent);
     IOLockUnlock(bus->dataPathLock);
     const auto *peerClock=enginePeerScheduleValid?&enginePeerSchedule:nullptr;
     const auto *clock=control.syncValid?(parentFound?&parent:peerClock):peerClock;
     windowPlanReady=scheduledPlan(seq,epoch,event.receivedAt*1000,txContext.source,nativeTemplateBytes,
       nativeTemplateInfo.bytes,clock,windowPlan,&control);
     if(windowPlanReady){
      // Who this window is synchronized to. source==master => we transmit as
      // master (role 1); otherwise we follow that source's cluster (role 3).
      setProperty("AWD6FollowTarget",windowPlan.master,6);
      setProperty("AWD6FollowSource",windowPlan.source,6);
      setProperty("AWD6FollowRole",memcmp(windowPlan.source,txContext.source,6)?3U:1U,32);
     }
     // 1.2.149 diagnostics: why this window self-promotes. Publish the engine
     // peer schedule the plan could have followed.
     setProperty("AWD6EnginePeerValid",enginePeerScheduleValid);
     if(enginePeerScheduleValid){
      const UInt64 planNowUs=event.receivedAt*1000;
      setProperty("AWD6EnginePeerMaster",enginePeerSchedule.master,6);
      setProperty("AWD6EnginePeerSource",enginePeerSchedule.source,6);
      setProperty("AWD6EnginePeerDistance",static_cast<UInt32>(enginePeerSchedule.distance),32);
      setProperty("AWD6EnginePeerAgeUs",static_cast<UInt64>(planNowUs>=enginePeerSchedule.capturedUs?planNowUs-enginePeerSchedule.capturedUs:0),64);
      setProperty("AWD6EnginePeerMasterIsSelf",!memcmp(enginePeerSchedule.master,txContext.source,6));
      setProperty("AWD6EnginePeerSourceIsSelf",!memcmp(enginePeerSchedule.source,txContext.source,6));
     }
     if(!firstGrant){firstGrant=event.receivedAt;grantedAt=event.receivedAt;grantedMs=roc::le32(event.tlv+16);}
    } else {
     const auto &rejected=radioSession.lastRejection();
     setProperty("AWD6GrantRejectReason",rejected.reason,32);
     setProperty("AWD6GrantRejectAtMs",radioClock(nullptr),64);
     setProperty("AWD6GrantExpectedToken",rejected.expected.token,32);
     setProperty("AWD6GrantExpectedBss",rejected.expected.secondaryBss,32);
     setProperty("AWD6GrantCapturedMs",rejected.captured,64);
     setProperty("AWD6GrantProcessedMs",rejected.processed,64);
     setProperty("AWD6GrantRejectedCount",radioSession.rejectedGrants(),64);
    }
   }
   // ROC/BSS setup can rebuild the receive filter. Apply the distinct
   // RFCR bitmap after the correlated grant, and qualify by actual readback.
   if(accepted&&relaxFilter&&!rxBitmapQueued){
    rxBitmapQueued=bus->mcuSetAwdlRxDrop(AwdlRxDropMask,false);
    setProperty("AWD6RxBitmapQueued",rxBitmapQueued);
    setProperty("AWD6RfcrDirectClear",bus->clearAwdlRfcr(AwdlRxDropMask));
    if(!rxBitmapQueued){yieldWindow("rx-filter-bitmap-queue-failed");break;}
   }
   if(accepted&&relaxFilter){
    IOLockLock(bus->dataPathLock);const UInt32 rfcr=bus->probeRfcr();IOLockUnlock(bus->dataPathLock);
    rxFilterReady=awdlRxFilterReady(rfcr);
    if(!rxFilterSampled||rfcr!=rxFilterLastRead){
     rxFilterSampled=true;rxFilterLastRead=rfcr;setProperty("AWD6RxFilterReadback",rfcr,32);
     setProperty("AWD6RxFilterReady",rxFilterReady);
    }
   }
   if(accepted&&rxFilterReady&&windowPlanReady&&coexMayPublish()&&validTxContext(txContext)&&
      radioSession.state()==roc::State::Granted&&!radioSession.tick(radioClock(nullptr))){
     awdlCurrentWindowChannel=channel;
     // Publish this real grant for slots 41/50 before submitting anything, so
     // a family MI can ride the same window. Cleared before the drain below.
     if(mt7922anguscore::awdl::validTxContext(txContext)){
      IOLockLock(bus->dataPathLock);
      awdlMiWindowActive=true;awdlMiWindowChannel=channel;awdlMiWindowBand=band;
      awdlMiWindowDeadlineUs=txContext.deadlineUs;awdlMiContext=txContext;
      IOLockUnlock(bus->dataPathLock);
     }
     // Drain at most four owned requests per real grant. Keep destination,
     // TLVs and serial together; consume only the exact submitted request.
     // A missing peer/channel defers its MI without blocking other peers.
     for(unsigned attempt=0;attempt<4;++attempt){
      MasterIndicationQueue::Entry mi{};auto miContext=txContext;
      IOLockLock(bus->dataPathLock);
      awdlMiDropped+=awdlMiQueue.expire(radioClock(nullptr)*1000);
      for(unsigned i=0;i<MasterIndicationQueue::Capacity;++i)if(const auto *e=awdlMiQueue.at(i)){
       if(e->channel&&e->channel!=channel)continue;
       const auto wcid=e->unicast?radioPeer.find(txContext.lease,e->peer):0;
       if(e->unicast){
        PeerSchedule destination{};PeerWindow peerSlot{};
        const auto nowUs=radioClock(nullptr)*1000;
        if(!wcid||!awdlPeerSchedules.get(e->peer,nowUs,destination)||
           !destination.window(nowUs,peerSlot)||!slotAdmits(nowUs,peerSlot,channel,true))continue;
       }
       if(!mi.used||e->serial<mi.serial){mi=*e;miContext=txContext;
        miContext.unicast=e->unicast;miContext.peerWcid=wcid;memcpy(miContext.peerAddress,e->peer,6);}
      }
      IOLockUnlock(bus->dataPathLock);
      if(!mi.used)break;
      size_t miBytes=0;
      const UInt64 miNow=mt7922AwUptimeUs();
      const DiscoveryTiming miTiming{miNow,miNow,miContext.deadlineUs,channel,3,++awdlMiSequence};
      const bool built=buildScheduledMif(miContext,miTiming,windowPlan,mi.payload,mi.length,
        awdlMifWireScratch,sizeof awdlMifWireScratch,miBytes);
      bool sent=false;
      IOLockLock(bus->dataPathLock);
      if(built&&radioSession.state()==roc::State::Granted&&!radioSession.tick(radioClock(nullptr)))
       sent=bus->submitAwdlDiscovery(awdlMifWireScratch,static_cast<UInt32>(miBytes),miContext);
      if(sent&&awdlMiQueue.take(mi.serial)){
       ++awdlMiStagedSent;++awdlMiSubmitted;awdlMiBytes+=miBytes;if(mi.unicast)++awdlMiUnicastSent;
      }
      const UInt32 depth=awdlMiQueue.count();
      IOLockUnlock(bus->dataPathLock);
      setProperty("AWD6MiStaged",depth!=0);setProperty("AWD6MiQueueDepth",depth,32);
      setProperty("AWD6MiStagedSent",awdlMiStagedSent,32);setProperty("AWD6MiUnicastSent",awdlMiUnicastSent,32);
      setProperty("AWD6MiSubmitted",awdlMiSubmitted,32);setProperty("AWD6MiBytes",awdlMiBytes,64);
      setProperty("AWD6MiDropped",awdlMiDropped,32);
      if(!sent)break; // expired grant or full descriptors: retain the request
     }
     servicePeriodicMif(txContext,windowPlan,channel);
     // 1.2.4: report the real AW start once per window. The family sets
     // ivars+0x304 (its "in AW" gate for handleUmiTimer/sendUnicastMI) and the
     // AW channel info; it must end with 0x2b before the ROC is released.
     if(awdlManager&&awEventsAllowed&&!awReported){
      // Exact 0x3f00 formula from reportDataPathEvents 0x21dfbbc:
      //   slot = min((awNumber % (A*(B+1))) / (B+1), 15)
      // with A=ivars[0xa744] (slot count) and B=ivars[0xa773]. sanityCheckAWStart
      // compares the entry at ivars+0xa774+slot*12 (version@+0, channel@+4,
      // flags@+8) with the reported channel/flags. Use the continuous wire
      // counter and validate that actual table entry, without inversion.
      const UInt64 nowUs=radioClock(nullptr)*1000;
      PeerWindow slot{};
      const bool ready=windowPlanReady&&windowPlan.window(nowUs,slot);
      const UInt32 slotCount=awIvars?awIvars[0xa744]:0;
      const UInt32 perSlot=awIvars?(static_cast<UInt32>(awIvars[0xa773])+1U):0U;
      const UInt32 slotIndex=ready&&slotCount&&perSlot?((slot.aw%(slotCount*perSlot))/perSlot):16;
      UInt32 entryChannel=0,channelFlags=0;
      if(slotIndex<slotCount&&slotIndex<16){
       memcpy(&entryChannel,awIvars+0xa774+slotIndex*12+4,4);
       memcpy(&channelFlags,awIvars+0xa774+slotIndex*12+8,4);
      }
      UInt8 aw[36]{};
      // The actual AW selects its table entry. Do not invent a counter to
      // make the first matching-channel entry pass native sanity checks.
      if(ready&&entryChannel==channel&&scheduledStart(windowPlan,txContext.source,nowUs,txContext.deadlineUs,
          channel,channelFlags,aw)){
       awdlAwNumber=roc::le32(aw);
       IOLockLock(bus->dataPathLock);memcpy(awdlAwPendingPayload,aw,sizeof aw);
       __atomic_store_n(&awdlAwStartPending,1U,__ATOMIC_RELEASE);IOLockUnlock(bus->dataPathLock);
       awReported=true;++awdlAwPosts;
       setProperty("AWD6AwPosts",awdlAwPosts,32);setProperty("AWD6AwChannel",static_cast<UInt32>(channel),32);
       setProperty("AWD6AwSlot",slotIndex,32);setProperty("AWD6AwFlags",channelFlags,32);
       setProperty("AWD6AwNumber",awdlAwNumber,32);setProperty("AWD6AwRole",roc::le32(aw+4),32);
       setProperty("AWD6AwStartClockSource","host-aw-boundary-plus-native3500");
       char eventHex[73];for(unsigned i=0;i<36;++i)snprintf(eventHex+i*2,3,"%02x",aw[i]);
       setProperty("AWD6AwPayloadHex",eventHex);
      }else{++awdlAwSkips;setProperty("AWD6AwSkips",awdlAwSkips,32);}
     }
   }
   const UInt32 attemptCap=8U;
   if(accepted&&rxFilterReady&&windowPlanReady&&grantedAt&&grantedMs&&txAttempted<attemptCap&&coexMayPublish()){
    const UInt64 grantStartUs=grantedAt*1000;
    const UInt64 hardwareEndUs=(grantedAt+grantedMs)*1000;
    const UInt64 planEndUs=__atomic_load_n(&coexWindowDeadlineUs,__ATOMIC_ACQUIRE);
    const UInt64 grantEndUs=hardwareEndUs<planEndUs?hardwareEndUs:planEndUs;
    UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
    const UInt64 nowUs=ns/1000;UInt8 subtype=0;
    if(discoveryCadence.candidate(nowUs,grantEndUs,subtype)){
     if(!txAttempted)firstAttemptDelayUs=nowUs>=grantStartUs?nowUs-grantStartUs:0;
     DiscoveryTiming timing=discoveryTimingInGrant(grantStartUs,nowUs,grantEndUs,channel,
      subtype,++awdlMiSequence);
     txContext.lease=radioSecondary.lease();txContext.deadlineUs=grantEndUs;
     UInt8 frame[DiscoveryCapacity];size_t bytes=0;bool submitted=false;
     const bool localMaster=windowPlanReady&&!memcmp(windowPlan.source,txContext.source,6);
     const bool built=windowPlanReady&&buildPeerDiscovery(txContext,timing,windowPlan,nativeTemplateBytes,
       nativeTemplateInfo.bytes,frame,sizeof frame,bytes,localMaster);
     if(built){
      PeerWindow slot{};windowPlan.window(nowUs,slot);
      setProperty("AWD6WireAwNumber",static_cast<UInt32>(slot.aw),32);
      setProperty("AWD6WireScheduleMode",localMaster?"native-plan-local":"native-plan-follower");
      setProperty("AWD6WireScheduleSparse",true);
      if(!localMaster)++followerBuilt;
     }else if(!localMaster)++followerDeferred;
     if(built){
      IOLockLock(bus->dataPathLock);
      if(radioSession.state()==roc::State::Granted&&!radioSession.tick(radioClock(nullptr)))
       submitted=bus->submitAwdlDiscovery(frame,static_cast<UInt32>(bytes),txContext);
      IOLockUnlock(bus->dataPathLock);
      if(!submitted)++submitRejected;
     }else{++buildRejected;lastTimingStatus=static_cast<UInt32>(discoveryTimingStatus(timing));}
     ++txAttempted;
     if(submitted){
      if(!txSubmitted){
       char hex[DiscoveryCapacity*2+1];for(size_t i=0;i<bytes;++i)snprintf(hex+i*2,3,"%02x",frame[i]);
       setProperty("AWD6WireLastDiscoveryHex",hex); // host-built bytes, not an air-capture ACK
       setProperty("AWD6WirePlanSeqSerial",coexWindowSequenceSerial,32);
       setProperty("AWD6WirePlanEpochUs",coexPlanEpochUs,64);
      }
      ++txSubmitted;discoveryCadence.submitted(subtype,nowUs,grantEndUs,false);
      setProperty("AWD6NextPsfUs",discoveryCadence.nextPsfUs(),64);
     }
     // 1.2.20 Darwin26 data channel: one queued interface frame rides this
     // real grant with a firmware peer and a data TxContext. Frames are only
     // released after the BSS data activation and its RLM are confirmed, and
     // only inside a slot the peer spends on this channel (1.2.39 gate).
     awdlCurrentWindowChannel=channel;

    }
   }
   // Data is not clocked by the PSF/MIF interval. Progress one queued packet
   // per bounded poll while the independently verified lease is still live.
   if(accepted&&rxFilterReady&&dataPlaneReady&&!dataActivationFailed&&coexMayPublish()&&
      radioSession.state()==roc::State::Granted&&!radioSession.tick(radioClock(nullptr)))
    if(windowPlanReady)serviceAwdlDataTx(txContext,&windowPlan);
   if(radioSession.tick(radioClock(nullptr)))break;
   IOSleep(1);
  }
  discoveryCadence.closeWindow();
  // No new family MI may enter this window; then drain every descriptor that
  // was already published (the pending check below includes MI frames), while
  // the lease still exists. stop() then aborts the ROC and deletes the
  // secondary BSS (confirmed deletion is the release).
  {
   IOLockLock(bus->dataPathLock);
   awdlMiWindowActive=false;
   IOLockUnlock(bus->dataPathLock);
  }
  bool txRetired=false;
  {
   const UInt64 deadline=radioClock(nullptr)+100;
   for(;;){
    bool pending=false;
    IOLockLock(bus->dataPathLock);
    bus->reclaimTxDMA();
    if(bus->txDataRing.entries)for(UInt16 n=0;n<bus->txDataRing.count;++n){
     const auto &e=bus->txDataRing.entries[n];
     if(e.inUse&&e.awdlTxContext.lease.generation==txContext.lease.generation){pending=true;break;}
    }
    IOLockUnlock(bus->dataPathLock);
    if(!pending){txRetired=true;break;}
    if(radioClock(nullptr)>=deadline)break;
    IOSleep(1);
   }
  }
  if(awdlManager&&awReported)__atomic_store_n(&awdlAwEndPending,1U,__ATOMIC_RELEASE);
  coexTrace("tx-retired",txRetired?1:0);
  coexTrace("roc-retire-enter");
  const bool expired=txRetired&&radioSession.retireExpired(radioClock(nullptr));
  coexTrace("roc-retire-result",expired?1:0);
  stopped=expired||coexReleaseContext();
  restoredOk=stopped&&coexReturnHome(stopped);
  setProperty("AWD6CoexContextParked",radioSession.contextParked());
  setProperty("AWD6EngineRestoreMethod","firmware-roc-release");

 }
 if(relaxFilter){
  __atomic_store_n(&awd6RxProbeActive,0U,__ATOMIC_RELEASE);
  if(filterOwned){
   coexTrace("filter-restore-enter");
   restoreProbeFilter();
   coexTrace("filter-restore-return");
   setProperty("AWD6EngineFilterPending",awd6FilterRestorePending);
  }
 }
 // 1.2.157: denser windows (24ms floor instead of 64ms) to close the TCP
 // handshake faster; keeps working AirDrop and was confirmed usable.
 coexNextWindowAtMs=radioClock(nullptr)+(infra?(coexLastPauseMs*2>24?coexLastPauseMs*2:24):2);
 const UInt64 externalEnd=__atomic_load_n(&awd6RxExternal,__ATOMIC_RELAXED);
 const UInt64 external=externalEnd>externalStart?externalEnd-externalStart:0;
 const UInt64 rxAllEnd=__atomic_load_n(&awd6RxAll,__ATOMIC_RELAXED);
 const UInt64 rxAll=rxAllEnd>rxAllStart?rxAllEnd-rxAllStart:0;
 setProperty("AWD6EngineRxAll",rxAll,64);
 setProperty("AWD6RxAllTotal",rxAllEnd,64);
 outGrants=grants;outTx=txSubmitted;
 outMs=static_cast<UInt32>(radioClock(nullptr)-startClock);outRestored=restoredOk;
 setProperty("AWD6EngineChannel",static_cast<UInt32>(channel),32);
 setProperty("AWD6EngineBand",static_cast<UInt32>(band),32);
 setProperty("AWD6EngineWindowGrants",grants,32);
 setProperty("AWD6EngineWindowAccepted",accepted,32);
 setProperty("AWD6EngineWindowTx",txSubmitted,32);
 setProperty("AWD6EngineWindowBuildAttempts",txAttempted,32);
 setProperty("AWD6GrantDurationMs",grantedMs,32);
 setProperty("AWD6FirstTxAttemptDelayUs",firstAttemptDelayUs,64);
 setProperty("AWD6EngineWindowBuildRejected",buildRejected,32);
 setProperty("AWD6EngineWindowSubmitRejected",submitRejected,32);
 setProperty("AWD6EngineWindowTimingStatus",lastTimingStatus,32);
 setProperty("AWD6FollowerWindowBuilt",followerBuilt,32);
 setProperty("AWD6FollowerWindowDeferred",followerDeferred,32);
 setProperty("AWD6EngineWindowMs",outMs,32);
 setProperty("AWD6EngineWindowStop",stopped);
 setProperty("AWD6EngineWindowRestore",restoredOk);
 setProperty("AWD6EngineLastExternal",external,64);
 setProperty("AWD6EnginePeerChannel",static_cast<UInt32>(awdlRxActionFrameChannel),32);
 setProperty("AWD6AwdlRxFrameSerial",__atomic_load_n(&awdlRxActionFrameSerial,__ATOMIC_ACQUIRE),32);
 {
  // Publish the bounded full 802.11 bytes of the last external Action frame.
  const UInt32 frameLength=__atomic_load_n(&awdlRxActionFrameLength,__ATOMIC_ACQUIRE);
  if(frameLength){
   static const char digits[]="0123456789abcdef";
   char hex[2*sizeof(awdlRxActionFrame)+1];
   const UInt32 limit=frameLength<sizeof(awdlRxActionFrame)
    ?frameLength:static_cast<UInt32>(sizeof(awdlRxActionFrame));
   for(UInt32 i=0;i<limit;++i){
    const UInt8 b=awdlRxActionFrame[i];
    hex[2*i]=digits[b>>4];hex[2*i+1]=digits[b&0xf];
   }
   hex[2*limit]=0;
   setProperty("AWD6AwdlRxFrameHex",hex);
   setProperty("AWD6AwdlRxFrameLen",limit,32);
   setProperty("AWD6AwdlRxFrameCh",awdlRxActionFrameChannel,32);
   // AWDL header is 16 bytes after the 802.11 header (category 0x7f + OUI
   // 00:17:f2 + OUI type 0x08 + header length byte); TLVs are
   // type(u8)+len(u16 LE)+value. Publish a bounded read-only summary and the
   // sync-parameters channel for scheduling.
   const UInt32 dot11=((awdlRxActionFrame[0]|(static_cast<UInt32>(awdlRxActionFrame[1])<<8))&0x8000U)?28U:24U;
   if(limit>=dot11+16){
    const UInt8 *body=awdlRxActionFrame+dot11;
    const UInt32 bodyLen=limit-dot11;
    const UInt32 tlvStart=(body[5]>=16&&body[5]<bodyLen)?body[5]:16U;
    char summary[192];size_t pos=0;summary[0]=0;
    UInt32 off=tlvStart,count=0,syncCh=0,down=0;
    UInt8 master[6]={0};
    bool services=false;
    while(off+3<=bodyLen&&count<16){
     const UInt8 type=body[off];
     const UInt32 len=body[off+1]|(static_cast<UInt32>(body[off+2])<<8);
     if(off+3+len>bodyLen)break;
     const UInt8 *value=body+off+3;
     // 1.2.18: the peer advertises services with TLV 2 (service response) or
     // TLV 6 (service params, length 9+count). An empty 6:9 means no classes.
     if(type==0x02||(type==0x06&&len>9))services=true;
     if(pos+16<sizeof summary){
      const int written=snprintf(summary+pos,sizeof summary-pos,"%s%u:%u",
       pos?",":"",static_cast<unsigned>(type),static_cast<unsigned>(len));
      if(written>0)pos+=static_cast<size_t>(written);
     }
     if(type==0x04&&len>=3){
      syncCh=value[0];
      down=value[1]|(static_cast<UInt32>(value[2])<<8);
     }
     if(type==0x18&&len>=6)memcpy(master,value,6);
     off+=3+len;++count;
    }
    if(services){++awdlRxServiceFrames;setProperty("AWD6RxSvcFrames",awdlRxServiceFrames,32);}
    setProperty("AWD6AwdlRxtlvs",summary);
    if(syncCh==6||syncCh==44||syncCh==149){
     awdlPeerNextChannel=static_cast<UInt8>(syncCh);
     setProperty("AWD6AwdlPeerNextCh",syncCh,32);
    }
    awdlPeerDownCounter=static_cast<UInt16>(down);
    setProperty("AWD6AwdlPeerDownCounter",down,32);
    bool any=false;for(unsigned i=0;i<6;++i)any|=master[i]!=0;
    if(any){
     char mac[18];
     snprintf(mac,sizeof mac,"%02x:%02x:%02x:%02x:%02x:%02x",
      master[0],master[1],master[2],master[3],master[4],master[5]);
     memcpy(awdlPeerMaster,master,6);
     setProperty("AWD6AwdlPeerMaster",mac);
    }
   }
  }
 }
 setProperty("AWD6EngineResourcesHeld",(radioSession.ownsResources()&&!radioSession.contextParked())||awd6FilterRestorePending);
 if(accepted&&txAttempted&&txSubmitted<2&&restoredOk&&!awd6FilterRestorePending&&
    radioClock(nullptr)*1000>=__atomic_load_n(&coexWindowDeadlineUs,__ATOMIC_ACQUIRE)){
  awdlWindowTransient=true;awdlWindowBlocked="slot-ended-before-pair";
  setProperty("AWD6WindowBlocked",awdlWindowBlocked);setProperty("AWD6WindowBlockTransient",true);
 }
 const bool ok=started==RadioResult::Success&&accepted&&stopped&&restoredOk&&
  (!radioSession.ownsResources()||radioSession.contextParked())&&!awd6FilterRestorePending;
 if(!ok){
  setProperty("AWD6LastWindowFailure",awdlWindowTransient?(awdlWindowBlocked?awdlWindowBlocked:"yield"):
   (started!=RadioResult::Success?"roc-start-failed":(!accepted?"no-matching-grant":"cleanup-failed")));
  setProperty("AWD6LastWindowFailureAtMs",radioClock(nullptr),64);
  setProperty("AWD6LastWindowFailureGrantsSeen",grants,32);
 }
 if(ok)IOLog("MT7922_AWD6 engine window ch=%u band=%u grants=%u tx=%u ext=%llu ms=%u\n",
  static_cast<unsigned>(channel),static_cast<unsigned>(band),grants,txSubmitted,
  static_cast<unsigned long long>(external),outMs);
 return ok;
}

bool MT7922AngusSWCore::nativeTimingReady(){
 using namespace mt7922anguscore::awdl;
 UInt32 sync[6]{};
 const int result=readNativeSetting(116,sync,sizeof sync);
 const auto source=selectSyncTiming(result,sync);
 setProperty("AWD6TimingSource",source==SyncTimingSource::FamilyDefaults?"family-init-defaults":
  (source==SyncTimingSource::ExplicitParameters?"explicit-sync-params":"unsupported-explicit-params"));
 setProperty("AWD6TimingExplicit",result==0);
 if(source==SyncTimingSource::Unsupported)return false;
 const UInt16 period=source==SyncTimingSource::FamilyDefaults?110:static_cast<UInt16>(sync[4]);
 __atomic_store_n(&nativeActionFramePeriodTU,period,__ATOMIC_RELEASE);
 setProperty("AWD6ActionFramePeriodTU",period,32);
 return true;
}

bool MT7922AngusSWCore::nativeAwdlStartEngine(){
 // BCM's SYNC_STATE submits awdl_opmode/configuration. It does not block the
 // Family configure gate until a physical availability window has elapsed.
 const bool alive=__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
  !__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
 auto refuse=[&](const char *why){setProperty("AWD6EngineRefused",why);return false;};
 if(!nativeWork||!bus||!proximity||!admissionPowerOn||!hardwareReady())return refuse("not-ready");
 if(!awdlFamilyTimer||!__atomic_load_n(&nativeStartComplete,__ATOMIC_ACQUIRE))return refuse("family-timer-not-ready");
 // ABI staging is broader than the currently implemented wire timing profile.
 // Never accept alternate timing and then silently transmit 16-TU frames.
 if(!nativeTimingReady())return refuse("sync-timing-backend-unsupported");
 UInt8 configuredBssid[6]{};IOLockLock(bus->dataPathLock);
 const bool haveBssid=nativeConfiguration.bssid(configuredBssid);
 IOLockUnlock(bus->dataPathLock);
 if(!haveBssid)return refuse("bssid-not-configured");
 if(alive){removeProperty("AWD6EngineRefused");return true;}
 if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return refuse("stopping");
 if(!__atomic_load_n(&proximityReady,__ATOMIC_ACQUIRE)||!nativeAwdlSeqValid||!nativeTemplateInfo.bytes)return refuse("configuration-incomplete");
 if(!__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE))return refuse("thread-busy");
 if(radioSession.ownsResources()||awd6FilterRestorePending)return refuse("resources-held");
 // The scheduler runs before the first window provisions radioConfiguration.
 // Seed its local identity now; otherwise a valid Family template cannot
 // bootstrap against an all-zero source address.
 ether_addr local{};
 if(proximity->getHardwareAddress(&local)!=kIOReturnSuccess||!mt7922anguscore::awdl::dataSource(local.octet))return refuse("local-address");
 IOLockLock(bus->dataPathLock);memcpy(radioConfiguration.address,local.octet,6);
 UInt64 receiveIdentity=0;memcpy(&receiveIdentity,local.octet,6);
 __atomic_store_n(&awd6RxLocal,receiveIdentity,__ATOMIC_RELEASE);IOLockUnlock(bus->dataPathLock);
 // No hardware work occurs here. The worker arbitrates with scan/join and
 // validates the sequence again after firmware provisioning, before ROC.
 inspectMgmtPool();
 ++nativeAwdlStartCount;nativeAwdlEngineSeqSerial=nativeAwdlSeqSerial;
 setProperty("AWD6EngineStartCount",nativeAwdlStartCount,32);
 coexNextWindowAtMs=0;coexFailureStreak=0;
 setProperty("AWD6FamilyDispatches",awdlFamilyDispatches,32);
 setProperty("AWD6FamilyWrongThread",awdlFamilyWrongThread,32);
 setProperty("AWD6FamilyDispatchStage","idle");
 setProperty("AWD6TxChecksumCompleted",awdlTxChecksumCompleted,32);
 setProperty("AWD6TxChecksumChanged",awdlTxChecksumChanged,32);
 setProperty("AWD6TxChecksumMalformed",awdlTxChecksumMalformed,32);
 setProperty("AWD6TxChecksumUnsupported",awdlTxChecksumUnsupported,32);
 __atomic_store_n(&awdlFamilyRefreshPending,false,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlFamilyHandoffPending,false,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlFamilyReportPending,false,__ATOMIC_RELEASE);
 discoveryCadence=mt7922anguscore::awdl::DiscoveryCadence{};
 __atomic_store_n(&awdlAwStartPending,0U,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlAwEndPending,0U,__ATOMIC_RELEASE);
 nativeAwdlEngineStarted=true;
 __atomic_store_n(&nativeAwdlEngineArmed,false,__ATOMIC_RELEASE);
 __atomic_store_n(&coexSchedulerActive,true,__ATOMIC_RELEASE);
 if(!nativeAwdlEngineThreadStart()){__atomic_store_n(&coexSchedulerActive,false,__ATOMIC_RELEASE);nativeAwdlEngineStarted=false;return refuse("thread-start");}
 awdlFamilyTimer->setTimeoutMS(1); // first arm only after engine startup succeeded
 removeProperty("AWD6EngineRefused");
 setProperty("AWD6EngineStopped",false);
 setProperty("AWD6EngineStopRequested",false);
 removeProperty("AWD6CoexFault");
 setProperty("AWD6EngineConfigured",true);
 setProperty("AWD6EngineWindowVerified",false);
 setProperty("AWD6EngineStarted",true); // worker, not a physical grant
 setProperty("AWD6EngineMode","coexistence-scheduled");
 setProperty("AWD6CoexPhase","home");
 return true;
}

bool MT7922AngusSWCore::nativeAwdlEngineThreadStart(){
 if(!__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE))return false;
 __atomic_store_n(&nativeAwdlEngineRun,true,__ATOMIC_RELEASE);
 __atomic_store_n(&nativeAwdlEngineThreadExited,false,__ATOMIC_RELEASE);
 retain();
 thread_t created=nullptr;
 if(kernel_thread_start(nativeAwdlEngineThreadMain,this,&created)!=KERN_SUCCESS){
  release();
  __atomic_store_n(&nativeAwdlEngineThreadExited,true,__ATOMIC_RELEASE);
  __atomic_store_n(&nativeAwdlEngineRun,false,__ATOMIC_RELEASE);
  return false;
 }
 nativeAwdlEngineThread=created;
 thread_deallocate(created);
 // The reporter only requests a native-thread pass; it never enters Family
 // or waits for its gate. Refuse start if this posting worker cannot run.
 if(!nativeAwdlReporterStart()){
  __atomic_store_n(&nativeAwdlEngineRun,false,__ATOMIC_RELEASE);
  IOLog("MT7922_AWD6 engine thread start refused: reporter\n");
  return false;
 }
 IOLog("MT7922_AWD6 engine thread started\n");
 return true;
}

bool MT7922AngusSWCore::nativeAwdlReporterStart(){
 if(!__atomic_load_n(&awdlReporterExited,__ATOMIC_ACQUIRE))return false;
 __atomic_store_n(&awdlReporterRun,true,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlReporterExited,false,__ATOMIC_RELEASE);
 retain();
 thread_t created=nullptr;
 if(kernel_thread_start(nativeAwdlReporterThreadMain,this,&created)!=KERN_SUCCESS){
  release();
  __atomic_store_n(&awdlReporterExited,true,__ATOMIC_RELEASE);
  __atomic_store_n(&awdlReporterRun,false,__ATOMIC_RELEASE);
  return false;
 }
 awdlReporterThread=created;
 thread_deallocate(created);
 return true;
}

void MT7922AngusSWCore::nativeAwdlReporterThreadMain(void *param,wait_result_t){
 auto *core=static_cast<MT7922AngusSWCore*>(param);
 if(core)while(__atomic_load_n(&core->awdlReporterRun,__ATOMIC_ACQUIRE)&&
              !__atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE)){
  __atomic_store_n(&core->awdlFamilyReportPending,true,__ATOMIC_RELEASE);
  IOSleep(5);
 }
 if(core){
  __atomic_store_n(&core->awdlReporterExited,true,__ATOMIC_RELEASE);
  core->release();
 }
 thread_terminate(current_thread());
}

// A dedicated source only for the three deferred AWDL Family actions. The
// general tick may call attach/reset/IOUC/queue maintenance and must not be
// moved wholesale onto this workloop thread (BCM uses both timer priorities).
void MT7922AngusSWCore::awdlFamilyTick(OSObject *owner,IOTimerEventSource *source){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 if(!core||source!=core->awdlFamilyTimer||
    __atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE)||
    !__atomic_load_n(&core->nativeStartComplete,__ATOMIC_ACQUIRE))return;
 core->serviceNativeAwdlFamilyWork();
 if(source==core->awdlFamilyTimer&&
    !__atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE)&&
    __atomic_load_n(&core->nativeAwdlEngineRun,__ATOMIC_ACQUIRE))source->setTimeoutMS(1);
}

// Called by the timer event source, not runAction/tryCloseGate: those APIs
// serialize callers but can still run inline on a foreign kernel thread.
void MT7922AngusSWCore::serviceNativeAwdlFamilyWork(){
 if(!nativeWork||!nativeWork->inGate()||!nativeWork->onThread()){
  ++awdlFamilyWrongThread;
  setProperty("AWD6FamilyWrongThread",awdlFamilyWrongThread,32);return;
 }
 if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return;
 const bool live=__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE);
 if(!live){
  __atomic_store_n(&awdlFamilyRefreshPending,false,__ATOMIC_RELEASE);
  __atomic_store_n(&awdlFamilyHandoffPending,false,__ATOMIC_RELEASE);
  __atomic_store_n(&awdlFamilyReportPending,false,__ATOMIC_RELEASE);
  return;
 }
 if(radioClock(nullptr)>=awdlRxEvidenceAtMs){
  awdlRxEvidenceAtMs=radioClock(nullptr)+1000;
  // Allocate and publish only on Family work, never in the TX callback.
  if(awdlLowTxCaptureEnabled){
   const size_t bytes=mt7922anguscore::awdl::LowTxCapture::SnapshotBytes;
   void *copy=IOMalloc(bytes);
   if(copy){
    if(awdlLowTxCapture.snapshot(copy,bytes))setProperty("AWD6LowTxCapture",copy,static_cast<unsigned>(bytes));
    IOFree(copy,bytes);
   }
   const size_t statusBytes=mt7922anguscore::awdl::LowTxCapture::StatusSnapshotBytes;
   void *statuses=IOMalloc(statusBytes);
   if(statuses){
    if(awdlLowTxCapture.snapshotStatuses(statuses,statusBytes))setProperty("AWD6LowTxStatusCapture",statuses,static_cast<unsigned>(statusBytes));
    IOFree(statuses,statusBytes);
   }
   const size_t rxBytes=mt7922anguscore::awdl::LowRxCapture::SnapshotBytes;
   void *rx=IOMalloc(rxBytes);
   if(rx){
    if(awdlLowRxCapture.snapshot(rx,rxBytes))setProperty("AWD6LowRxCapture",rx,static_cast<unsigned>(rxBytes));
    IOFree(rx,rxBytes);
   }
  }
  setProperty("AWD6FirmwareUnicastSuccess",__atomic_load_n(&awdlUnicastFreeOk,__ATOMIC_RELAXED),32);
  setProperty("AWD6FirmwareUnicastFailed",__atomic_load_n(&awdlUnicastFreeFailed,__ATOMIC_RELAXED),32);
  setProperty("AWD6FirmwareBroadcastCompleted",__atomic_load_n(&awdlBroadcastFree,__ATOMIC_RELAXED),32);
 }
 serviceNativeAwdlPeerCacheCompletions();
 const bool reportPending=__atomic_exchange_n(&awdlFamilyReportPending,false,__ATOMIC_ACQ_REL)||
    __atomic_load_n(&awdlAwStartPending,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&awdlAwEndPending,__ATOMIC_ACQUIRE);
 if(!reportPending&&!__atomic_load_n(&awdlFamilyHandoffPending,__ATOMIC_ACQUIRE)&&
    !__atomic_load_n(&awdlFamilyRefreshPending,__ATOMIC_ACQUIRE))return;
 if(reportPending){
  setProperty("AWD6FamilyDispatchStage","report");
  nativeAwdlReportAction(this,nullptr,nullptr,nullptr,nullptr);
 }
 // Clear only AFTER the callback: the radio worker treats pending as a
 // barrier, keeping its template/context parked until Family has returned.
 if(__atomic_load_n(&awdlFamilyHandoffPending,__ATOMIC_ACQUIRE)){
  setProperty("AWD6FamilyDispatchStage","handoff");
  const auto before=nativeActionRx.acceptedCount();
  nativeAwdlHandoffAction(this,nullptr,nullptr,nullptr,nullptr);
  __atomic_store_n(&awdlFamilyHandoffPending,false,__ATOMIC_RELEASE);
  // An RX producer can enqueue while Family is processing the last batch.
  // Keep its wakeup rather than clearing it after the callback returns.
  if(nativeActionRx.acceptedCount()!=before)
   __atomic_store_n(&awdlFamilyHandoffPending,true,__ATOMIC_RELEASE);
  setProperty("AWD6ActionRxOutsideWindow",__atomic_load_n(&awdlActionsOutsideWindow,__ATOMIC_RELAXED),32);
 }
 if(__atomic_load_n(&awdlFamilyRefreshPending,__ATOMIC_ACQUIRE)){
  setProperty("AWD6FamilyDispatchStage","template");
  const IOReturn result=nativeAwdlRefreshTemplateAction(this,nullptr,nullptr,nullptr,nullptr);
  setProperty("AWD6EngineTemplateRefresh",static_cast<UInt32>(result),32);
  __atomic_store_n(&awdlFamilyRefreshPending,false,__ATOMIC_RELEASE);
 }
 setProperty("AWD6FamilyDispatchStage","idle");
 ++awdlFamilyDispatches;
 setProperty("AWD6FamilyDispatches",awdlFamilyDispatches,32);
}

// Only the real native workloop enters Family. No packet/data/control lock
// is held across reportDataPathEvents: completion can synchronously reenter
// cache-control, enqueue packets, or trigger another MI template.
void MT7922AngusSWCore::serviceNativeAwdlPeerCacheCompletions(){
 if(!nativeWork||!nativeWork->inGate()||!nativeWork->onThread()||!packetLock||
    __atomic_load_n(&stopping,__ATOMIC_ACQUIRE)||
    !__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE))return;
 auto *manager=proximity?MT7922GetNativePeerManager(proximity):nullptr;
 if(!manager||!manager->metaCast("IO80211AWDLPeerManager"))return;
 for(unsigned n=0;n<mt7922anguscore::awdl::PeerCacheControl::Capacity;++n){
  if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)||!__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE))break;
  UInt8 completion[mt7922anguscore::awdl::PeerCacheControl::CompletionBytes];
  IOLockLock(packetLock);const bool ready=awdlPeerCache.popCompletion(completion);IOLockUnlock(packetLock);
  if(!ready)break;
  const UInt32 result=MT7922AwdlReportEvents(manager,0x33,completion,sizeof completion);
  setProperty("AWD6PeerCacheLastEventResult",result,32); // raw Family result, not a routability assertion
  ++awdlPeerCacheCompletions;
  setProperty("AWD6PeerCacheCompletions",awdlPeerCacheCompletions,32);
  UInt32 op=0;memcpy(&op,completion+4,4);setProperty("AWD6PeerCacheLastCompletion",op,32);
 }
}

// 1.2.143 synchronous AW-start uplink. RE: postMessage is the
// IO80211SkywalkInterface virtual inherited by IO80211VirtualInterface at slot
// 355 (0xb18), signature void(UInt32,void*,unsigned long,bool). Call it through
// the object vtable (like the C++ virtual in MtkWireless) with sync=true, on the
// workloop, right at window open so the Family sees the AW start in-window.
typedef void (*MT7922InterfacePostMessageFn)(void*,UInt32,void*,unsigned long,bool);
static bool MT7922PostMessageSync(void *self,UInt32 message,void *data,unsigned long length){
 if(!self)return false;
 void **vtable=*reinterpret_cast<void***>(self);
 if(!vtable||!vtable[355])return false;
 reinterpret_cast<MT7922InterfacePostMessageFn>(vtable[355])(self,message,data,length,true);
 return true;
}
IOReturn MT7922AngusSWCore::nativeAwdlAwStartPostAction(OSObject *owner,void*,void*,void*,void*){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 if(!core||!core->nativeWork||!core->nativeWork->inGate()||!core->nativeWork->onThread())return kIOReturnNotPermitted;
 if(!core->bus||!core->proximity||__atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE)||
    !__atomic_load_n(&core->nativeAwdlEngineRun,__ATOMIC_ACQUIRE))return kIOReturnNotReady;
 UInt8 payload[36];
 IOLockLock(core->bus->dataPathLock);memcpy(payload,core->awdlAwPendingPayload,sizeof payload);IOLockUnlock(core->bus->dataPathLock);
 const bool ok=MT7922PostMessageSync(core->proximity,0x29,payload,sizeof payload);
 core->setProperty("AWD6AwStartSyncPost",ok);
 // On success the deferred reporter must not resend the same start.
 if(ok)__atomic_store_n(&core->awdlAwStartPending,0U,__ATOMIC_RELEASE);
 return kIOReturnSuccess;
}

IOReturn MT7922AngusSWCore::nativeAwdlReportAction(OSObject *owner,void*,void*,void*,void*){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 if(!core->nativeWork||!core->nativeWork->inGate()||!core->nativeWork->onThread())return kIOReturnNotPermitted;
 if(!__atomic_load_n(&core->awdlReporterRun,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&core->stopping,__ATOMIC_ACQUIRE))return kIOReturnNotReady;
 core->nativeAwdlReporterLoop();return kIOReturnSuccess;
}
void MT7922AngusSWCore::nativeAwdlReporterLoop(){
 // A single bounded pass on the controller workloop thread.
 if(!nativeWork||!nativeWork->inGate()||!nativeWork->onThread())return;
  auto *manager=proximity?MT7922GetNativePeerManager(proximity):nullptr;
  if(manager&&!manager->metaCast("IO80211AWDLPeerManager"))manager=nullptr;
  const auto *ivars=manager?*reinterpret_cast<const UInt8*const*>(
   reinterpret_cast<const UInt8*>(manager)+0x20):nullptr;
  // End first. A coalesced start may only be delivered if its RF window is
  // still active; otherwise it would reopen the Family gate after expiry.
  if(__atomic_exchange_n(&awdlAwEndPending,0U,__ATOMIC_ACQ_REL)){
   if(manager){
    UInt8 end[40];
    mt7922anguscore::awdl::availabilityWindowEnd(radioClock(nullptr)*1000ULL,end);
    MT7922AwdlReportEvents(manager,0x2b,end,sizeof end);
    setProperty("AWD6AwEndPayloadBytes",static_cast<UInt32>(sizeof end),32);
    ++awdlAwEnds;
    setProperty("AWD6AwEnds",awdlAwEnds,32);
    setProperty("AWD6AwInWindow",ivars&&(ivars[0x304]&1));
   }
  }
  if(__atomic_exchange_n(&awdlAwStartPending,0U,__ATOMIC_ACQ_REL)){
   UInt8 payload[36];
   IOLockLock(bus->dataPathLock);
   const bool live=awdlMiWindowActive&&awdlMiWindowDeadlineUs>radioClock(nullptr)*1000&&coexMayPublish()&&
    __atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
    !__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
   memcpy(payload,awdlAwPendingPayload,sizeof payload);
   IOLockUnlock(bus->dataPathLock);
   if(manager){
    // 1.2.144: deliver the AW start synchronously via the postMessage virtual
    // (slot 355, sync=true), exactly like MtkWireless
    // notifyAvailabilityWindowStart. The payload already carries the AW
    // boundary captured at window open, so a few-ms reporter latency no longer
    // drops the report; `live` is recorded, not used to suppress the send.
    const bool syncPost=proximity&&MT7922PostMessageSync(proximity,0x29,payload,sizeof payload);
    if(syncPost){++awdlAwStartSyncPosts;setProperty("AWD6AwStartSyncPosts",awdlAwStartSyncPosts,32);}
    else MT7922AwdlReportEvents(manager,0x29,payload,sizeof payload);
    setProperty("AWD6AwStartLive",live);
    setProperty("AWD6AwStartPayloadBytes",static_cast<UInt32>(sizeof payload),32);
    ++awdlAwReports;
    awdlAwStartInWindow=ivars?(ivars[0x304]&1):0;
    setProperty("AWD6AwReports",awdlAwReports,32);
    setProperty("AWD6AwStartInWindow",awdlAwStartInWindow!=0);
    setProperty("AWD6AwInWindow",ivars&&(ivars[0x304]&1));
   } else {
    ++awdlAwSkips;
    setProperty("AWD6AwSkips",awdlAwSkips,32);
   }
  }
}

void MT7922AngusSWCore::nativeAwdlEngineThreadMain(void *param,wait_result_t){
 auto *core=static_cast<MT7922AngusSWCore*>(param);
 if(core)core->nativeAwdlEngineLoop();
 if(core){
  __atomic_store_n(&core->nativeAwdlEngineThreadExited,true,__ATOMIC_RELEASE);
  core->release();
 }
 thread_terminate(current_thread());
}

void MT7922AngusSWCore::nativeAwdlEngineLoop(){
 using namespace mt7922anguscore::awdl;
 // Native start arms the loop only after the family's enable path returns;
 // a system-driven start that never calls it still proceeds after 5 s.
 for(UInt32 waited=0;waited<5000&&!__atomic_load_n(&nativeAwdlEngineArmed,__ATOMIC_ACQUIRE)&&
     __atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
     !__atomic_load_n(&stopping,__ATOMIC_ACQUIRE);waited+=10)IOSleep(10);
 setProperty("AWD6EngineArmed",__atomic_load_n(&nativeAwdlEngineArmed,__ATOMIC_ACQUIRE));
 while(__atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
       !__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)){
  // These requests are completed by tick on the actual native workloop
  // thread. Never enter Family inline from a thread holding its gate.
  // Keep the radio parked until handoff/template access has finished; no
  // native gate or radio lock is held while this worker defers a window.
  if(__atomic_load_n(&awdlFamilyRefreshPending,__ATOMIC_ACQUIRE)||
     __atomic_load_n(&awdlFamilyHandoffPending,__ATOMIC_ACQUIRE)){
   IOSleep(1);continue;
  }
  if(nativeWork&&radioClock(nullptr)>=coexTemplateRefreshAtMs&&(!radioSession.ownsResources()||radioSession.contextParked())){
   coexTemplateRefreshAtMs=radioClock(nullptr)+64;
   __atomic_store_n(&awdlFamilyRefreshPending,true,__ATOMIC_RELEASE);
   IOSleep(1);continue;
  }
  if(radioSession.contextParked()&&radioSession.tokenRolloverRequired()){
   Mt7922zOWControlGuard control(bus->controlLock);if(control&&!coexReleaseContext())break;
  }
  refreshEnginePeerSchedule();
  UInt8 band=0;const UInt8 channel=nativeAwdlEngineNextChannel(band);
  if(!channel){IOSleep(2);continue;}
  UInt32 grants=0,tx=0,ms=0;bool restored=false;
  const bool ran=channel&&nativeAwdlRunWindow(channel,band,true,grants,tx,ms,restored);
  // Count descriptor submissions from every periodic window, even if its
  // later cleanup fails. This is submission evidence, not an over-air ACK.
  nativeAwdlEngineTx+=tx;
  if(ran)coexFailureStreak=0;
  else if(!awdlWindowTransient)++coexFailureStreak;
  if((radioSession.ownsResources()&&!radioSession.contextParked())||awd6FilterRestorePending||coexFailureStreak>=3){
   setProperty("AWD6CoexFault",awd6FilterRestorePending?"filter-restore":(radioSession.ownsResources()&&!radioSession.contextParked()?"context-release":"window-failures"));
   setProperty("AWD6CoexFailureStreak",coexFailureStreak,32);
   break; // retain uncertain resources; the normal exit performs one cleanup
  }
  if(ran){
   ++nativeAwdlEngineWindows;
   if(tx) setProperty("AWD6EngineWindowVerified",true);
   setProperty("AWD6EngineFirstGrants",grants,32);
   setProperty("AWD6EngineFirstTx",tx,32);
   setProperty("AWD6EngineFirstMs",ms,32);
  }
  setProperty("AWD6EngineWindows",nativeAwdlEngineWindows,32);
  setProperty("AWD6EngineTxTotal",nativeAwdlEngineTx,32);
  // Consume the captured Action on the real Family workloop before the next RF window.
  __atomic_store_n(&awdlFamilyHandoffPending,true,__ATOMIC_RELEASE);
  if(ran){
   ++nativeAwdlEngineRounds;
   setProperty("AWD6EngineRounds",nativeAwdlEngineRounds,32);
   setProperty("AWD6EngineSkips",nativeAwdlEngineSkips,32);
  } else {
   ++nativeAwdlEngineSkips;
   setProperty("AWD6EngineSkips",nativeAwdlEngineSkips,32);
  }
  for(UInt32 slept=0;slept<(channel?2U:5U)&&
      __atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
      !__atomic_load_n(&stopping,__ATOMIC_ACQUIRE);slept+=1)IOSleep(1);
 }
 __atomic_store_n(&nativeAwdlEngineRun,false,__ATOMIC_RELEASE);
 // Asynchronous teardown (matches BCM's async iovar model): the family
 // disable callbacks only request the stop; this thread releases the radio,
 // the RX filter and the reporter, then publishes the real state.
 bool cleaned=!radioSession.ownsResources()&&!awd6FilterRestorePending;
 if(!cleaned&&bus&&bus->controlLock){
  Mt7922zOWControlGuard control(bus->controlLock);
  if(control){
   if(radioSession.ownsResources())coexReleaseContext();
   if(awd6FilterRestorePending)restoreProbeFilter();
  }
  cleaned=!radioSession.ownsResources()&&!awd6FilterRestorePending;
 }
 {
  bool stagedDropped=false;
  if(bus&&bus->dataPathLock){
   IOLockLock(bus->dataPathLock);
   awdlMiWindowActive=false;
   awdlPeriodicMif.reset();
   const auto dropped=awdlMiQueue.clear();awdlMiDropped+=dropped;stagedDropped=dropped!=0;
   IOLockUnlock(bus->dataPathLock);
  }
  if(stagedDropped){
   setProperty("AWD6MiStaged",false);
   setProperty("AWD6MiQueueDepth",static_cast<UInt32>(0),32);
   setProperty("AWD6MiDropped",awdlMiDropped,32);
  }
 }
 IOLockLock(packetLock);awdlPeerCacheCancelled+=awdlPeerCache.count();awdlPeerCache.clear();IOLockUnlock(packetLock);
 setProperty("AWD6PeerCacheCancelled",awdlPeerCacheCancelled,32);
 IOLockLock(bus->dataPathLock);enginePeerScheduleState.reset();awdlPeerSchedules.reset();awdlHostControl={};awdlScheduleOfferSerial=0;IOLockUnlock(bus->dataPathLock);
 enginePeerSchedule={};enginePeerScheduleValid=false;enginePeerScheduleSerial=0;
 mt7922anguscore::awdl::ActionRxRecord discard{};for(unsigned n=0;n<8&&nativeActionRx.take(discard);++n){}
 __atomic_store_n(&awdlReporterRun,false,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlAwStartPending,0U,__ATOMIC_RELEASE);
 for(unsigned i=0;i<60&&!__atomic_load_n(&awdlReporterExited,__ATOMIC_ACQUIRE);++i)IOSleep(5);
 setProperty("AWD6AwReporterExited",__atomic_load_n(&awdlReporterExited,__ATOMIC_ACQUIRE));
 nativeAwdlEngineStarted=false;
 __atomic_store_n(&coexSchedulerActive,false,__ATOMIC_RELEASE);
 setProperty("AWD6EngineStarted",false);
 setProperty("AWD6EngineResourcesHeld",!cleaned);
 setProperty("AWD6EngineStopped",cleaned);
 setProperty("AWD6EngineStopRequested",false);
 setProperty("AWD6EngineMode","stopped");
 IOLog("MT7922_AWD6 engine thread exiting rounds=%u skips=%u cleaned=%u\n",
  nativeAwdlEngineRounds,nativeAwdlEngineSkips,cleaned?1U:0U);
}

bool MT7922AngusSWCore::nativeAwdlRequestStop(const char *source){
 setProperty("AWD6EngineStopSource",source?source:"unknown");
 setProperty("AWD6EngineStopRequested",true);
 __atomic_store_n(&nativeAwdlEngineArmed,false,__ATOMIC_RELEASE);
 __atomic_store_n(&nativeAwdlEngineRun,false,__ATOMIC_RELEASE);
 // 1.2.13: no asynchronous family callbacks during a family teardown. The
 // reporter enters reportDataPathEvents (controller workqueue dataPath lock);
 // stopping it here removes that lock contention while disableAWDL runs.
 __atomic_store_n(&awdlReporterRun,false,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlAwStartPending,0U,__ATOMIC_RELEASE);
 if(__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE)){
  // Nothing to wind down; publish the real state immediately.
  nativeAwdlEngineStarted=false;
  const bool clean=!radioSession.ownsResources()&&!awd6FilterRestorePending;
  setProperty("AWD6EngineStopRequested",false);
  setProperty("AWD6EngineStarted",false);
  setProperty("AWD6EngineStopped",clean);
  setProperty("AWD6EngineResourcesHeld",!clean);
  setProperty("AWD6EngineMode","stopped");
 }
 IOLog("MT7922_AWD6 engine stop requested: %s\n",source?source:"unknown");
 return true;
}

bool MT7922AngusSWCore::nativeAwdlStopEngine(const char *source){
 setProperty("AWD6EngineStopSource",source?source:"unknown");
 __atomic_store_n(&nativeAwdlEngineArmed,false,__ATOMIC_RELEASE);
 __atomic_store_n(&nativeAwdlEngineRun,false,__ATOMIC_RELEASE);
 // A staged family MI cannot be sent once the loop stops; count it as
 // dropped instead of leaving it for an unrelated future window.
 bool stagedDropped=false;
 if(bus&&bus->dataPathLock){
  IOLockLock(bus->dataPathLock);
  awdlMiWindowActive=false;
  awdlPeriodicMif.reset();
  const auto dropped=awdlMiQueue.clear();awdlMiDropped+=dropped;stagedDropped=dropped!=0;
  IOLockUnlock(bus->dataPathLock);
 }
 if(stagedDropped){
  setProperty("AWD6MiStaged",false);
  setProperty("AWD6MiQueueDepth",static_cast<UInt32>(0),32);
  setProperty("AWD6MiDropped",awdlMiDropped,32);
 }
 bool exited=__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
 if(!exited){
  for(unsigned i=0;i<800&&!exited;++i){
   IOSleep(5);
   exited=__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
  }
 }
 if(!exited){
  setProperty("AWD6EngineStopRefused","thread-busy");
  IOLog("MT7922_AWD6 engine stop refused: thread busy\n");
  return false;
 }
 // 1.2.6: stop the AW reporter and drop an undelivered start. A pending end
 // is left to the reporter: it holds no driver lock and exits after the
 // family call returns (the caller may hold the family dataPath lock).
 __atomic_store_n(&awdlReporterRun,false,__ATOMIC_RELEASE);
 __atomic_store_n(&awdlAwStartPending,0U,__ATOMIC_RELEASE);
 for(unsigned i=0;i<60&&!__atomic_load_n(&awdlReporterExited,__ATOMIC_ACQUIRE);++i)IOSleep(5);
 setProperty("AWD6AwReporterExited",__atomic_load_n(&awdlReporterExited,__ATOMIC_ACQUIRE));
 bool cleaned=!radioSession.ownsResources()&&!awd6FilterRestorePending;
 if(!cleaned){
  if(bus&&bus->controlLock){
   Mt7922zOWControlGuard control(bus->controlLock);
   if(control&&!__atomic_load_n(&bus->powerBlocked,__ATOMIC_ACQUIRE)&&
      bus->firmwareReady&&bus->firmwareRunning&&bus->wfdmaReady){
    cleaned=!radioSession.ownsResources()||coexReleaseContext();
    if(cleaned&&awd6FilterRestorePending)cleaned=restoreProbeFilter();
   }
  }
 }
 setProperty("AWD6EngineStopped",cleaned);
 setProperty("AWD6EngineResourcesHeld",radioSession.ownsResources()||awd6FilterRestorePending);
 setProperty("AWD6EngineRounds",nativeAwdlEngineRounds,32);
 setProperty("AWD6EngineSkips",nativeAwdlEngineSkips,32);
 if(!cleaned){
  setProperty("AWD6EngineRefused","stop-resources-held");
  IOLog("MT7922_AWD6 engine stop refused: resources held\n");
  return false;
 }
 nativeAwdlEngineStarted=false;
 setProperty("AWD6EngineStarted",false);
 setProperty("AWD6EngineMode","stopped");
 return true;
}
