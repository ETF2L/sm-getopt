/**
 * Copyright 2026 ETF2L Contributors, Dave
 *
 * Another plugin for the tests of test_getopt.sp: it keeps a spec, a result
 * and a type of its own, which the test plugin must not be able to use, and
 * shares the type hexcolor with every plugin's specs.
 */
#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <getopt>

public Plugin myinfo = {
	name = "Getopt tests: another plugin",
	author = "ETF2L Contributors, Dave",
	description = "A spec, a result and a shared type of another plugin",
	version = "1.0.0",
	url = "https://github.com/etf2l/sm-getopt"
};

GetoptSpec g_Spec;
GetoptResult g_Result;
GetoptType g_Type;

public void OnPluginStart() {
	g_Type = Getopt_RegisterType("hexcolor", GetoptKind_Value, "", true)
		.SetLabel("Color")
		.OnCheck(HexColor_Check)
		.OnCoerce(HexColor_Coerce);

	g_Spec = new GetoptSpec("sm_other");
	g_Spec.Option("x", "string").Default("y");
	g_Result = g_Spec.Parse("");
}

public void OnPluginEnd() {
	/* The test plugin reads these three through Runner.GetPluginValue. */
	delete g_Result;
	g_Type.SetLabel("Color");
}

bool HexColor_Check(GetoptKeys keys, const char[] value, char[] problem, int maxlength) {
	bool isHex = strlen(value) == 6;
	for (int i = 0; isHex && value[i] != '\0'; i++) {
		isHex = IsCharNumeric(value[i]) || (CharToLower(value[i]) >= 'a' && CharToLower(value[i]) <= 'f');
	}
	if (!isHex) {
		Format(problem, maxlength, "'%s' is not a hex color", value);
	}
	return isHex;
}

void HexColor_Coerce(GetoptKeys keys, const char[] value, char[] coerced, int maxlength) {
	Format(coerced, maxlength, "#%s", value);
	for (int i = 1; coerced[i] != '\0'; i++) {
		coerced[i] = CharToUpper(coerced[i]);
	}
}
