/* Copyright 2026 ETF2L Contributors, Dave
 *
 * The command line is read level by level first, and the levels it selects
 * are settled afterwards, innermost first, since each result holds the one
 * below it. An automatic option answers as soon as its level is read, so
 * --help wins over a missing required option. Every selected level settles
 * its values before any value is resolved, all are resolved before any is
 * verified, and all are verified before any is prepared: no value is
 * prepared unless every value passed verify. A prepare that fails ends the
 * parse; what the values before it prepared stays arranged. */
#include "parser.h"

#include "errors.h"
#include "help.h"
#include "text.h"

#include <string.h>

#include <deque>

namespace smgetopt {

namespace {

using text::Format;

/* The command line values collected so far, by option. An inherited option
 * is one option on every level, so its words add up across them. */
using GivenValues = std::map<const Option *, Given>;

/* An automatic option was given: the parse ends with its answer. */
struct Answer {
	std::vector<std::string> output;
};

/* One selected level as the command line gave it. */
struct Step {
	const Level *level;
	std::optional<std::string> command;
};

/* A value a parse settled on, by the name it is kept under, with what its
 * type's later steps need: the type and the wording for a problem. */
struct SettledEntry {
	std::string name;
	const Type *type;
	Report report;
};

struct Settlement {
	const Level *level;
	std::optional<std::string> command;
	std::map<std::string, Value> values;
	std::vector<SettledEntry> entries;
};

/* Calls visit on every single value of value, in order. */
template <typename Visit>
void ForEachScalar(Value &value, Visit visit)
{
	if (value.single)
		visit(*value.single);
	for (Scalar &scalar : value.list)
		visit(scalar);
	for (auto &pair : value.mapping)
		visit(pair.second);
	for (Mapping &object : value.objects) {
		for (auto &pair : object)
			visit(pair.second);
	}
}

/* value with every single value in it replaced by what process returns for
 * it, the shape kept. The number a type gave the old text no longer holds;
 * its attributes stay. */
Value Processed(const Value &value, const ProcessValue &process, const ProcessContext &context)
{
	Value processed = value;
	ForEachScalar(processed, [&](Scalar &scalar) {
		scalar = Scalar{process(context, scalar.text), {}, {}, {}, std::move(scalar.attributes)};
	});
	return processed;
}

/* Reads the options of one level off the front of the words, the way
 * Getopt::Long does with bundling, without abbreviations and with case
 * mattering. On a level with commands it stops at the first word that is
 * no option; elsewhere options and other words may mix. */
class OptionReader {
public:
	OptionReader(const Level &level, std::deque<std::string> &words, GivenValues &given)
		: m_level(level), m_words(words), m_given(given)
	{
	}

	/* Leaves the words that are no options in words. Throws UserError with
	 * every problem found on the level. */
	void Read()
	{
		std::deque<std::string> others;
		while (!m_words.empty()) {
			if (!IsOption(m_words.front()) && m_words.front() != "--" && m_level.HasCommands())
				break;

			std::string word = std::move(m_words.front());
			m_words.pop_front();
			if (word == "--")
				break;
			if (IsOption(word))
				ReadOption(word);
			else
				others.push_back(std::move(word));
		}
		m_words.insert(m_words.begin(), others.begin(), others.end());

		if (!m_problems.empty())
			throw UserError(text::Join(m_problems, "; "));
	}

private:
	static bool IsOption(const std::string &word)
	{
		return word.size() > 1 && word[0] == '-' && word != "--";
	}

	void ReadOption(const std::string &word)
	{
		if (word[1] == '-')
			ReadLong(word.substr(2));
		else
			ReadBundle(word.substr(1));
	}

