/* Copyright 2026 ETF2L Contributors, Dave */
#include "help.h"

#include "text.h"

#include <algorithm>
#include <map>

namespace smgetopt {

namespace {

/* The columns left and right of the widest label. */
const size_t kLabelIndent = 3;
const size_t kLabelGap = 5;

/* How far the Valid and Default lines stand in from the help text. */
const size_t kSubLineIndent = 4;

std::vector<std::string> OptionSpellings(const Option &option)
{
	std::vector<std::string> names = option.Names();
	std::vector<std::string> spellings;
	if (option.type->Kind() != TypeKind::Bool) {
		for (const std::string &name : names)
			spellings.push_back(text::OptionSpelling(name));
		return spellings;
	}

	/* A negatable option shows its negation on the first long name, or after
	 * the names when all of them are single letters. */
	auto negated = std::find_if(names.begin(), names.end(), [](const std::string &name) { return name.size() > 1; });
	for (auto name = names.begin(); name != names.end(); ++name)
		spellings.push_back(name == negated ? "--[no-]" + *name : text::OptionSpelling(*name));
	if (negated == names.end())
		spellings.push_back("--no-" + option.name);
	return spellings;
}

std::string OptionLabel(const Option &option)
{
	std::string label = text::Join(OptionSpellings(option), ", ");
	if (option.hash)
		return label + " <key=value>";
	if (option.objectlist)
		return label + " <N.key=value>";
	if (option.csv)
		return label + " <a,b,...>";
	if (option.type->TakesValue())
		return label + " <>";
	return label;
}

std::string ArgLabel(const Arg &arg)
{
	return "<" + arg.name + ">";
}

std::vector<const Option *> VisibleOptions(const Level &level)
{
	std::vector<const Option *> visible;
	for (const Option *option : level.AllOptions()) {
		if (!option->hidden)
			visible.push_back(option);
	}
	return visible;
}

/* Renders one level; the label column is as wide as its widest label. */
class Helper {
public:
	Helper(const Spec &spec, const Level &level)
		: m_spec(spec), m_level(level), m_options(VisibleOptions(level)), m_labelWidth(LabelWidth())
	{
	}

	std::vector<std::string> Render() const
	{
		std::vector<std::vector<std::string>> blocks;
		blocks.push_back(HeaderBlock());
		if (!m_level.args.empty())
			blocks.push_back(ArgsBlock());
		for (std::vector<std::string> &block : OptionBlocks())
			blocks.push_back(std::move(block));
		if (m_level.HasCommands())
			blocks.push_back(CommandsBlock());
		if (!m_level.examples.empty())
			blocks.push_back(ExamplesBlock());

		std::vector<std::string> lines;
		for (const std::vector<std::string> &block : blocks) {
			if (!lines.empty())
				lines.push_back("");
			lines.insert(lines.end(), block.begin(), block.end());
		}
		return lines;
	}

private:
	size_t LabelWidth() const
	{
		size_t widest = 0;
		for (const Option *option : m_options)
			widest = std::max(widest, text::Width(OptionLabel(*option)));
		for (const Arg &arg : m_level.args)
			widest = std::max(widest, text::Width(ArgLabel(arg)));
		for (const std::string &name : m_level.CommandNames())
			widest = std::max(widest, text::Width(name));
		return kLabelIndent + widest + kLabelGap;
	}

	/* The program name and the command path, as the start of a command
	 * line. */
	std::vector<std::string> Invocation() const
	{
		std::vector<std::string> tokens{m_spec.programName};
		tokens.insert(tokens.end(), m_level.pathWords.begin(), m_level.pathWords.end());
		return tokens;
	}

	std::vector<std::string> HeaderBlock() const
	{
		std::vector<std::string> tokens = Invocation();
		if (!m_options.empty())
			tokens.push_back("[options]");
		if (m_level.HasCommands())
			tokens.push_back("<command>");
		for (const Arg &arg : m_level.args) {
			std::string token = arg.name + (arg.multiple ? "..." : "");
			tokens.push_back(arg.required ? token : "[" + token + "]");
		}

		std::vector<std::string> block{"# " + text::Join(tokens, " ")};
		if (!m_level.description.empty())
			block.push_back("# " + m_level.description);
		return block;
	}

