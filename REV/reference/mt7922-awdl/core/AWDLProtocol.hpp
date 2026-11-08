// SPDX-License-Identifier: ISC
#pragma once
#include "All.hpp"
// ABI declarations from macOS 26 IO80211Family / BCM; unsupported methods
// are implemented fail-closed by the driver, never as fabricated success.
struct apple80211_awdl_af_sec_payload_request;
struct apple80211_awdl_af_tx_mode;
struct apple80211_awdl_bssid;
struct apple80211_awdl_cap;
struct apple80211_awdl_cca;
struct apple80211_awdl_config_request;
struct apple80211_awdl_dev_stats;
struct apple80211_awdl_device_capabilities;
struct apple80211_awdl_dfsp_config;
struct apple80211_awdl_dfsp_ucsa_config;
struct apple80211_awdl_election_id;
struct apple80211_awdl_election_metric;
struct apple80211_awdl_election_params;
struct apple80211_awdl_election_rssi_thresholds;
struct apple80211_awdl_extension_state_machine_parameter;
struct apple80211_awdl_master_channel;
struct apple80211_awdl_min_rate;
struct apple80211_awdl_oob_request;
struct apple80211_awdl_peer_traffic_registration;
struct apple80211_awdl_presence_mode;
struct apple80211_awdl_psf_dwell_params;
struct apple80211_awdl_secondary_master_channel;
struct apple80211_awdl_sync_channel_sequence;
struct apple80211_awdl_sync_enabled;
struct apple80211_awdl_sync_frame_template;
struct apple80211_awdl_sync_params;
struct apple80211_awdl_sync_state;
struct apple80211_awdl_txcal_period;
struct apple80211_bssid_data;
struct apple80211_channel_data;
struct apple80211_companion_skywalk_if_create;
struct apple80211_encrypt;
struct apple80211_key;
struct apple80211_llw_params;
struct apple80211_p2p_coex_protection;
struct apple80211_peer_cache_control;
struct apple80211_peer_cache_maximum_size;
struct apple80211_rtg_peer_stats_cfg;
struct apple80211_ssid_data;
struct apple80211_state_data;
struct apple80211_tx_rate;
struct awdl_peer_advert_add;
class IO80211AWDLProtocol : public IO80211VirtualInterface {
 OSDeclareAbstractStructors(IO80211AWDLProtocol)
public:
#define MT7922_AWDL_METHOD(name, type) virtual SInt32 name(type*) = 0;
#define MT7922_AWDL_TEMPLATE(name, type) virtual SInt32 name(type*) = 0;
#include "AWDLProtocolMethods.inc"
#undef MT7922_AWDL_TEMPLATE
#undef MT7922_AWDL_METHOD
};
static_assert(sizeof(IO80211AWDLProtocol)==0x130, "macOS 26 AWDL native object size");