	/* --name, --name=value, --no-name. */
	void ReadLong(const std::string &body)
	{
		/* A name is at least one character, so a leading = belongs to it. */
		size_t equals = body.find('=', 1);
		std::string typed = body.substr(0, equals);
		std::optional<std::string> attached;
		if (equals != std::string::npos)
			attached = body.substr(equals + 1);

		bool negated = false;
		const Option *option = m_level.OptionNamed(typed);
		if (!option) {
			option = NegatedOption(typed);
			negated = option != nullptr;
		}
		if (!option) {
			m_problems.push_back("Unknown option: " + typed);
			return;
		}

		if (!option->type->TakesValue()) {
			if (attached)
				m_problems.push_back(Format("Option %s does not take an argument", typed.c_str()));
			else
				GiveSwitch(*option, negated);
			return;
		}
		if (attached && attached->empty()) {
			m_problems.push_back(Format("Option %s requires an argument", typed.c_str()));
			return;
		}
		if (attached)
			GiveWord(*option, typed, *attached);
		else
			GiveNextWord(*option, typed);
	}

	/* The bool option that --no-NAME or --noNAME turns off. */
	const Option *NegatedOption(const std::string &typed) const
	{
		for (const char *prefix : {"no-", "no"}) {
			if (typed.compare(0, strlen(prefix), prefix) != 0)
				continue;
			const Option *option = m_level.OptionNamed(typed.substr(strlen(prefix)));
			if (option && option->type->Kind() == TypeKind::Bool)
				return option;
		}
		return nullptr;
	}

	/* -abc: single-letter options sharing one dash. The first letter is
	 * read; a letter that takes a value takes the rest of the word, or the
	 * next word. Whatever is left goes back in front of the words as a word
	 * of its own with a dash, so -ab is read as -a -b. */
	void ReadBundle(const std::string &letters)
	{
		std::string letter(text::FirstCharacter(letters));
		std::string rest = letters.substr(letter.size());

		const Option *option = m_level.OptionNamed(letter);
		bool restIsValue = false;
		if (!option)
			m_problems.push_back("Unknown option: " + letter);
		else if (!option->type->TakesValue())
			GiveSwitch(*option, false);
		else if (rest.empty())
			GiveNextWord(*option, letter);
		else
			restIsValue = GiveWord(*option, letter, rest);

		if (!rest.empty() && !restIsValue)
			m_words.push_front("-" + rest);
	}

	void GiveSwitch(const Option &option, bool negated)
	{
		Given &given = m_given[&option];
		switch (option.type->Kind()) {
		case TypeKind::Counter:
			given.count++;
			break;
		case TypeKind::Bool:
			given.count = negated ? 0 : 1;
			break;
		default:
			given.count = 1;
			break;
		}
	}

	/* A value-taking option takes the next word whatever it looks like. */
	void GiveNextWord(const Option &option, const std::string &typed)
	{
		if (m_words.empty()) {
			m_problems.push_back(Format("Option %s requires an argument", typed.c_str()));
			return;
		}
		std::string word = std::move(m_words.front());
		m_words.pop_front();
		GiveWord(option, typed, word);
	}

	/* False when the word is no value for the option: a hash or objectlist
	 * option needs KEY=VALUE. */
	bool GiveWord(const Option &option, const std::string &typed, const std::string &word)
	{
		if (!option.hash && !option.objectlist) {
			Given &given = m_given[&option];
			if (!option.multiple)
				given.words.clear();
			given.words.push_back(word);
			return true;
		}

		size_t equals = word.find('=');
		if (equals == std::string::npos) {
			m_problems.push_back(Format("Option %s, key \"%s\", requires a value", typed.c_str(), word.c_str()));
			return false;
		}
		m_given[&option].pairs.emplace_back(word.substr(0, equals), word.substr(equals + 1));
		return true;
	}

	const Level &m_level;
	std::deque<std::string> &m_words;
	GivenValues &m_given;
	std::vector<std::string> m_problems;
};

class Parser {
public:
	Parser(std::shared_ptr<const Spec> spec, const std::vector<std::string> &words, const ParseContext &context)
		: m_shared(std::move(spec)), m_spec(*m_shared), m_words(words.begin(), words.end()), m_context(context)
	{
	}

