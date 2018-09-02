#include "version.hpp"
#include <sstream>
std::string ric::get_version() {
	std::ostringstream s;
	s << LibraryName << " v" << Version_Major << '.' << Version_Minor << '.' << Version_Patch << '(' << Version_Build << ')';
	return s.str();
}

#include "ric.hpp"
namespace ric::Exceptions {
	class InnerCompilationError {
	public:
		std::string error;
		size_t line;
		size_t pos;
		InnerCompilationError(std::string const& error, size_t line, size_t pos)
			: error(error), line(line), pos(pos) {}
	};
}
ric::Exceptions::CompilationError::CompilationError(std::string const& error, size_t line, size_t pos, std::string const& file)
	: error(error), line(line), pos(pos), file(file) {}
ric::Exceptions::CompilationError::CompilationError(InnerCompilationError const& _error, std::string const& file)
	: error(_error.error), line(_error.line), pos(_error.pos), file(file) {}
std::string ric::Exceptions::CompilationError::what() const {
	std::ostringstream s;
	s << "Error compiling " << file << "\n\t\t" 
		<< "at position " << pos << " of line " << line << ":\n    "
		<< error << "\n";
	return s.str();
}

#include <list>
namespace ric {
	enum class TokenType {
		unknown,
		semicolon,
		comma,
		arithmetic,
		block,
		index,
		bracket,
		reserved,
		library,
		identificator,
		number,
		color_literal,
	};
	struct Token {
		TokenType type;
		std::string value;
		size_t line, pos;
		Token(TokenType type, std::string value, size_t line, size_t pos) 
			: type(type), value(value), line(line), pos(pos) {}
		std::string* operator->() { return &value; }
		std::string const* operator->() const { return &value; }
	};
}
#include <set>
const std::set<char> separators {' ', '\t'};
const std::set<char> s_tokens {';', ',', '{', '}', '[', ']', '(', ')', '/', '*', '+', '-', '='};
const std::set<std::string> reserved {
	"palette", "color", "object", "primitive"
};
#include <map>
const std::list<std::tuple<std::string, bool, std::set<std::string>>> library_reserved {
	{ "primitives", false, std::set<std::string>{ "ellipse", "circle", "rectangle", "square" } },
	{ "transformations", false, std::set<std::string>{ "translate", "scale", "rotate" } }
};

//#use parameters.
bool double_precision = false;

namespace ric {
	void use_directive(std::string line, size_t c, size_t p = 0) {
		std::istringstream s(line);
		std::string temp;
		if (s >> temp >> temp >> temp)
			throw Exceptions::InnerCompilationError("Junk information at the end of #use directive.", c, p);

		if (temp == "double_precision")
			double_precision = true;
		else if (temp == "single_precision")
			double_precision = false;
		else
			throw Exceptions::InnerCompilationError("Unknown use directive.", c, p);
	}
	void include_directive(std::list<Token> &tokens, std::string line, size_t c, size_t p = 0) {

	}
	void add_line(std::list<Token> &tokens, bool &is_commentary, std::string line, size_t c, size_t p = 0) {
		if (!is_commentary && line.size() != 0)
			if (line.at(0) == '#') {
				if (line.substr(0, 8) == "#include") {
					include_directive(tokens, line, c, p);
				} else if (line.substr(0, 4) == "#use") {
					use_directive(line, c, p);
				} else if (line.substr(0, 2) == "# ")
					throw Exceptions::InnerCompilationError("There should be no space after '#' in directives.", c, p);
				else
					throw Exceptions::InnerCompilationError("Unknown preprocessor directive: " + line, c, p);
			} else
				tokens.push_back(Token(TokenType::unknown, line, c, p));
	}
	void parse_line(std::list<Token> &tokens, bool &is_commentary, std::string line, size_t c) {
		size_t p_shift = 0;
		for (int i = 0; i < line.size() - 1; i++)
			if (!is_commentary && line.at(i) == '/' && line.at(i + 1) == '*') {
				add_line(tokens, is_commentary, line.substr(0, i), c, p_shift);
				is_commentary = true; i++;
			} else if (is_commentary && line.at(i) == '*' && line.at(i + 1) == '/') {
				line = line.substr(i + 2);
				p_shift = i + 2; i = 0;
				is_commentary = false;
				if (line.size() == 0)
					return;
			} else if (!is_commentary && line.at(i) == '/' && line.at(i + 1) == '/') {
				line = line.substr(0, i);
				if (line.size() == 0)
					return;
			}
		add_line(tokens, is_commentary, line, c, p_shift);
	}

	bool split_tokens(std::list<Token> &tokens, std::list<Token>::iterator &it, int i) {
		for (auto &s : separators)
			if (it->value.at(i) == s) {
				Token temp(TokenType::unknown, it->value.substr(0, i), it->line, it->pos);
				it->value = it->value.substr(i + 1);
				it->pos += i + 1;
				tokens.insert(it, temp);
				return true;
			}
		for (auto &s : s_tokens)
			if (it->value.at(i) == s) {
				Token temp(TokenType::unknown, it->value.substr(0, i), it->line, it->pos);
				Token s_temp(TokenType::unknown, it->value.substr(i, 1), it->line, it->pos + i);
				it->value = it->value.substr(i + 1);
				it->pos += i + 1;
				tokens.insert(it, temp);
				tokens.insert(it, s_temp);
				return true;
			}
		return false;
	}

