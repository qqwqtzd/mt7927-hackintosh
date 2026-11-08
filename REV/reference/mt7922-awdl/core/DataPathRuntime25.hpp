// SPDX-License-Identifier: ISC
// Darwin26 AWDL data channel. The AWDL interface's Skywalk TX queue is the
// family's data path into the driver: its producer query reports free slots,
// transmit() accepts Ethernet frames, and the AW window transmits them to the
// peer with a firmware peer (radioPeer) and the data TxContext. Peer data
// frames received in the window are decoded back to Ethernet and published on
// the interface RX queue. No OS22 code; the frame format is the AWDL wire
// format already used by the native template.
#include "../AWDL/DataFrame.hpp"

// Queue enable/rearm/completion/publication belong to the native controller
// workloop, never the radio thread holding controlLock. Bridge the system's
// successful engine start/stop when it does not call enableDatapath itself.
void MT7922AngusSWCore::serviceAwdlDataPath(){
 const bool live=__atomic_load_n(&proximityReady,__ATOMIC_ACQUIRE)&&
  admissionPowerOn&&!__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)&&
  __atomic_load_n(&nativeAwdlEngineRun,__ATOMIC_ACQUIRE)&&
  !__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE);
 if(live!=awdlDataPathEngineLive){
  awdlDataPathEngineLive=live;
  nativeAwdlDataPath(live);
 }
 // Do not repeatedly enable a live session: an explicit family datapath
 // disable must remain effective until the next engine lifecycle.
 serviceProximityPackets();
}

UInt32 MT7922AngusSWCore::proximityQuery(void *owner,void*,UInt32 *bytes){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 UInt32 space=0;
 if(core&&__atomic_load_n(&core->awdlDataPathEnabled,__ATOMIC_ACQUIRE)&&core->packetLock){
  IOLockLock(core->packetLock);
  space=core->awdlDataTxCount<8?8U-core->awdlDataTxCount:0U;
  IOLockUnlock(core->packetLock);
 }
 if(bytes)*bytes=space?1518U*space:0U;
 if(core)core->setProperty("AWD6DataTxSpace",space,32);
 return space;
}

UInt32 MT7922AngusSWCore::proximityTx(void*,void*,void*const *packets,UInt32 count,void *context){
 auto *core=static_cast<MT7922AngusSWCore*>(context);
 if(!core||!packets)return 0;
 const UInt32 calls=__atomic_add_fetch(&core->awdlTxCalls,1U,__ATOMIC_RELAXED);
 const UInt32 total=__atomic_add_fetch(&core->awdlTxPackets,count,__ATOMIC_RELAXED);
 core->setProperty("AWD6AwdlTxCalls",calls,32);
 core->setProperty("AWD6AwdlTxPackets",total,32);
 if(!__atomic_load_n(&core->awdlDataPathEnabled,__ATOMIC_ACQUIRE))return 0;
 UInt32 accepted=0;
 for(;accepted<count;++accepted){
  const UInt32 n=mt7922anguscore::packet::length(packets[accepted]);
  void *data=nullptr;UInt32 capacity=0;
  if(n<14||n>1518||mt7922anguscore::packet::dataWindow(packets[accepted],&data,&capacity)!=0||n>capacity){
   ++core->awdlDataTxDropped;
   mt7922anguscore::packet::completeTx(packets[accepted],core->proximityResources.txQueue(),core->proximityResources.transmitPool());
   continue;
  }
  bool queued=false;
  IOLockLock(core->packetLock);
  if(core->awdlDataTxCount<8&&core->proximityPackets.reserve()){
   auto &e=core->awdlDataTxQueue[(core->awdlDataTxHead+core->awdlDataTxCount)%8];
   memcpy(e.ether,data,n);
   e.length=static_cast<UInt16>(n);
   memcpy(e.dst,e.ether,6);
   e.packet=packets[accepted];
   e.tries=0;
   ++core->awdlDataTxCount;
   ++core->awdlDataTxQueued;
   queued=true;
  }
  IOLockUnlock(core->packetLock);
  if(!queued)break;
 }
 core->setProperty("AWD6DataTxQueued",core->awdlDataTxQueued,32);
 core->setProperty("AWD6DataTxDropped",core->awdlDataTxDropped,32);
 core->setProperty("AWD6DataTxPending",core->awdlDataTxCount,32);
 return accepted;
}