	/* The text next to a label: the required mark, the type's notes, the
	 * help text and the type label. */
	static std::string EntryText(bool required, const Type &type, const std::string &help, const std::string &typeLabel)
	{
		std::vector<std::string> parts;
		if (required)
			parts.push_back("[REQ]");
		for (const std::string &note : type.ConstraintNotes())
			parts.push_back("[" + note + "]");
		if (!help.empty())
			parts.push_back(help);
		if (!typeLabel.empty())
			parts.push_back("[" + typeLabel + "]");
		return text::Join(parts, " ");
	}

	std::vector<std::string> ArgsBlock() const
	{
		std::vector<std::string> block{"## Arguments"};
		for (const Arg &arg : m_level.args)
			AppendEntry(block, ArgLabel(arg), EntryText(arg.required, *arg.type, arg.help, arg.TypeLabel()));
		return block;
	}

	std::vector<std::vector<std::string>> OptionBlocks() const
	{
		std::map<std::string, std::vector<const Option *>> grouped;
		for (const Option *option : m_options)
			grouped[option->group].push_back(option);

		std::vector<std::vector<std::string>> blocks;
		std::string subIndent(m_labelWidth + kSubLineIndent, ' ');
		for (const auto &group : grouped) {
			std::vector<std::string> block{"## " + group.first};
			for (const Option *option : group.second) {
				AppendEntry(block, OptionLabel(*option),
				            EntryText(option->required, *option->type, option->help, option->TypeLabel()));
				if (option->hasValidList)
					block.push_back(subIndent + "Valid   = [ " + text::Join(option->validList, ", ") + " ]");
				if (option->hasDefault)
					block.push_back(subIndent + "Default = " + option->presentedDefault);
			}
			blocks.push_back(std::move(block));
		}
		return blocks;
	}

	std::vector<std::string> CommandsBlock() const
	{
		std::vector<std::string> block{"## Commands"};
		for (const auto &command : m_level.commands)
			AppendEntry(block, command.first, command.second->description);
		return block;
	}

	std::vector<std::string> ExamplesBlock() const
	{
		std::vector<std::string> block{"# Examples:"};
		std::string invocation = text::Join(Invocation(), " ");
		for (size_t i = 0; i < m_level.examples.size(); i++) {
			if (i > 0)
				block.push_back("");
			block.push_back("## " + m_level.examples[i].text);
			block.push_back("##   " + invocation + " " + m_level.examples[i].args);
		}
		return block;
	}

	/* The label in its column and the text wrapped next to it, continuation
	 * lines under the text. A word longer than a line stays whole. */
	void AppendEntry(std::vector<std::string> &block, const std::string &label, const std::string &entryText) const
	{
		std::string line = std::string(kLabelIndent, ' ') + label;
		std::vector<std::string> words = text::SplitAtWhitespace(entryText);
		if (words.empty()) {
			block.push_back(line);
			return;
		}

		/* A line ends one column before the width, and the text column always
		 * has room for a word. */
		size_t lineLimit = std::max<size_t>(m_spec.helpWidth, m_labelWidth + 2) - 1;
		size_t width = text::Width(line);
		line.append(m_labelWidth > width ? m_labelWidth - width : 0, ' ');
		width = std::max(width, m_labelWidth);

		bool lineHasText = false;
		for (const std::string &word : words) {
			size_t wordWidth = text::Width(word);
			if (lineHasText && width + 1 + wordWidth > lineLimit) {
				block.push_back(line);
				line = std::string(m_labelWidth, ' ');
				width = m_labelWidth;
				lineHasText = false;
			}
			if (lineHasText) {
				line += ' ';
				width++;
			}
			line += word;
			width += wordWidth;
			lineHasText = true;
		}
		block.push_back(line);
	}

	const Spec &m_spec;
	const Level &m_level;
	std::vector<const Option *> m_options;
	size_t m_labelWidth;
};

} // namespace

std::vector<std::string> RenderHelp(const Spec &spec, const Level &level)
{
	return Helper(spec, level).Render();
}

std::string RenderVersion(const Spec &spec)
{
	return spec.programName + " " + spec.version;
}

} // namespace smgetopt