	std::shared_ptr<const LevelResult> Parse()
	{
		std::vector<Step> steps = ReadCommandLine();

		std::vector<Settlement> settlements;
		for (auto step = steps.rbegin(); step != steps.rend(); ++step)
			settlements.push_back(InContext(*step->level, [&] { return Settle(*step); }));

		const ParseContext &context = m_context;
		CheckSettled(settlements, [&context](const Type &type, Scalar &scalar) { return type.Resolve(scalar, context); });
		CheckSettled(settlements, [&context](const Type &type, Scalar &scalar) { return type.Verify(scalar, context); });
		CheckSettled(settlements, [&context](const Type &type, Scalar &scalar) { return type.Prepare(scalar, context); });

		std::shared_ptr<const LevelResult> result;
		for (const Settlement &settlement : settlements)
			result = ResultOf(settlement, result);
		return result;
	}

private:
	/* A user error raised by code belongs to level, whose help the user
	 * sees below the message. */
	template <typename Code>
	static auto InContext(const Level &level, Code code) -> decltype(code())
	{
		try {
			return code();
		} catch (UserError &error) {
			if (!error.level)
				error.level = &level;
			throw;
		}
	}

	/* One step per selected level, outermost first. */
	std::vector<Step> ReadCommandLine()
	{
		std::vector<Step> steps;
		for (const Level *level = m_spec.root.get(); level;) {
			Step step = InContext(*level, [&] { return ReadLevel(*level); });
			steps.push_back(step);
			level = step.command ? level->Command(*step.command) : nullptr;
		}
		return steps;
	}

	Step ReadLevel(const Level &level)
	{
		OptionReader(level, m_words, m_given).Read();
		AnswerAutoOptions(level);
		return Step{&level, level.HasCommands() ? SelectCommand(level) : std::nullopt};
	}

	void AnswerAutoOptions(const Level &level) const
	{
		for (const auto &option : level.options) {
			if (!m_given.count(option.get()))
				continue;
			if (option->automatic == AutoOption::Help)
				throw Answer{RenderHelp(m_spec, level)};
			if (option->automatic == AutoOption::Version)
				throw Answer{{RenderVersion(m_spec)}};
		}
	}

	std::optional<std::string> SelectCommand(const Level &level)
	{
		std::string expected = text::Join(level.CommandNames(), ", ");
		if (m_words.empty()) {
			if (!level.commandRequired)
				return std::nullopt;
			throw UserError("missing command, expected one of: " + expected);
		}

		std::string word = std::move(m_words.front());
		m_words.pop_front();
		if (!level.Command(word))
			throw UserError(Format("unknown command '%s', expected one of: %s", word.c_str(), expected.c_str()));
		return word;
	}

	/* The checked values of one selected level. Its options take what the
	 * command line gave them on any level, else their default. */
	Settlement Settle(const Step &step)
	{
		Settlement settlement{step.level, step.command, {}, {}};
		for (const Option *option : step.level->DeclaredOptions())
			SettleOption(*option, settlement);
		if (!step.level->HasCommands())
			SettleArgs(*step.level, settlement);
		return settlement;
	}

	void SettleOption(const Option &option, Settlement &settlement) const
	{
		std::string spelling = option.Spelling();
		auto given = m_given.find(&option);
		if (given != m_given.end()) {
			Report report = [spelling](const std::string &problem) {
				throw UserError(Format("option '%s': %s", spelling.c_str(), problem.c_str()));
			};
			Keep(settlement, option.name, option.FromCommandLine(given->second, report), *option.type, report);
			return;
		}
		if (option.hasDefault) {
			Report report = [spelling](const std::string &problem) {
				throw UserError(Format("option '%s': default value: %s", spelling.c_str(), problem.c_str()));
			};
			Keep(settlement, option.name, option.CurrentDefault(report), *option.type, report);
			return;
		}
		if (option.required)
			throw UserError(Format("missing required option '%s'", spelling.c_str()));
		settlement.values[option.name] = option.EmptyValue();
	}

