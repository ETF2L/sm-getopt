/* Copyright 2026 ETF2L Contributors, Dave
 *
 * Parses the words of a command line against a spec. */
#ifndef GETOPT_CORE_PARSER_H
#define GETOPT_CORE_PARSER_H

#include "result.h"
#include "spec.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace smgetopt {

enum class Outcome {
	Parsed,    // the command line is valid; the values are there
	Rejected,  // a mistake on the command line; the output says which
	Answered,  // an automatic option (--help, --version); the output is its answer
};

struct ParseResult {
	/* What the parse was made with; the levels of the values point into it. */
	std::shared_ptr<const Spec> spec;

	Outcome outcome = Outcome::Parsed;

	/* The values of the top level, when parsed. */
	std::shared_ptr<const LevelResult> values;

	/* The mistake, when rejected. */
	std::string error;

	/* What the user is to be shown, line by line: the error and the help of
	 * the level it belongs to, or the answer of an automatic option. Empty
	 * when parsed. */
	std::vector<std::string> output;
};

/* Never throws for a mistake on the command line. A callback of the spec
 * that fails throws CallbackError. context goes to the steps of the types
 * that run during the parse. */
ParseResult Parse(std::shared_ptr<const Spec> spec, const std::vector<std::string> &words, const ParseContext &context = {});

} // namespace smgetopt

#endif // GETOPT_CORE_PARSER_H
