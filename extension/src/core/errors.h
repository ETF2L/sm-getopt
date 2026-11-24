/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The four ways the library fails. A spec error is a mistake in the
 * declaration, that is, in the plugin. A user error is a mistake on the
 * command line; the parser turns it into a rejected parse. A callback error
 * means a plugin callback could not be run. A read error is a mistake in
 * how a result is read, in the plugin again. */
#ifndef GETOPT_CORE_ERRORS_H
#define GETOPT_CORE_ERRORS_H

#include <stdexcept>
#include <string>

namespace smgetopt {

class Level;

class SpecError : public std::runtime_error {
public:
	explicit SpecError(const std::string &message) : std::runtime_error(message) {}
};

class UserError : public std::runtime_error {
public:
	explicit UserError(const std::string &message) : std::runtime_error(message) {}

	/* The level whose help the user sees below the message. */
	const Level *level = nullptr;
};

class CallbackError : public std::runtime_error {
public:
	explicit CallbackError(const std::string &message) : std::runtime_error(message) {}
};

class ReadError : public std::runtime_error {
public:
	explicit ReadError(const std::string &message) : std::runtime_error(message) {}
};

} // namespace smgetopt

#endif // GETOPT_CORE_ERRORS_H
