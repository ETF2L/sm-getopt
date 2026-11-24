/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The extension's description for SourceMod's smsdk_ext. */
#ifndef _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_

#define SMEXT_CONF_NAME        "Getopt"
#define SMEXT_CONF_DESCRIPTION "Declarative parsing of console command arguments for plugins"
#define SMEXT_CONF_VERSION     "1.0.0"
#define SMEXT_CONF_AUTHOR      "ETF2L Contributors, Dave"
#define SMEXT_CONF_URL         "https://github.com/etf2l/sm-getopt"
#define SMEXT_CONF_LOGTAG      "GETOPT"
#define SMEXT_CONF_LICENSE     "GPL"
#define SMEXT_CONF_DATESTRING  __DATE__

#define SMEXT_LINK(name) SDKExtension *g_pExtensionIface = name;

#define SMEXT_ENABLE_HANDLESYS
#define SMEXT_ENABLE_PLUGINSYS
#define SMEXT_ENABLE_PLAYERHELPERS

#endif // _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_