UInt32 MT7922AngusSWCore::proximityDrain(void *owner){
 auto *core=static_cast<MT7922AngusSWCore*>(owner);
 if(!core)return kIOReturnSuccess;
 core->serviceProximityPackets();
 IOLockLock(core->packetLock);
 const bool empty=core->proximityPackets.outstanding()==0&&core->awdlDataTxCount==0&&!core->proximityCompletionRetry;
 IOLockUnlock(core->packetLock);
 return empty?0:static_cast<UInt32>(kIOReturnBusy);
}

void MT7922AngusSWCore::retireProximityTx(void *packet){
 IOLockLock(packetLock);
 const bool ok=proximityPackets.retire(packet);
 IOLockUnlock(packetLock);
 if(ok){
  ++awdlDataTxCompleted;
  setProperty("AWD6DataTxCompleted",awdlDataTxCompleted,32);
 } else IOLog("MT7922_AWD6 proximity completion ownership violation\n");
}

void MT7922AngusSWCore::serviceProximityPackets(){
 if(!packetLock)return;
 // The engine owns a copied queue head until its thread exits. Only then
 // can stop return unsent packets, without racing a DMA submission.
 if(!__atomic_load_n(&awdlDataPathEnabled,__ATOMIC_ACQUIRE)&&__atomic_load_n(&nativeAwdlEngineThreadExited,__ATOMIC_ACQUIRE)){
  UInt32 returned=0;
  IOLockLock(packetLock);
  while(awdlDataTxCount){
   auto &entry=awdlDataTxQueue[awdlDataTxHead];
   if(!proximityPackets.retire(entry.packet))break;
   entry.packet=nullptr;entry.length=0;
   awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;++awdlDataTxDropped;++returned;
  }
  IOLockUnlock(packetLock);
  if(returned){
   setProperty("AWD6DataTxPending",awdlDataTxCount,32);
   setProperty("AWD6DataTxDropped",awdlDataTxDropped,32);
  }
 }
 if(__atomic_load_n(&awdlDataPathEnabled,__ATOMIC_ACQUIRE)&&proximityResources.isDataPathEnabled())proximityResources.rearmTransmit();
 // Return transmitted packets to the family TX queue.
 for(unsigned i=0;i<32;++i){
  IOLockLock(packetLock);
  void *p=proximityCompletionRetry?proximityCompletionRetry:proximityPackets.completion();
  IOLockUnlock(packetLock);
  if(!p)break;
  const auto result=mt7922anguscore::packet::completeTx(p,proximityResources.txQueue(),proximityResources.transmitPool());
  if(result){proximityCompletionRetry=p;IOLog("MT7922_AWD6 proximity completion failure 0x%x\n",static_cast<unsigned>(result));break;}
  proximityCompletionRetry=nullptr;
 }
 // Publish decoded peer data frames on the AWDL interface RX queue.
 const UInt32 publishedBefore=awdlDataRxPublished,droppedBefore=awdlDataRxDropped;
 for(unsigned i=0;i<32;++i){
  IOLockLock(packetLock);
  auto packet=static_cast<mbuf_t>(awdlDataRx.received());
  IOLockUnlock(packetLock);
  if(!packet)break;
  const size_t n=mbuf_pkthdr_len(packet);
  UInt8 data[1518];
  if(!__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)&&proximityResources.isDataPathEnabled()&&
     n>=14&&n<=sizeof data&&mt7922anguscore::copyMbufChain<MT7922NativeMbufOps>(packet,n,data)){
   void *rx=nullptr;void *window=nullptr;UInt32 capacity=0;
   if(mt7922anguscore::packet::allocate(proximityResources.receivePool(),&rx)==0&&rx){
    bool published=false;
    if(mt7922anguscore::packet::prepareRx(rx)==0&&
       mt7922anguscore::packet::dataWindow(rx,&window,&capacity)==0&&n<=capacity){
     memcpy(window,data,n);
     if(mt7922anguscore::packet::setLength(rx,static_cast<UInt32>(n))==0)
      published=proximityResources.receive(&rx,1)==0;
    }
    if(!published){
     mt7922anguscore::packet::deallocate(proximityResources.receivePool(),rx);
     ++awdlDataRxDropped;
    } else ++awdlDataRxPublished;
   } else ++awdlDataRxDropped;
  } else ++awdlDataRxDropped;
  mbuf_freem(packet);
 }
 if(awdlDataRxPublished!=publishedBefore)setProperty("AWD6DataRxPublished",awdlDataRxPublished,32);
 if(awdlDataRxDropped!=droppedBefore)setProperty("AWD6DataRxDropped",awdlDataRxDropped,32);
}

