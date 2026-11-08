// SPDX-License-Identifier: ISC
// Included by ModernCore.cpp only for the Darwin24/25 network-only profile.
#include "../Compatibility/WCLStatus.hpp"
#include "../Compatibility/WCLChannels.hpp"
#include "../Compatibility/WCLExtendedBss.hpp"
#include "../Compatibility/WCLCapabilities.hpp"
#include "../Compatibility/WCLCipherKey.hpp"
#include "../Compatibility/WCLHeCapability.hpp"
SInt32 MT7922AngusSWCore::isCommandProhibited(int){
    return __atomic_load_n(&stopping,__ATOMIC_ACQUIRE)?kIOReturnNotReady:kIOReturnSuccess;
}
SInt32 MT7922AngusSWCore::handleCardSpecific(IO80211SkywalkInterface*,unsigned long,void*,bool){return kIOReturnUnsupported;}
SInt32 MT7922AngusSWCore::getDRIVER_VERSION(IO80211SkywalkInterface *i,apple80211_version_data *p){return MT7922WclStatus(apple80211SkywalkRequest(0xc03069c9U,APPLE80211_IOC_DRIVER_VERSION,i,p));}
SInt32 MT7922AngusSWCore::getHARDWARE_VERSION(IO80211SkywalkInterface *i,apple80211_version_data *p){return MT7922WclStatus(apple80211SkywalkRequest(0xc03069c9U,APPLE80211_IOC_HARDWARE_VERSION,i,p));}
SInt32 MT7922AngusSWCore::getCARD_CAPABILITIES(IO80211SkywalkInterface *i,apple80211_capability_data *p){
    if(!p)return kIOReturnBadArgument;
    // The AWDL interface must answer the bind's request 12 too; its reply is
    // the conservative zero-capability payload from apple80211SkywalkRequest.
    if(i!=interfaceObject)return MT7922WclStatus(apple80211SkywalkRequest(0xc03069c9U,APPLE80211_IOC_CARD_CAPABILITIES,i,p));
    const UInt32 queries=__atomic_add_fetch(&wclCardCapabilityQueries,1U,__ATOMIC_RELAXED);
    setProperty("MT7922WCLCardCapabilityQueries",queries,32);
    // Separately measured native allocations: 24 bytes on 15, 28 on 26.
    // Keep the internal 13 structure local; add modern protocol bits below.
    apple80211_capability_data internal{};
    const auto result=apple80211SkywalkRequest(0xc03069c9U,APPLE80211_IOC_CARD_CAPABILITIES,i,&internal);
    if(result)return MT7922WclStatus(result);
    memset(p,0,MT7922_NATIVE_DARWIN==24?24:28);
    memcpy(p,&internal,sizeof(internal));
#if MT7922_NATIVE_DARWIN == 25
    const bool pskAdmission=mt7922wcl::enableNativePskAdmission(p,28);
    setProperty("MT7922WCLPskAdmission",pskAdmission);
    setProperty("MT7922WCLHeCapabilityAdvertised",mt7922wcl::enableHeCapability(p,28));
    // IO80211.framework Apple80211Scan tests bitmap byte 5, bit 6 before
    // creating/waiting on its scan-completion semaphore. Without it a live
    // scan immediately reads results, before WCL has completed the request
    // (0xe0820445). AppleBCMWLANCore sets the same bit at payload +9.
    // Our WCL bridge supplies real results (0xc9) and completion (0xed);
    // advertise that asynchronous protocol, not additional radio features.
    constexpr unsigned asyncScanCompletionBit=46;
    auto *bitmap=reinterpret_cast<UInt8*>(p)+sizeof(UInt32);
    bitmap[asyncScanCompletionBit/8]|=UInt8(1u<<(asyncScanCompletionBit%8));
    // The family's capability bitmap starts at payload+4:
    // isCardCapabilitiesSet (0x229a2b2) indexes 0xb34+cap/8 over the buffer
    // loaded at controller+0x120+0xb30 by loadCardCapabilities (0x229a202).
    // bit 2  = configure's "invalid card capabilities" diagnostic gate
    //          (0x21c56d8, non-fatal); BCM sets it by default (0xe66f at
    //          payload+4) and MT7922 uses AES-CCMP for WPA2.
    // bit 29 = dual-band gate of configureValidAWDLChannels (0x21cab1e reads
    //          payload+7 bit5); MT7922 genuinely operates on 5GHz (STA on
    //          ch149 in this project). BCM sets it when feature flag 0x1c is
    //          present (0x16610e8..0x16610fd).
    constexpr unsigned aesCapabilityBit=2,dualBandCapabilityBit=29;
    bitmap[aesCapabilityBit/8]|=UInt8(1u<<(aesCapabilityBit%8));
    bitmap[dualBandCapabilityBit/8]|=UInt8(1u<<(dualBandCapabilityBit%8));
    setProperty("MT7922WCLDualBandAdvertised",true);
    // bit 94 = WCLDeviceConfiguration::is6GSupported (payload+0xf bit6);
    // advertise only when the configured regulatory profile admits 6 GHz.
    // bit 96 = isOWESupport (payload+0x10 bit0); the driver implements OWE.
    const bool sixGHz=mt7922reg::sixGHzAvailable(mt7922reg::config());
    const bool sixGHzOk=mt7922wcl::setSixGHzCapability(p,28,sixGHz);
    const bool oweOk=mt7922wcl::setOweCapability(p,28,true);
    setProperty("MT7922WCLSixGHzAdvertised",sixGHzOk&&sixGHz);
    setProperty("MT7922WCLOweAdvertised",oweOk);
#endif
    // Capability bytes as advertised to the family: lets a later comparison
    // name which modern bits (AWDL/proximity/async scan) are present.
    char hex[3*28+1];size_t pos=0;
    const unsigned bytes=MT7922_NATIVE_DARWIN==24?24:28;
    for(unsigned i=0;i<bytes;++i)
        pos+=static_cast<size_t>(snprintf(hex+pos,sizeof hex-pos,"%02x",
            reinterpret_cast<const UInt8*>(p)[i]));
    hex[pos<sizeof hex?pos:sizeof hex-1]=0;
    setProperty("MT7922CardCapabilities",hex);
    return kIOReturnSuccess;
}
SInt32 MT7922AngusSWCore::getPOWER(IO80211SkywalkInterface *i,apple80211_power_data *p){return MT7922WclStatus(admissionRequest(0xc03069c9U,APPLE80211_IOC_POWER,i,p));}
SInt32 MT7922AngusSWCore::setPOWER(IO80211SkywalkInterface *i,apple80211_power_data *p){return MT7922WclStatus(admissionRequest(0x803069c8U,APPLE80211_IOC_POWER,i,p));}
SInt32 MT7922AngusSWCore::getCOUNTRY_CODE(IO80211SkywalkInterface *i,apple80211_country_code_data *p){return MT7922WclStatus(apple80211SkywalkRequest(0xc03069c9U,APPLE80211_IOC_COUNTRY_CODE,i,p));}
SInt32 MT7922AngusSWCore::setCOUNTRY_CODE(IO80211SkywalkInterface*,apple80211_country_code_data*){return kIOReturnUnsupported;}
SInt32 MT7922AngusSWCore::setGET_DEBUG_INFO(IO80211SkywalkInterface*,apple80211_debug_command*){return kIOReturnUnsupported;}
CCLogStream *MT7922AngusSWCore::getLogger() const{return reinterpret_cast<CCLogStream*>(familyLogger);}
CommonFaultReporter *MT7922AngusSWCore::getFaultReporterFromDriver(){return reinterpret_cast<CommonFaultReporter*>(familyFaultReporter);}
#if MT7922_NATIVE_DARWIN == 24
int MT7922AngusSWInterface::setMacAddress(ether_addr &address){
    return setHardwareAddress(&address);
}
#else
int MT7922AngusSWInterface::setMacAddress(mloAddrArray &addresses){
    // WCL passes a 26-byte address bundle, but its join path programs only
    // the primary six-byte station address. MT7922 is single-link; do not
    // reject a valid private address because unused MLO metadata is present.
    ether_addr primary{};
    memcpy(primary.octet,&addresses,6);
    return setHardwareAddress(&primary);
}
#endif
extern "C" void MT7922WclSetCurrentAp(IO80211InfraInterface*,ether_addr*) __asm("__ZN21IO80211InfraInterface19setCurrentApAddressEP10ether_addr");
SInt32 MT7922AngusSWCore::wclHardwareRequest(MT7922AngusSWInterface *interface,int selector,void *data,size_t bytes){
    if(!bus||interface!=interfaceObject||__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return kIOReturnNotReady;
    const unsigned request=unsigned(selector)&0xffffU;
    const bool set=(selector&0x10000)!=0;
    setProperty("MT7922WCLLastSelector",request,32);
    {
        // Ring of the last eight apple80211 selectors (s=set, g=get) so an
        // AirDrop/AWDL request can be identified even when it falls through
        // to the generic unsupported path.
        const UInt32 n=__atomic_add_fetch(&wclSelectorCount,1U,__ATOMIC_RELAXED);
        wclSelectorRing[(n-1)&7U]=request|(set?0x10000U:0U);
        char trace[160];size_t pos=0;trace[0]=0;
        const unsigned shown=n<8?n:8;
        for(unsigned i=0;i<shown;++i){
            const UInt32 v=wclSelectorRing[(n-shown+i)&7U];
            const int written=snprintf(trace+pos,sizeof trace-pos,"%s0x%x%s",pos?",":"",v&0xffffU,(v&0x10000U)?"s":"g");
            if(written>0)pos+=static_cast<size_t>(written);
        }
        setProperty("MT7922WCLSelectors",trace);
        if((n&63U)==1U)IOLog("MT7922_WCL_SEL count=%u last=0x%x %s\n",n,request,set?"set":"get");
    }
    // IO80211Family's static setVIRTUAL_IF_CREATE (0xffffff80022886a8) checks a
    // 32-byte request (cmpl $0x20,0x18(req)) and forwards that same pointer to
    // this slot, so the payload extent is proven for this selector alone.
    // apple80211_virt_if_create_data is exactly 32 bytes:
    //   +0 version, +4 mac[6], +10 unk1, +12 role, +16 bsd_name[16].
    // Role 6 is AWDL (apple80211_virtual_interface_type, __IO80211_TARGET >= 13:
    // P2P_DEVICE=3, P2P_CLIENT=4, P2P_GO=5, AWDL=6, SOFT_AP=7). Any other role
    // is refused instead of being mapped onto the AWDL factory, and the request
    // is never used to satisfy a non-AWDL interface.
    if(set&&request==APPLE80211_IOC_VIRTUAL_IF_CREATE){
        const UInt32 count=__atomic_add_fetch(&wclAwdlCreateRequests,1U,__ATOMIC_RELAXED);
        setProperty("MT7922AWDLCreateRequests",count,32);
        setProperty("MT7922AWDLCreatePayloadPresent",data!=nullptr);
        setProperty("MT7922AWDLCreatePayloadBytes",static_cast<UInt32>(bytes),32);
        if(!data||bytes!=sizeof(mt7922anguscore::awdl::CreateRequest)){
            setProperty("MT7922AWDLCreateResult",static_cast<UInt32>(kIOReturnBadArgument),32);
            setProperty("MT7922AWDLCreateRejected","payload-extent");
            IOLog("MT7922 AWDL WCL create: bytes=%u refused (payload extent)\n",
                  static_cast<unsigned>(bytes));
            return kIOReturnBadArgument;
        }
        mt7922anguscore::awdl::CreateRequest create{};
        memcpy(&create,data,sizeof create);
        char wanted[17]{};memcpy(wanted,create.bsdName,16);
        char address[13];size_t at=0;
        for(unsigned i=0;i<6;++i)
            at+=static_cast<size_t>(snprintf(address+at,sizeof address-at,"%02x",create.mac[i]));
        address[at<sizeof address?at:sizeof address-1]=0;
        setProperty("MT7922AWDLCreateVersion",create.version,32);
        setProperty("MT7922AWDLCreateRole",create.role,32);
        setProperty("MT7922AWDLCreateName",wanted);
        setProperty("MT7922AWDLCreateAddress",address);
        if(create.version!=1||create.role!=6){
            if(create.role!=6)
                __atomic_add_fetch(&wclAwdlCreateRoleRejected,1U,__ATOMIC_RELAXED);
            setProperty("MT7922AWDLCreateRoleRejected",wclAwdlCreateRoleRejected,32);
            setProperty("MT7922AWDLCreateResult",static_cast<UInt32>(kIOReturnUnsupported),32);
            setProperty("MT7922AWDLCreateRejected",create.version!=1?"version":"role");
            if(count<=8||(count&63)==0)
                IOLog("MT7922 AWDL WCL create: role=%u version=%u refused (not AWDL)\n",
                      create.role,create.version);
            return kIOReturnUnsupported;
        }
        // Role 6: create through the same verified factory the diagnostic uses,
        // but with the address the family derived for this interface.
        IOReturn created=kIOReturnSuccess;
        if(!(proximity&&__atomic_load_n(&proximityReady,__ATOMIC_ACQUIRE)))
            created=buildProximity(create.mac);
        if(created!=kIOReturnSuccess){
            __atomic_add_fetch(&wclAwdlCreateFailures,1U,__ATOMIC_RELAXED);
            setProperty("MT7922AWDLCreateFailures",wclAwdlCreateFailures,32);
            setProperty("MT7922AWDLCreateResult",static_cast<UInt32>(created),32);
            setProperty("MT7922AWDLCreateRejected","build-failed");
            setProperty("MT7922AWDLStatus","role6-create-failed");
            IOLog("MT7922 AWDL WCL create: build failed 0x%x\n",static_cast<unsigned>(created));
            return created;
        }
        // Report the interface that actually got registered. The family and
        // airportd match awdl0 by this name/address pair; inventing one would
        // leave them at "role=awdl parent=(null)".
        UInt8 *out=static_cast<UInt8*>(data);
        bool wroteAddress=false,wroteName=false;
        ether_addr actual{};
        if(proximity&&proximity->getHardwareAddress(&actual)==kIOReturnSuccess){
            memcpy(out+4,actual.octet,6);wroteAddress=true;
        }
        if(proximity){
            const char *name=proximity->getBSDName();
            const size_t length=name?strnlen(name,16):0;
            memset(out+16,0,16);
            if(length){memcpy(out+16,name,length<16?length:15);wroteName=true;}
        }
        wclAwdlCreateAddressWritten=wroteAddress;
        wclAwdlCreateNameWritten=wroteName;
        const UInt32 accepted=__atomic_add_fetch(&wclAwdlCreateAccepted,1U,__ATOMIC_RELAXED);
        setProperty("MT7922AWDLCreateAccepted",accepted,32);
        setProperty("MT7922AWDLCreateAddressWritten",wroteAddress);
        setProperty("MT7922AWDLCreateNameWritten",wroteName);
        setProperty("MT7922AWDLCreateResult",static_cast<UInt32>(kIOReturnSuccess),32);
        removeProperty("MT7922AWDLCreateRejected");
        setProperty("MT7922AWDLStatus","role6-created-by-family-request");
        IOLog("MT7922 AWDL WCL create: role=6 accepted name=%s mac=%s requested-name=%s\n",
              wroteName?reinterpret_cast<const char*>(out+16):"<none>",
              wroteAddress?address:"<none>",wanted);
        return kIOReturnSuccess;
    }
    if(set&&request==APPLE80211_IOC_POWERSAVE){
        if(!data)return kIOReturnBadArgument;
        const auto *policy=static_cast<const apple80211_powersave_data*>(data);
        if(policy->version!=APPLE80211_VERSION)return kIOReturnBadArgument;
        // Level zero is the only state implemented by firmware/queue setup.
        // Acknowledge that idempotent request, but never claim doze/U-APSD.
        return policy->powersave_level==0?kIOReturnSuccess:kIOReturnUnsupported;
    }
    if(set&&request==0x1b9)return submitWclScan(data,bytes);
    if(!set&&request==0x1b1)return submitWclBssInfo(data,bytes);
    // WNM configuration and firmware offload are not implemented. Never
    // claim that an unparsed configuration has been applied.
    if((set&&request==0x1eb)||request==0x1ec)return kIOReturnUnsupported;
    if(set&&request==0x1ce){
        // This SPI override supplies bytes=0. The DTO's internal length does
        // not prove the caller's buffer extent; dereferencing it can panic.
        // PMF protection and the category/payload contract also need proof.
        setProperty("MT7922WCLActionTxResult",UInt32(kIOReturnUnsupported),32);
        return data?kIOReturnUnsupported:kIOReturnBadArgument;
    }
#if MT7922_NATIVE_DARWIN == 25
    if(!set&&request==0x23a){
        const auto result=mt7922wcl::encodeHeCapability(data,bytes)?kIOReturnSuccess:kIOReturnBadArgument;
        setProperty("MT7922WCLHeCapabilityResult",static_cast<UInt32>(result),32);
        setProperty("MT7922WCLHeCapabilityBytes",static_cast<UInt32>(bytes),32);
        return result;
    }
    if(!set&&request==0x1cc){
        if(!data||bytes<mt7922wcl::ExtendedBssBytes)return kIOReturnBadArgument;
        mt7922wcl::ExtendedBssSnapshot snapshot;
        UInt8 rsn[0x101]{};size_t rsnBytes=0;
        IOLockLock(bus->dataPathLock);
        const bool linked=bus->networkVisibleLink();
        // scanLegacyRates/networkLegacyRates use the 12-entry Apple rate map
        // for both bands; do not shift 5GHz bits into the reserved nibble.
        snapshot.rates=bus->networkLegacyRates;snapshot.basic=bus->networkBasicRates;
        const auto &phy=bus->networkPhy;
        if(phy.ht)memcpy(snapshot.ht,phy.peer.mcs,sizeof(snapshot.ht));
        snapshot.vht=phy.vht?phy.vhtMcs:0xffff;snapshot.he=phy.he?phy.heMcs:0xffff;
        // getAssociatedWPARSNIESync copies the associated BSS RSN IE, not the
        // client RSN used in M2. The session holds the AP IE from the scan.
        if(bus->wpaSession.apRSNLen&&bus->wpaSession.apRSNLen<=sizeof(rsn)) {
            memcpy(rsn,bus->wpaSession.apRSN,bus->wpaSession.apRSNLen);
            rsnBytes=bus->wpaSession.apRSNLen;
        }
        IOLockUnlock(bus->dataPathLock);
        if(rsnBytes){snapshot.rsn=rsn;snapshot.rsnBytes=rsnBytes;}
        const auto result=!linked?kIOReturnNotReady:
            mt7922wcl::encodeExtendedBss(snapshot,data,bytes)?kIOReturnSuccess:kIOReturnBadArgument;
        setProperty("MT7922WCLExtendedBssResult",static_cast<UInt32>(result),32);
        IOLog("MT7922 WCL EXTENDED BSS: result=0x%x bytes=%u\n",result,(unsigned)bytes);
        return result;
    }
    if(set&&request==0x1f6){
        if(!data||bytes!=2)return kIOReturnBadArgument;
        const auto *flags=static_cast<const UInt8*>(data);
        // Native setCurrentBSS calls this before updateLinkState and aborts
        // the link publication if it fails. This driver has no Fast Lane
        // scheduler/ACM override, so acknowledge the optional hint as a
        // no-op instead of returning unsupported. Do not claim fastlane is
        // active; publish the requested flag for diagnostics only.
        setProperty("MT7922WCLFastLane",static_cast<UInt32>(flags[0]),32);
        return kIOReturnSuccess;
    }
#endif

    if(set&&request==0x1ba){
        const auto profile=MT7922_NATIVE_DARWIN==24?mt7922compat::AssociationProfile::Sequoia15:mt7922compat::AssociationProfile::Tahoe26;
        const auto result=submitWclAssociation(data,bytes,profile);
        setProperty("MT7922WCLJoinSubmitErrno",UInt32(result),32);
        return MT7922WclStatus(result);
    }
    if(set&&request==0x1b3){ // WCL_SCAN_ABORT
        Mt7922zOWControlGuard control(bus->controlLock);if(!control)return kIOReturnBusy;
        return bus->mcuCancelHardwareScan()?kIOReturnSuccess:kIOReturnBusy;
    }
    if(set&&request==0x59){ // WCL_CLEAR_PMKSA_CACHE
        // The driver keeps no PMKSA cache; the PMK is supplied per join.
        // Reporting success lets IO80211 finish its own clear path instead of
        // aborting on kIOReturnUnsupported.
        IOLog("MT7922 WCL PMKSA CLEAR: no driver cache, reporting success\n");
        return kIOReturnSuccess;
    }
    if(set&&request==0x3){
        // WPA2-Enterprise: the system supplicant owns EAPOL and supplies the
        // PTK/GTK. Queue them for the connection worker; never touch MCU rings
        // from this IO80211 request context.
        if(bus->networkEnterpriseActive) {
            const bool accepted=bus->enqueueEnterpriseKey(data,bytes);
            setProperty("MT7922WCLSystemKeyInstallEnabled",accepted);
            KEXT_INFO("MT7922 WCL SET_KEY: enterprise request bytes=%u accepted=%u",
                (unsigned)bytes,accepted?1U:0U);
            return accepted?kIOReturnSuccess:kIOReturnUnsupported;
        }
#if MT7922_NATIVE_DARWIN == 25
        mt7922wclkey::Metadata meta{};
        const auto result=mt7922wclkey::metadata(data,bytes,meta);
        setProperty("MT7922WCLKeyMetadataResult",static_cast<UInt32>(result),32);
        setProperty("MT7922WCLSystemKeyInstallEnabled",false);
        if(result==mt7922wclkey::Result::Valid) {
            setProperty("MT7922WCLKeyCipher",meta.cipher,32);
            setProperty("MT7922WCLKeyBytes",meta.length,32);
            setProperty("MT7922WCLKeyRscBytes",meta.rscLength,32);
            IOLog("MT7922 WCL SET_KEY: bytes=%u cipher=%u keyLen=%u rscLen=%u flags=0x%04x index=%u owner=driver; system-install-disabled\n",
                (unsigned)bytes,meta.cipher,meta.length,meta.rscLength,unsigned(meta.flags),unsigned(meta.index));
        } else {
            removeProperty("MT7922WCLKeyCipher");removeProperty("MT7922WCLKeyBytes");removeProperty("MT7922WCLKeyRscBytes");
        }
#else
        if(data&&bytes>=0x10) {
        const auto *p=static_cast<const UInt8*>(data);
        UInt32 keyLength=0,cipher=0;memcpy(&keyLength,p+4,4);memcpy(&cipher,p+8,4);
        IOLog("MT7922 WCL SET_KEY: bytes=%u cipher=%u keyLen=%u flags=0x%04x index=%u\n",
            (unsigned)bytes,cipher,keyLength,unsigned(p[0xc]|(unsigned(p[0xd])<<8)),unsigned(p[0xe]|(unsigned(p[0xf])<<8)));
        }
#endif
        return kIOReturnUnsupported;
    }
#if MT7922_NATIVE_DARWIN == 24
    if(set&&request==0x1c6){
        if(!data||bytes<16)return kIOReturnBadArgument;
        const auto *p=static_cast<const UInt8*>(data);
        if(p[6]>1||p[7]>1||p[8]>1)return kIOReturnBadArgument;
        UInt32 debounce=0;memcpy(&debounce,p+12,4);
        if(p[6]){
            IOLockLock(bus->dataPathLock);
            const bool linked=bus->networkVisibleLink()&&!memcmp(bus->currentBSSID,p,6);
            IOLockUnlock(bus->dataPathLock);
            if(!linked)return kIOReturnNotReady;
            ether_addr address{};memcpy(address.octet,p,6);
            if(p[8])MT7922WclSetCurrentAp(interface,&address);
            if(p[7])interface->setLinkState(kIO80211NetworkLinkUp,0,false,0);
        }else{
            MT7922WclSetCurrentAp(interface,nullptr);
            interface->setLinkState(kIO80211NetworkLinkDown,0,debounce!=0,debounce);
        }
        return kIOReturnSuccess;
    }
#endif
    if(set&&request==0x1bc){ // WCL_LINK_UP_DONE acknowledges an actual linked port.
        IOLockLock(bus->dataPathLock);const bool linked=bus->networkVisibleLink();IOLockUnlock(bus->dataPathLock);
        return linked?kIOReturnSuccess:kIOReturnNotReady;
    }
    if(set&&request==0x1be){ // WCL_SET_SCAN_HOME_AWAY_TIME
        // The direct scan path already uses the requested dwell/channel plan.
        // BCM accepts this bookkeeping request; rejecting it made WCL log a
        // failure after every scan and could disturb the scan-manager state.
        setProperty("MT7922ScanHomeAwayAccepted",true);
        return kIOReturnSuccess;
    }
    // IO80211's setWCL_LEAVE_NETWORK wrapper uses 0x1a9 (Darwin25
    // protocol slot 0x1270). 0x1b6 is JOIN_ABORT, a separate operation.
    // Both must retire the old hardware session before another AP can join.
    // No private leave DTO fields are dereferenced here: use the established
    // disconnect path and propagate its real result, including Busy.
    if(set&&(request==0x1a9||request==0x1b6||request==APPLE80211_IOC_DISASSOCIATE||request==APPLE80211_IOC_DEAUTH)){
        // The leave DTO is opaque and may legitimately be absent; the driver
        // never dereferences it. Rejecting a null leave left WCL's old join
        // active and the next network stuck in "authenticating".
        setProperty("MT7922WCLLeaveStage","radio-priority");
        IOLog("MT7922 WCL LEAVE: enter request=0x%x\n",request);
        if(!prioritizeInfrastructure("leave")){setProperty("MT7922WCLLeaveStage","radio-busy");return kIOReturnBusy;}
        setProperty("MT7922WCLLeaveStage","hardware-cleanup");
        const bool hadData=data!=nullptr;
        const auto result=bus->userDisconnect(true);
        setProperty("MT7922WCLLeaveStage","hardware-returned");
        // Retire the WCL join even when the hardware cleanup returned Busy /
        // NotReady; otherwise the next begin() sees the old attempt active.
        IOLockLock(bus->dataPathLock);wclJoin.cancel();IOLockUnlock(bus->dataPathLock);
        if(request==0x1a9){
            setProperty("MT7922WCLLeaveResult",static_cast<UInt32>(result),32);
            setProperty("MT7922WCLLeaveHadData",hadData);
            IOLog("MT7922 WCL LEAVE: request=0x%x data=%u result=0x%x\n",
                  request,hadData?1U:0U,static_cast<unsigned>(result));
        }
        if(timer)timer->setTimeoutMS(1);
        return result;
    }
    // These are unchanged scalar/array query prefixes already implemented by
    // the hardware backend. WCL-specific DTOs never enter this legacy helper.
    if(!set&&data){
        if(request==0x1ab){ // WCL_LOW_LATENCY_INFO
            // The native 4-byte DTO is {trafficOngoing:u8, p2pRealtime:u8,
            // desiredChannel:u16}. We have no active low-latency/P2P traffic
            // scheduler; zero accurately reports that none is in progress.
            // Returning Unsupported makes WCLJoinManager abort every join
            // before authentication (observed on macOS 26, selector 427).
            memset(data,0,4);
            return kIOReturnSuccess;
        }
        if(request==APPLE80211_IOC_MAX_NSS_FOR_AP){
            // AppleBCMWLANCore::getMAX_NSS_FOR_AP reads the btc_nss iovar and
            // stores only the u32 value at DTO+4 (bcm.disasm 0x169e22a), so
            // the version word at +0 stays caller-owned. Reporting the peer's
            // negotiated NSS keeps WCL's rate/NSS selection aligned with the
            // association instead of failing the query with ENOTSUP.
            UInt32 nss=2;
            IOLockLock(bus->dataPathLock);
            if(bus->networkVisibleLink()&&bus->networkPhy.nss)nss=bus->networkPhy.nss;
            IOLockUnlock(bus->dataPathLock);
            memcpy(static_cast<UInt8 *>(data)+4,&nss,sizeof(nss));
            setProperty("MT7922WCLMaxNssForAp",nss,32);
            return kIOReturnSuccess;
        }
        if(request==0x1c1){
            const auto result=mt7922wcl::encodeChannelInfo(data,sizeof(mt7922wcl::ChannelInfo),mt7922reg::config());
            return result==mt7922wcl::ChannelsResult::Success?kIOReturnSuccess:result==mt7922wcl::ChannelsResult::Invalid?kIOReturnBadArgument:kIOReturnUnsupported;
        }
        if(request==0x1b||request==0xfe){
            const auto result=mt7922wcl::encodeChannels(data,0x12d8,mt7922reg::config(),request==0xfe);
            return result==mt7922wcl::ChannelsResult::Success?kIOReturnSuccess:result==mt7922wcl::ChannelsResult::Invalid?kIOReturnBadArgument:kIOReturnUnsupported;
        }
        switch(request){
        case APPLE80211_IOC_CHANNEL:case APPLE80211_IOC_OP_MODE:
        case APPLE80211_IOC_TXPOWER:case APPLE80211_IOC_RATE:
        case APPLE80211_IOC_RSSI:case APPLE80211_IOC_NOISE:
        case APPLE80211_IOC_AUTH_TYPE:case APPLE80211_IOC_PHY_MODE:
        case APPLE80211_IOC_POWERSAVE:
        case APPLE80211_IOC_MCS:case APPLE80211_IOC_NSS:
        case APPLE80211_IOC_TX_NSS:
        case APPLE80211_IOC_CHANNELS_INFO:
        case APPLE80211_IOC_SUPPORTED_CHANNELS:
        case APPLE80211_IOC_HW_SUPPORTED_CHANNELS:
            if(request==APPLE80211_IOC_CHANNELS_INFO)
                setProperty("MT7922WCLChannelsInfoQueried",true);
            return MT7922WclStatus(apple80211SkywalkRequest(0xc03069c9U,int(request),interface,data));
        default:break;
        }
    }
    setProperty("MT7922WCLUnsupportedSelector",request,32);
    return kIOReturnUnsupported;
}

SInt32 MT7922AngusSWInterface::wclRequest(int selector,void *data,size_t bytes){return core?core->wclHardwareRequest(this,selector,data,bytes):kIOReturnNotReady;}
