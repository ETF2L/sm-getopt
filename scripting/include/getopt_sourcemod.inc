/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * The option types of the getopt-sourcemod-types plugin, for every
 * plugin's specs: map, cvar (also convar), command, player, target, key,
 * plugin and config. Including this file makes a plugin require that
 * plugin, so it does not load without it. Define GETOPT_SOURCEMOD_OPTIONAL
 * before the include to do without.
 *
 * SourceMod starts plugins in the order it loads them, so a spec that names
 * these types is best checked in OnAllPluginsLoaded rather than in
 * OnPluginStart:
 *
 *     public void OnAllPluginsLoaded()
 *     {
 *         g_Spec.Validate();
 *     }
 *
 * The docs of the getopt extension describe the types in full.
 */

#if defined _getopt_sourcemod_included
#endinput
#endif
#define _getopt_sourcemod_included

#include <getopt>

/**
 * The players a target option matched, read from its attributes.
 *
 * @param result       A parsed result.
 * @param name         The target option or arg.
 * @param clients      Receives the client indexes.
 * @param maxClients   The size of clients.
 * @param targetName   Receives the name of the target, for ShowActivity2
 *                     and ReplyToCommand: a phrase when isPhrase is true
 *                     (use %t), else a player's name (use %s).
 * @param maxlength    The size of targetName.
 * @param isPhrase     Whether targetName is a phrase.
 * @return             The number of clients; 0 when the option has no value.
 */
stock int Getopt_GetTargets(GetoptResult result, const char[] name, int[] clients, int maxClients, char[] targetName,
                            int maxlength, bool &isPhrase) {
	targetName[0] = '\0';
	isPhrase = false;
	char list[MAXPLAYERS * 4];
	if (!result.GetAttribute(name, "clients", list, sizeof(list))) {
		return 0;
	}

	result.GetAttribute(name, "name", targetName, maxlength);
	char flag[4];
	result.GetAttribute(name, "ml", flag, sizeof(flag));
	isPhrase = StrEqual(flag, "1");

	char parts[MAXPLAYERS][4];
	int found = ExplodeString(list, " ", parts, sizeof(parts), sizeof(parts[]));
	int count = found < maxClients ? found : maxClients;
	for (int i = 0; i < count; i++) {
		clients[i] = StringToInt(parts[i]);
	}
	return count;
}

/**
 * The plugin a plugin option names.
 *
 * @return             The plugin's handle, or null when the option has no
 *                     value or the plugin has unloaded since the parse.
 */
stock Handle Getopt_GetPlugin(GetoptResult result, const char[] name) {
	char file[PLATFORM_MAX_PATH];
	if (!result.GetString(name, file, sizeof(file))) {
		return null;
	}
	return FindPluginByFile(file);
}

public SharedPlugin __pl_getopt_sourcemod_types = {
	name = "getopt-sourcemod-types",
	file = "getopt-sourcemod-types.smx",
#if defined GETOPT_SOURCEMOD_OPTIONAL
	required = 0,
#else
	required = 1,
#endif
};