bool MT7922AngusSWCore::receiveProximityFrame(const void *data,UInt32 bytes){
 using namespace mt7922anguscore::awdl;
 if(!data||!bytes||!bus)return false;
 ++awdlDataRxCalls;
 setProperty("AWD6DataRxCalls",awdlDataRxCalls,32);
 const UInt8 *frame=nullptr;UInt32 length=0;bool translated=false;
 if(!mt7922packet::mt7922RxPayload(data,bytes,&frame,&length,&translated))return false;
 if(!translated&&length>=2&&(mt7922packet::le16(frame)&0x0c)==0x08){
  ++awdlDataRxAnyData;
  setProperty("AWD6DataRxAnyData",awdlDataRxAnyData,32);
 }
 if(translated){
  // The firmware may deliver secondary-BSS AWDL data translated to Ethernet
  // (Darwin26 translation path). Decode it with the translated layout instead
  // of ignoring it; 1.2.22 had TX success but zero RX, which is consistent
  // with the peer answers arriving translated.
  ++awdlDataRxRouted;
  if(!isTranslatedData(frame,length)){setProperty("AWD6DataRxRouted",awdlDataRxRouted,32);return false;}
  ++awdlDataRxTranslated;
  setProperty("AWD6DataRxLayout",isSnapTranslatedData(frame,length)?"8023-apple-snap":"ethernet-0800-awdl");
  UInt8 ethernet[1518];size_t written=0;UInt16 sequence=0;
  if(!decodeTranslatedData(frame,length,radioConfiguration.address,ethernet,sizeof ethernet,written,sequence)){
   ++awdlDataRxDropped;
   setProperty("AWD6DataRxRouted",awdlDataRxRouted,32);
   setProperty("AWD6DataRxTranslated",awdlDataRxTranslated,32);
   setProperty("AWD6DataRxDropped",awdlDataRxDropped,32);
   return true;
  }
  ++awdlDataRxFrames;
  mbuf_t packet=nullptr;
  if(mbuf_allocpacket(MBUF_DONTWAIT,written,nullptr,&packet)||!packet){++awdlDataRxDropped;}
  else if(mbuf_copyback(packet,0,written,ethernet,MBUF_DONTWAIT)){mbuf_freem(packet);++awdlDataRxDropped;}
  else {
   // copyback populated each segment; only the packet-header total is ours.
   mbuf_pkthdr_setlen(packet,written);
   IOLockLock(packetLock);
   const bool accepted=awdlDataRx.receive(packet);
   IOLockUnlock(packetLock);
   if(!accepted){mbuf_freem(packet);++awdlDataRxDropped;}
  }
  setProperty("AWD6DataRxRouted",awdlDataRxRouted,32);
  setProperty("AWD6DataRxTranslated",awdlDataRxTranslated,32);
  setProperty("AWD6DataRxFrames",awdlDataRxFrames,32);
  setProperty("AWD6DataRxDropped",awdlDataRxDropped,32);
  return true;
 }
 if(!isDataFrame(frame,length,radioConfiguration.bssid)){setProperty("AWD6DataRxRouted",awdlDataRxRouted,32);return false;}
 ++awdlDataRxRouted;
 ++awdlDataRxRaw;
 ++awdlDataRxFrames;
 setProperty("AWD6DataRxRouted",awdlDataRxRouted,32);
 setProperty("AWD6DataRxRaw",awdlDataRxRaw,32);
 setProperty("AWD6DataRxFrames",awdlDataRxFrames,32);
 const auto ok=decodeDataPackets(frame,length,radioConfiguration.address,[&](const UInt8 *ethernet,size_t n,UInt16){
  if(n<14||n>1518){++awdlDataRxDropped;return true;}
  mbuf_t packet=nullptr;
  if(mbuf_allocpacket(MBUF_DONTWAIT,n,nullptr,&packet)||!packet){++awdlDataRxDropped;return true;}
  if(mbuf_copyback(packet,0,n,ethernet,MBUF_DONTWAIT)){mbuf_freem(packet);++awdlDataRxDropped;return true;}
  mbuf_pkthdr_setlen(packet,n);
  IOLockLock(packetLock);
  const bool accepted=awdlDataRx.receive(packet);
  IOLockUnlock(packetLock);
  if(!accepted){mbuf_freem(packet);++awdlDataRxDropped;}
  return true;
 },radioConfiguration.bssid);
 if(!ok)++awdlDataRxDropped;
 setProperty("AWD6DataRxPublished",awdlDataRxPublished,32);
 setProperty("AWD6DataRxDropped",awdlDataRxDropped,32);
 return ok;
}

