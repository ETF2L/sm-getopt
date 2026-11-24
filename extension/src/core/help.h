/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The help text and the version line of one level of a spec. */
#ifndef GETOPT_CORE_HELP_H
#define GETOPT_CORE_HELP_H

#include "spec.h"

#include <string>
#include <vector>

namespace smgetopt {

/* The help of level, line by line, wrapped to the spec's help width. */
std::vector<std::string> RenderHelp(const Spec &spec, const Level &level);

/* "NAME VERSION". */
std::string RenderVersion(const Spec &spec);

} // namespace smgetopt

#endif // GETOPT_CORE_HELP_H
