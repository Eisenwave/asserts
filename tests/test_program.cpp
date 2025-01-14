template<typename... T> std::string stringf(T... args) {
	size_t length = snprintf(0, 0, args...);
	if(length < 0) abort();
	std::string str(length, 0);
	snprintf(str.data(), length + 1, args...);
	return str;
}

template<typename I>
inline std::string join(I iter, I end, const std::string& delim) {
	std::string str;
	if(std::distance(iter, end) > 0) {
		str += *iter;
		while(++iter != end) {
			str += delim;
			str += *iter;
		}
	}
	return str;
}

template<class C>
inline std::string join(C container, std::string delim) {
	return join(container.begin(), container.end(), delim);
}

template<size_t N>
inline std::string join(const std::string (&container)[N], std::string delim) {
	return join(std::begin(container), std::end(container), delim);
}

// C++20 polyfill
struct source_location {
	const char* const file;
	const char* const function;
	const int line;
	constexpr source_location(
		const char* file = __builtin_FILE(),
		const char* function = __builtin_FUNCTION(),
		int line = __builtin_LINE()
	) : file(file), function(function), line(line) {}
};

struct token_t {
	token_e token_type;
	std::string value;
};

std::string union_regexes(std::initializer_list<std::string> regexes) {
	std::string composite;
	for(const std::string& str : regexes) {
		if(composite != "") composite += "|";
		composite += stringf("(?:%s)", str.c_str());
	}
	return composite;
}

std::regex generate_lexer() { // Should only be called once
	std::string keywords[] = { "alignas", "constinit", "false", "public", "true",
		"alignof", "const_cast", "float", "register", "try", "asm", "continue", "for",
		"reinterpret_cast", "typedef", "auto", "co_await", "friend", "requires", "typeid",
		"bool", "co_return", "goto", "return", "typename", "break", "co_yield", "if",
		"short", "union", "case", "decltype", "inline", "signed", "unsigned", "catch",
		"default", "int", "sizeof", "using", "char", "delete", "long", "static", "virtual",
		"char8_t", "do", "mutable", "static_assert", "void", "char16_t", "double",
		"namespace", "static_cast", "volatile", "char32_t", "dynamic_cast", "new", "struct",
		"wchar_t", "class", "else", "noexcept", "switch", "while", "concept", "enum",
		"nullptr", "template", "const", "explicit", "operator", "this", "consteval",
		"export", "private", "thread_local", "constexpr", "extern", "protected", "throw" };
	std::string punctuators[] = { "{", "}", "[", "]", "(", ")", "<:", ":>", "<%",
		"%>", ";", ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*",
		"/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==",
		"!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--",
		",", "and", "or", "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq",
		"xor_eq", "not_eq" };
	// Sort longest -> shortest (secondarily A->Z)
	const auto cmp = [](const std::string& a, const std::string& b) {
		if(a.length() > b.length()) return true;
		else if(a.length() == b.length()) return a < b;
		else return false;
	};
	std::sort(std::begin(keywords), std::end(keywords), cmp);
	std::sort(std::begin(punctuators), std::end(punctuators), cmp);
	// Escape special characters
	const auto escape = [](const std::string& str) {
		const std::regex special_chars { R"([-[\]{}()*+?.,\^$|#\s])" };
		return std::regex_replace(str, special_chars, "\\$&");
	};
	std::transform(std::begin(keywords), std::end(keywords), std::begin(keywords), escape);
	std::transform(std::begin(punctuators), std::end(punctuators), std::begin(punctuators), escape);
	// regular expressions
	std::string keywords_re    = join(keywords, "|");
	std::string punctuators_re = join(punctuators, "|");
	// literals
	std::string optional_integer_suffix = "(?:[Uu](?:LL?|ll?|Z|z)?|(?:LL?|ll?|Z|z)[Uu]?)?";
	std::string int_binary  = "^0[Bb][01](?:'?[01])*" + optional_integer_suffix + "$";
	// slightly modified so 0 is lexed as a decimal literal instead of octal
	std::string int_octal   = "^0(?:'?[0-7])+" + optional_integer_suffix + "$";
	std::string int_decimal = "^(?:0|[1-9](?:'?\\d)*)" + optional_integer_suffix + "$";
	std::string int_hex	    = "^0[Xx](?!')(?:'?[\\da-fA-F])+" + optional_integer_suffix + "$";

	std::string digit_sequence = "\\d(?:'?\\d)*";
	std::string fractional_constant = stringf("(?:(?:%s)?\\.%s|%s\\.)", digit_sequence.c_str(), digit_sequence.c_str(), digit_sequence.c_str());
	std::string exponent_part = "(?:[Ee][\\+-]?" + digit_sequence + ")";
	std::string suffix = "[FfLl]";
	std::string float_decimal = stringf("^(?:%s%s?|%s%s)%s?$",
								fractional_constant.c_str(), exponent_part.c_str(),
								digit_sequence.c_str(), exponent_part.c_str(), suffix.c_str());
	std::string hex_digit_sequence = "[\\da-fA-F](?:'?[\\da-fA-F])*";
	std::string hex_frac_const = stringf("(?:(?:%s)?\\.%s|%s\\.)", hex_digit_sequence.c_str(), hex_digit_sequence.c_str(), hex_digit_sequence.c_str());
	std::string binary_exp = "[Pp][\\+-]?" + digit_sequence;
	std::string float_hex = stringf("^0[Xx](?:%s|%s)%s%s?$",
						hex_frac_const.c_str(), hex_digit_sequence.c_str(), binary_exp.c_str(), suffix.c_str());
	// char and string
	std::string char_literal = R"((?:u8|[UuL])?'(?:\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.|[^'])+')";
	std::string string_literal = R"((?:u8|[UuL])?"(?:\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.|[^'])+")";
	std::string raw_string_literal = R"((?:u8|[UuL])?R"(?<__raw_delim>[^ ()\\t\r\v\n]*)\((?:(?!\)\k<__raw_delim>).)+\)\k<__raw_delim>")";
	// final rule set
	std::pair<std::string, std::string> rules[] = {
		{ "keyword"    , keywords_re },
		{ "punctuation", punctuators_re },
		{ "literal"    , union_regexes({
			int_binary,
			int_octal,
			int_decimal,
			int_hex,
			float_decimal,
			float_hex,
			char_literal,
			string_literal,
			raw_string_literal,
			"true|false|nullptr"
		}) },
		{ "identifier" , R"((?!\d+)(?:[\da-zA-Z_\$]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8})+)" },
		{ "whitespace" , R"(\s+)" }
	};
	// build the lexer
	std::string lexer;
	for(const auto& pair : rules) {
		if(lexer != "") lexer += "|";
		lexer += stringf("(?<%s>%s)", pair.first.c_str(), pair.second.c_str());
	}
	return std::regex { lexer };
}