bool MT7922AngusSWCore::receiveProximityEthernet(const UInt8*,size_t,UInt16){
 // Ethernet delivery is handled by the decoded RX queue above.
 return false;
}

void MT7922AngusSWCore::nativeAwdlDataPath(bool enable){
 if(enable==__atomic_load_n(&awdlDataPathEnabled,__ATOMIC_ACQUIRE))return;
 if(!proximityResources.isRegistered())return;
 const auto result=proximityResources.setDataPathEnabled(enable);
 if(result)return;
 __atomic_store_n(&awdlDataPathEnabled,enable,__ATOMIC_RELEASE);
 if(enable)proximityResources.requestTransmit();
 setProperty("AWD6DataPathEnabled",enable);
 IOLog("MT7922_AWD6 data path %s\n",enable?"enabled":"disabled");
}

void MT7922AngusSWCore::serviceAwdlDataTx(const mt7922anguscore::awdl::TxContext &window,
 const mt7922anguscore::awdl::PeerSchedule *plan){
 using namespace mt7922anguscore::awdl;
 if(!bus||!bus->dataPathLock||!__atomic_load_n(&awdlDataPathEnabled,__ATOMIC_ACQUIRE))return;
 if(!window.lease.generation||!window.deadlineUs)return;
 UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
 const UInt64 nowUs=ns/1000;
 // 1.2.153: leave an 8ms RX tail after any TX so the peer's immediate reply
 // (SYN-ACK ~0.2ms after our SYN) is still received on the AWDL channel before
 // the window closes and the radio returns to the primary channel.
 if(nowUs+8000>=window.deadlineUs)return;
 // Peek the head destination before the gate: the edge rule depends on the
 // class. OWL 0.4.1 sync.rs: a fixed-channel radio uses the 3 TU guard for
 // both classes, but multicast goes through Clock::edge_wait, which also
 // requires clearing the slot's START edge; unicast's same-channel exception
 // makes its start guard zero. This engine is fixed-channel (same-channel
 // windows), so the guard stays 3 TU and only multicast gains the start check.
 UInt8 headDst[6]={};bool haveHead=false;
 IOLockLock(packetLock);
 if(awdlDataTxCount){memcpy(headDst,awdlDataTxQueue[awdlDataTxHead].dst,6);haveHead=true;}
 IOLockUnlock(packetLock);
 if(!haveHead)return;
 const bool headUnicast=!(headDst[0]&1);
 // 1.2.39 AWDL-11/16: data may only go to a peer in a slot where both are
 // on the same channel, clear of the slot edges. The edge rule itself lives in
 // PeerSchedule.hpp slotAdmits() so it can be tested off-hardware. A deferred
 // frame keeps its queue entry; it is not a transmission failure.
 const PeerSchedule *admission=plan?plan:(enginePeerScheduleValid?&enginePeerSchedule:nullptr);
 if(plan&&(!plan->fresh(nowUs)||!awdlCurrentWindowChannel))return;
 if(admission&&awdlCurrentWindowChannel&&admission->fresh(nowUs)){
  PeerWindow peerWindow{};
  const bool shared=admission->window(nowUs,peerWindow)&&
   slotAdmits(nowUs,peerWindow,awdlCurrentWindowChannel,headUnicast);
  if(!shared){
   ++awdlSlotDeferred;
   setProperty("AWD6SlotDeferred",awdlSlotDeferred,32);
   return;
  }
  ++awdlSlotTx;
  setProperty("AWD6SlotTx",awdlSlotTx,32);
 }
 // The local plan only proves that OUR radio is in this slot. For a
 // unicast, the destination's receive window is an additional condition;
 // the elected root is not a substitute for the transfer peer's schedule.
 if(headUnicast){
  PeerSchedule destination{};PeerWindow peerSlot{};
  IOLockLock(bus->dataPathLock);
  const bool known=awdlPeerSchedules.get(headDst,nowUs,destination);
  IOLockUnlock(bus->dataPathLock);
  if(!known||!destination.window(nowUs,peerSlot)||
     !slotAdmits(nowUs,peerSlot,awdlCurrentWindowChannel,true)){
   setProperty("AWD6DataTxDeferred",known?"destination-channel":"destination-phase-missing");
   return;
  }
 }
 AwdlDataTxEntry entry{};
 IOLockLock(packetLock);
 if(!awdlDataTxCount){IOLockUnlock(packetLock);return;}
 entry=awdlDataTxQueue[awdlDataTxHead];
 IOLockUnlock(packetLock);
 // Darwin26 data frames are both unicast (TCP/SRV resolution) and IPv6
 // multicast (mDNS ff02::fb -> 33:33:..). Multicast rides the lease group
 // WTBL that radioSession.start already programmed; only unicast needs a
 // per-peer WTBL. 1.2.21 tried radioPeer.open for every DA, so all multicast
 // frames failed before the MCU (DataPeerStatus=0, NoPeer=28x3) and mDNS
 // never left the Mac.
 const bool unicast=!(entry.dst[0]&1);
 UInt8 wcid=0;
 if(unicast){
  wcid=radioPeer.find(window.lease,entry.dst);
  // Peer programming belongs to pre-grant preparation. Keep a new queue
  // head for the next window; never spend a live RF lease in synchronous MCU.
  if(!wcid){
   ++awdlDataTxNoPeer;
   IOLockLock(packetLock);
   const bool drop=awdlDataTxQueue[awdlDataTxHead].tries>=3;
   void *packet=nullptr;
   if(drop){
    packet=awdlDataTxQueue[awdlDataTxHead].packet;
    awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;
    proximityPackets.retire(packet);
    ++awdlDataTxDropped;
   }
   IOLockUnlock(packetLock);
   setProperty("AWD6DataTxNoPeer",awdlDataTxNoPeer,32);
   return;
  }
 }
 // Raw 802.11 TXWI cannot consume the stack's Ethernet checksum-offload
 // request. Complete UDP/TCP/ICMPv6 on our private queue-head copy, as the
 // Darwin22 path already does; never modify the family's owned packet.
 UInt8 checksumProtocol=0;bool checksumChanged=false;
 const auto checksum=completeIPv6Checksum(entry.ether,entry.length,checksumProtocol,&checksumChanged);
 if(checksum!=ChecksumResult::Completed){
  IOLockLock(packetLock);
  void *packet=awdlDataTxQueue[awdlDataTxHead].packet;
  awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;
  proximityPackets.retire(packet);++awdlDataTxDropped;
  IOLockUnlock(packetLock);
  if(checksum==ChecksumResult::Malformed)++awdlTxChecksumMalformed;else ++awdlTxChecksumUnsupported;
  setProperty("AWD6TxChecksumMalformed",awdlTxChecksumMalformed,32);
  setProperty("AWD6TxChecksumUnsupported",awdlTxChecksumUnsupported,32);
  setProperty("AWD6DataTxDropped",awdlDataTxDropped,32);return;
 }
 TxContext ctx=window;
 ctx.data=true;ctx.unicast=unicast;
 if(unicast){ctx.peerWcid=wcid;memcpy(ctx.peerAddress,entry.dst,6);}
 UInt8 wire[1600];size_t written=0;
 const UInt16 sequence=static_cast<UInt16>(__atomic_add_fetch(&awdlDataSequence,1U,__ATOMIC_RELAXED));
 if(!encodeData(entry.ether,entry.length,ctx.source,sequence,wire,sizeof wire,written,ctx.bssid)){
  IOLockLock(packetLock);
  void *packet=awdlDataTxQueue[awdlDataTxHead].packet;
  awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;
  proximityPackets.retire(packet);
  ++awdlDataTxDropped;
  IOLockUnlock(packetLock);
  setProperty("AWD6DataTxDropped",awdlDataTxDropped,32);
  return;
 }
 bool sent=false;
 IOLockLock(bus->dataPathLock);
 if(radioSession.state()==roc::State::Granted&&!radioSession.tick(radioClock(nullptr)))
  sent=bus->submitNetworkFrame(wire,static_cast<UInt32>(written),1,entry.packet,&ctx);
 IOLockUnlock(bus->dataPathLock);
 if(sent){
  IOLockLock(packetLock);
  awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;
  IOLockUnlock(packetLock);
  ++awdlTxChecksumCompleted;if(checksumChanged)++awdlTxChecksumChanged;
  if(checksumProtocol==17)++awdlTxChecksumUDP;else if(checksumProtocol==6)++awdlTxChecksumTCP;else if(checksumProtocol==58)++awdlTxChecksumICMP;
  setProperty("AWD6TxChecksumCompleted",awdlTxChecksumCompleted,32);
  setProperty("AWD6TxChecksumChanged",awdlTxChecksumChanged,32);
  setProperty("AWD6TxChecksumUDP",awdlTxChecksumUDP,32);setProperty("AWD6TxChecksumTCP",awdlTxChecksumTCP,32);
  setProperty("AWD6TxChecksumICMP",awdlTxChecksumICMP,32);
  ++awdlDataTxSent;
  setProperty("AWD6DataTxSent",awdlDataTxSent,32);
  setProperty("AWD6DataTxPending",awdlDataTxCount,32);
 } else {
  IOLockLock(packetLock);
  ++awdlDataTxQueue[awdlDataTxHead].tries;
  const bool drop=awdlDataTxQueue[awdlDataTxHead].tries>=3;
  void *packet=nullptr;
  if(drop){
   packet=awdlDataTxQueue[awdlDataTxHead].packet;
   awdlDataTxHead=(awdlDataTxHead+1)%8;--awdlDataTxCount;
   proximityPackets.retire(packet);
   ++awdlDataTxDropped;
  }
  IOLockUnlock(packetLock);
  setProperty("AWD6DataTxDropped",awdlDataTxDropped,32);
 }
}
