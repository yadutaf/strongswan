/*
 * Copyright (C) 2013 Martin Willi
 * Copyright (C) 2013 revosec AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/**
 * @defgroup kernel_wfp_compat kernel_wfp_compat
 * @{ @ingroup kernel_wfp
 */

#ifndef KERNEL_WFP_COMPAT_H_
#define KERNEL_WFP_COMPAT_H_

#include <winsock2.h>
#include <windows.h>
#include <ipsectypes.h>

/* MinGW defines CIPHERs incorrectly starting at 0 */
#define IPSEC_CIPHER_TYPE_DES					1
#define IPSEC_CIPHER_TYPE_3DES					2
#define IPSEC_CIPHER_TYPE_AES_128				3
#define IPSEC_CIPHER_TYPE_AES_192				4
#define IPSEC_CIPHER_TYPE_AES_256				5
#define IPSEC_CIPHER_TYPE_MAX					6

#include <fwpmtypes.h>
#include <fwpmu.h>
#undef interface

/* MinGW defines TRANSFORMs incorrectly starting at 0 */
#define IPSEC_TRANSFORM_AH						1
#define IPSEC_TRANSFORM_ESP_AUTH				2
#define IPSEC_TRANSFORM_ESP_CIPHER				3
#define IPSEC_TRANSFORM_ESP_AUTH_AND_CIPHER		4
#define IPSEC_TRANSFORM_ESP_AUTH_FW				5
#define IPSEC_TRANSFORM_TYPE_MAX				6

/* missing in MinGW */
enum {
	FWPM_TUNNEL_FLAG_POINT_TO_POINT = 						(1<<0),
	FWPM_TUNNEL_FLAG_ENABLE_VIRTUAL_IF_TUNNELING =			(1<<1),
};

/* missing in MinGW */
enum {
	IPSEC_SA_DETAILS_UPDATE_TRAFFIC =						(1<<0),
	IPSEC_SA_DETAILS_UPDATE_UDP_ENCAPSULATION =				(1<<1),
	IPSEC_SA_BUNDLE_UPDATE_FLAGS =							(1<<2),
	IPSEC_SA_BUNDLE_UPDATE_NAP_CONTEXT =					(1<<3),
	IPSEC_SA_BUNDLE_UPDATE_KEY_MODULE_STATE =				(1<<4),
	IPSEC_SA_BUNDLE_UPDATE_PEER_V4_PRIVATE_ADDRESS =		(1<<5),
	IPSEC_SA_BUNDLE_UPDATE_MM_SA_ID =						(1<<6),
};

/* missing in MinGW */
enum {
	FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET =					(1<<0),
	FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET =					(1<<1),
	FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET =					(1<<2),
	FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET =					(1<<3),
	FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET =					(1<<4),
	FWPM_NET_EVENT_FLAG_APP_ID_SET =						(1<<5),
	FWPM_NET_EVENT_FLAG_USER_ID_SET =						(1<<6),
	FWPM_NET_EVENT_FLAG_SCOPE_ID_SET =						(1<<7),
	FWPM_NET_EVENT_FLAG_IP_VERSION_SET =					(1<<8),
	FWPM_NET_EVENT_FLAG_REAUTH_REASON_SET =					(1<<9),
};

/* missing in MinGW */
enum {
	FWPM_FILTER_FLAG_PERSISTENT =							(1<<0),
	FWPM_FILTER_FLAG_BOOTTIME =								(1<<1),
	FWPM_FILTER_FLAG_HAS_PROVIDER_CONTEXT =					(1<<2),
	FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT =					(1<<3),
	FWPM_FILTER_FLAG_PERMIT_IF_CALLOUT_UNREGISTERED =		(1<<4),
	FWPM_FILTER_FLAG_DISABLED =								(1<<5),
};

/* missing in MinGW */
enum {
	IPSEC_SA_BUNDLE_FLAG_ND_SECURE =							(1<< 0),
	IPSEC_SA_BUNDLE_FLAG_ND_BOUNDARY =							(1<< 1),
	IPSEC_SA_BUNDLE_FLAG_ND_PEER_NAT_BOUNDARY =					(1<< 2),
	IPSEC_SA_BUNDLE_FLAG_GUARANTEE_ENCRYPTION =					(1<< 3),
	IPSEC_SA_BUNDLE_FLAG_NLB =									(1<< 4),
	IPSEC_SA_BUNDLE_FLAG_NO_MACHINE_LUID_VERIFY =				(1<< 5),
	IPSEC_SA_BUNDLE_FLAG_NO_IMPERSONATION_LUID_VERIFY =			(1<< 6),
	IPSEC_SA_BUNDLE_FLAG_NO_EXPLICIT_CRED_MATCH =				(1<< 7),
	IPSEC_SA_BUNDLE_FLAG_ALLOW_NULL_TARGET_NAME_MATCH =			(1<< 9),
	IPSEC_SA_BUNDLE_FLAG_CLEAR_DF_ON_TUNNEL =					(1<<10),
	IPSEC_SA_BUNDLE_FLAG_ASSUME_UDP_CONTEXT_OUTBOUND =			(1<<11),
	IPSEC_SA_BUNDLE_FLAG_ND_PEER_BOUNDARY =						(1<<12),
	IPSEC_SA_BUNDLE_FLAG_SUPPRESS_DUPLICATE_DELETION =			(1<<13),
	IPSEC_SA_BUNDLE_FLAG_PEER_SUPPORTS_GUARANTEE_ENCRYPTION =	(1<<14),
	IPSEC_SA_BUNDLE_FLAG_FORCE_INBOUND_CONNECTIONS =			(1<<15),
	IPSEC_SA_BUNDLE_FLAG_FORCE_OUTBOUND_CONNECTIONS =			(1<<16),
	IPSEC_SA_BUNDLE_FLAG_FORWARD_PATH_INITIATOR =				(1<<17),
};

DWORD WINAPI FwpmIPsecTunnelAdd0(HANDLE, UINT32,
	const FWPM_PROVIDER_CONTEXT0*, const FWPM_PROVIDER_CONTEXT0*, UINT32,
	const FWPM_FILTER_CONDITION0*, PSECURITY_DESCRIPTOR);

#endif /** KERNEL_WFP_COMPAT_H_ @}*/