	bool is_operator(std::list<Token>::iterator &it) {
		if (it->value.size() == 1)
			switch (it->value[0]) {
				case ';': it->type = TokenType::semicolon; return true;
				case ',': it->type = TokenType::comma; return true;
				case '{': case '}': it->type = TokenType::block; return true;
				case '[': case ']': it->type = TokenType::index; return true;
				case '(': case ')': it->type = TokenType::bracket; return true;

				case '+': case '-': case '*': case '/': case '=':
					it->type = TokenType::arithmetic; return true;
			}
		return false;
	}
	bool is_reserved(std::list<Token>::iterator &it) {
		if (reserved.find(it->value) != reserved.end()) {
			it->type = TokenType::reserved;
			return true;
		} else 
			for (auto lib : library_reserved)
				if (std::get<1>(lib))
					if (std::get<2>(lib).find(it->value) != reserved.end()) {
						it->type = TokenType::library;
						return true;
					}
		return false;
	}
	bool is_number(std::string const& s) {
		bool has_point = false;
		if (s.size() == 0 || !(isdigit(s[0]) || s[0] == '+' || s[0] == '-'))
			if (s[0] == '.')
				has_point = true;
			else
				return false;
		for (int i = 1; i < s.size(); i++) {
			if (s[i] == '.')
				if (has_point)
					return false;
				else
					has_point = true;
			else if (!(isdigit(s[i])))
				return false;
		}
		return true;
	}
	bool is_number(std::list<Token>::iterator &it) {
		bool ret = is_number(it->value);
		if (ret) it->type = TokenType::number;
		return ret;
	}
	bool is_identificator(std::string const& s) {
		if (s.size() == 0 || !isalpha(s[0]))
			return false;
		for (int i = 1; i < s.size(); i++)
			if (!(isalnum(s[i]) || s[i] == '_'))
				return false;
		return true;
	}
	bool is_identificator(std::list<Token>::iterator &it) {
		bool ret = is_identificator(it->value);
		if (ret) it->type = TokenType::identificator;
		return ret;
	}
	bool is_color_literal(std::string const& s, size_t line, size_t pos) {
		if (s.size() < 2 || s[0] != '0' || s[1] != 'x')
			return false;
		if (s.size() > 10)
			throw Exceptions::InnerCompilationError("Color literal seems to be broken: " + s + "\n    "
											   "Literal cannot consist of more than 8 digits.", line, pos);
		for (int i = 2; i < s.size(); i++)
			if ((s[i] < '0' || s[i] > '9') && (s[i] < 'A' || s[i] > 'F') && (s[i] < 'a' || s[i] > 'f'))
				throw Exceptions::InnerCompilationError("Color literal seems to be broken: " + s + "\n    '"
												   + s[i] + "' is not a valid hex-digit.", line, pos);
		return true;
	}
	bool is_color_literal(std::list<Token>::iterator &it) {
		bool ret = is_color_literal(it->value, it->line, it->pos);
		if (ret)
			it->type = TokenType::color_literal;
		return ret;
	}

	std::list<Token> tokenize(std::istream &source) {
		std::list<Token> tokens;

		bool is_commentary = false;
		size_t c = 0;
		std::string temp;
		while (std::getline(source, temp)) {
			if (temp.size() != 0)
				parse_line(tokens, is_commentary, temp, c);
			c++;
		}
		if (is_commentary)
			throw Exceptions::InnerCompilationError("Looks like there is an unclosed commentary at the end of the file.", 
											   c, 0);

		for (auto it = tokens.begin(); it != tokens.end(); it++)
			if (it->type == TokenType::unknown)
				for (int i = 0; i < it->value.size(); i++)
					if (split_tokens(tokens, it, i))
						i = -1;

		for (auto it = tokens.begin(); it != tokens.end();) {
			if (it->type == TokenType::unknown)
				if (it->value.size() == 0)
					if (it != tokens.begin()) {
						auto del = it--;
						tokens.erase(del);
					} else {
						tokens.pop_front();
						it = tokens.begin();
						continue;
					}
			it++;
		}

		for (auto it = tokens.begin(); it != tokens.end(); it++)
			if (it->type == TokenType::unknown) {
				if (it->value.size() == 1 && is_operator(it))
					continue;
				else if (is_reserved(it))
					continue;
				else if (is_number(it))
					continue;
				else if (is_identificator(it))
					continue;
				else if (is_color_literal(it))
					continue;
				else
					throw Exceptions::InnerCompilationError("Unknown Token: " + it->value, it->line, it->pos);
			}
		return tokens;
	}
}

#include <fstream>
void ric::compile(std::string const& path) {
	std::ifstream f;
	f.open(path);
	if (!f) {
		throw Exceptions::CompilationError("Unable to open file.", 0, 0, path);
		exit(1);
	}
	try {
		auto tokens = tokenize(f);
	} catch (Exceptions::InnerCompilationError &e) {
		throw Exceptions::CompilationError(e, path);
	}
	return;
}