	static void Keep(Settlement &settlement, const std::string &name, Value value, const Type &type, Report report)
	{
		settlement.entries.push_back({name, &type, std::move(report)});
		settlement.values[name] = std::move(value);
	}

	/* The words left on the innermost level, handed to its args in order. */
	void SettleArgs(const Level &level, Settlement &settlement)
	{
		for (const Arg &arg : level.args) {
			std::string name = arg.name;
			Report report = [name](const std::string &problem) {
				throw UserError(Format("argument <%s>: %s", name.c_str(), problem.c_str()));
			};

			Value value;
			if (arg.multiple) {
				value.shape = Shape::List;
				if (m_words.empty() && arg.required)
					throw UserError(Format("missing required argument <%s>", name.c_str()));
				for (const std::string &word : m_words)
					value.list.push_back(arg.CheckedScalar(word, report));
				m_words.clear();
			} else if (!m_words.empty()) {
				value.single = arg.CheckedScalar(m_words.front(), report);
				m_words.pop_front();
			} else if (arg.required) {
				throw UserError(Format("missing required argument <%s>", name.c_str()));
			}
			Keep(settlement, name, std::move(value), *arg.type, report);
		}

		if (!m_words.empty())
			throw UserError(Format("unexpected extra argument '%s'", m_words.front().c_str()));
	}

	/* Runs check on every single value of every settled value, a problem
	 * worded for where the value came from. check may change the value. */
	template <typename Check>
	static void CheckSettled(std::vector<Settlement> &settlements, Check check)
	{
		for (Settlement &settlement : settlements) {
			InContext(*settlement.level, [&] {
				for (const SettledEntry &entry : settlement.entries) {
					ForEachScalar(settlement.values[entry.name], [&](Scalar &scalar) {
						if (Problem problem = check(*entry.type, scalar))
							entry.report(*problem);
					});
				}
				return 0;
			});
		}
	}

	/* The result of one settled level, holding the result of the level
	 * below it. Every processValue callback sees the values before any was
	 * replaced, so the order they run in does not matter. */
	std::shared_ptr<const LevelResult> ResultOf(const Settlement &settlement, std::shared_ptr<const LevelResult> subcommand) const
	{
		auto unprocessed = std::make_shared<const LevelResult>(settlement.level, settlement.values, settlement.command, subcommand);

		ProcessContext context{m_shared, unprocessed};
		std::map<std::string, Value> processed = settlement.values;
		for (const Option *option : settlement.level->DeclaredOptions()) {
			if (option->processValue)
				processed[option->name] = Processed(settlement.values.at(option->name), option->processValue, context);
		}
		for (const Arg &arg : settlement.level->args) {
			if (arg.processValue)
				processed[arg.name] = Processed(settlement.values.at(arg.name), arg.processValue, context);
		}
		return std::make_shared<const LevelResult>(settlement.level, std::move(processed), settlement.command, std::move(subcommand));
	}

	std::shared_ptr<const Spec> m_shared;
	const Spec &m_spec;
	std::deque<std::string> m_words;
	ParseContext m_context;
	GivenValues m_given;
};

} // namespace

ParseResult Parse(std::shared_ptr<const Spec> spec, const std::vector<std::string> &words, const ParseContext &context)
{
	ParseResult result;
	result.spec = spec;
	try {
		result.values = Parser(spec, words, context).Parse();
	} catch (const Answer &answer) {
		result.outcome = Outcome::Answered;
		result.output = answer.output;
	} catch (const UserError &error) {
		result.outcome = Outcome::Rejected;
		result.error = error.what();
		result.output = {std::string("ERROR: ") + error.what(), ""};
		std::vector<std::string> help = RenderHelp(*spec, error.level ? *error.level : *spec->root);
		result.output.insert(result.output.end(), help.begin(), help.end());
	}
	return result;
}

} // namespace smgetopt
