// ==========================================================================
// Regular expressions matching
// ==========================================================================
// Print the matching substring between an input string and and input search pattern
//
//---------------------------------------------------------------------------
// C definitions for SPIM system calls
//---------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define max(a,b) \
	({ __typeof__ (a) _a = (a); \
	   __typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })
#define min(a,b) \
	({ __typeof__ (a) _a = (a); \
	   __typeof__ (b) _b = (b); \
	_a <d _b ? _a : _b; })
// Should we use backtracking?
#define BACKTRACKS 1

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DESCRIPTION:
//		Regular expression parser with support for wildcard ".", parentheses "(abc)de",
//		kleene star "a*", backtracking "a*a", alternation "a|b", empty string matching
//		"(a|)b" and a combination of all of these "((ab)*|d)a(|.)b"
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int read_char() { return getchar(); }
void read_string(char* s, int size) { fgets(s, size, stdin); }

void print_char(int c) { putchar(c); }
void print_string(char* s) { printf("%s", s); }
void print_string_range(char* s, int start, int length) { int i; for (i = 0; i < length; i++) print_char(s[start + i]); }

union REGEX;

//////////////////////////////
//->	TYPES
//////////////////////////////

// matcher_def
//	ARGUMENTS:
//		int start: index in the string to start at
//		char* haystack: string to check against
//		regex* needle: regular expression definition to use
//		int maxlength: maximum length of a match
//	RETURNS:
//		int: length of the match, or -1 if there was no match
typedef int (*matcher_def)(int, char*, union REGEX*, int);
typedef void (*dispose_def)(union REGEX*);

typedef struct CHARACTER_DEF
{
	matcher_def matcher;
	dispose_def dispose;
	char actual_char;
} character;

typedef struct CONCATENATION_DEF
{
	matcher_def matcher;
	dispose_def dispose;
	union REGEX* first;
	union REGEX* second;
} concatenation;

typedef struct KLEENE_STAR_DEF
{
	matcher_def matcher;
	dispose_def dispose;
	union REGEX* inner;
} kleene_star;

typedef struct REGUNION_DEF
{
	matcher_def matcher;
	dispose_def dispose;
	union REGEX* first;
	union REGEX* second;
} regunion;

typedef struct REGEX_SUPERCLASS_DEF
{
	matcher_def matcher;
	dispose_def dispose;
} regex_super;

typedef union REGEX
{
	regex_super sup;
	character chr;
	concatenation cnc;
	kleene_star kls;
	alternation alt;
} regex;

// Because nope.
typedef int bool;

//////////////////////////////
//->	FUNCTIONS
//////////////////////////////

int match_null(int start, char* haystack, regex* needle, int maxlength)
{
	if (maxlength < 0)
		return -1;

	return 0;
}

void dispose_null(regex* r)
{
}

// get_matcher, get_dispose
//	ARGUMENTS:
//		regex* a: regular expression to use
//	RETURNS:
//		matcher_def/dispose_def: the inner function pointer for that type
//	NOTE:
//		This actually casts to regex_super and loads the as a ..._def from that. If
//		you created a regex type that did not have function pointers in the expected places
//		this would output garbage. To paraphrase 10cc, the things we do for object orientation.
matcher_def get_matcher(regex* a)
{
	if (a == NULL)
		return &match_null;

	return (a->sup).matcher;
}

dispose_def get_dispose(regex* a)
{
	if (a == NULL)
		return &dispose_null;

	return (a->sup).dispose;
}

// Function for readability: returns true if the "result" (the length of a match) represents "no match"
bool has_match(int result)
{
	return result >= 0;
}

// Function for readability: returns true if the given character represents the end of a string
bool is_end(char c)
{
	return c == '\0' || c == '\n';
}

// Simply matches a regex character to a char literal in the input string
int match_char(int start, char* haystack, regex* needle, int maxlength)
{
	if (maxlength < 1)
		return -1;

	// Get the underlying typed regular expression
	character* actual_needle = &(needle->chr);

	bool matches = !is_end(haystack[start]) &&
		(actual_needle->actual_char == '\0' ||
			(actual_needle->actual_char) == haystack[start]);

	return matches ? 1 : -1;
}

// Natively supports arbitrarily nested statements
int match_concatenation(int start, char* haystack, regex* needle, int maxlength)
{
	// Get the underlying typed regular expression
	concatenation* actual_needle = &(needle->cnc);

	matcher_def first_matcher = get_matcher(actual_needle->first);		// Get matcher function for left operand
	matcher_def second_matcher = get_matcher(actual_needle->second);	// Get matcher function for right operand

	int sl = -1;
	int tryl = maxlength + 1;

	do
	{
		tryl = first_matcher(start, haystack, actual_needle->first, tryl - 1);

		if (!has_match(tryl))
			break;

		sl = second_matcher(start + tryl, haystack, actual_needle->second, maxlength - tryl);
	}
	while (has_match(tryl) && !has_match(sl));

	if (!has_match(sl))
		return -1;

	return tryl + sl;
}

// Matches the inner regex 0 or more times.
int match_kleene(int start, char* haystack, regex* needle, int maxlength)
{
	// If we don't have enough space left to match, return no match
	if (maxlength < 0)
		return -1;

	// Get the underlying typed regular expression
	kleene_star* actual_needle = &(needle->kls);

	matcher_def inner_matcher = get_matcher(actual_needle->inner);		// Get matcher function for inner regex

	int l = 0;
	int tot = 0;

	do
	{
		l = inner_matcher(start + tot, haystack, actual_needle->inner, maxlength - tot);

		if (l > 0)
			tot += l;
	}
	while (has_match(l));

	return tot;
}

int match_alternation(int start, char* haystack, regex* needle, int maxlength)
{
	// Get the underlying typed regular expression
	alternation* actual_needle = &(needle->alt);

	matcher_def first_matcher = get_matcher(actual_needle->first);		// Get matcher function for left operand
	matcher_def second_matcher = get_matcher(actual_needle->second);	// Get matcher function for right operand

	int flen = first_matcher(start, haystack, actual_needle->first, maxlength),
		slen = second_matcher(start, haystack, actual_needle->second, maxlength);

	if (flen > slen)
		return flen;

	return slen;
}

int get_match(int start, char* haystack, regex* needle)
{
	matcher_def matcher = get_matcher(needle);		// Get matcher function for inner regex

	int max = 65535; // Max length of match -- arbitrarily chosen value

	return matcher(start, haystack, needle, max);
}

void dispose_char(regex* needle)
{
	free(needle);
}

void dispose_concatenation(regex* needle)
{
	concatenation* actual_needle = &(needle->cnc);

	dispose_def first_dispose = get_dispose(actual_needle->first),	// Get matcher function for left operand
		second_dispose = get_dispose(actual_needle->second);		// Get matcher function for right operand

	first_dispose(actual_needle->first);
	second_dispose(actual_needle->second);
	free(needle);
}

void dispose_kleene(regex* needle)
{
	kleene_star* actual_needle = &(needle->kls);

	dispose_def inner_dispose = get_dispose(actual_needle->inner);	// Get matcher function for inner regex
	inner_dispose(actual_needle->inner);
	free(needle);
}

void dispose_alternation(regex* needle)
{
	alternation* actual_needle = &(needle->alt);

	dispose_def first_dispose = get_dispose(actual_needle->first),	// Get matcher function for left operand
		second_dispose = get_dispose(actual_needle->second);		// Get matcher function for right operand

	first_dispose(actual_needle->first);
	second_dispose(actual_needle->second);
	free(needle);
}

regex character_factory(char chr)
{
	character c =
		{
			&match_char,
			&dispose_char,
			chr
		};

	// If only making the parser was this easy
	return (regex)c;
}

regex concatenation_factory(regex* left, regex* right)
{
	concatenation conc =
		{
			&match_concatenation,
			&dispose_concatenation,
			left,
			right
		};

	return (regex)conc;
}

regex kleene_star_factory(regex* inner)
{
	kleene_star kln =
		{
			&match_kleene,
			&dispose_kleene,
			inner
		};

	return (regex)kln;
}

regex alternation_factory(regex* left, regex* right)
{
	alternation alt =
		{
			&match_alternation,
			&dispose_alternation,
			left,
			right
		};

	return (regex)alt;
}

regex* build_regex(regex* last, char* pattern, int* i, bool escape)
{
	if (is_end(pattern[*i]))
		return last;

	char cur = pattern[*i];
	if (!escape)
	{
		if (cur == '.')
			cur = '\0';

		if (cur == '\\')
		{
			*i = *i + 1;
			return build_regex(last, pattern, i, 1);
		}

		if (cur == '|')
		{
			regex* a = malloc(sizeof(regex));

			*i = *i + 1;
			*a = alternation_factory(last, build_regex(NULL, pattern, i, 0));

			return build_regex(a, pattern, i, 0);
		}
	}

	regex* out;
	if (cur == ')')
	{
		return last;
	}
	else if (cur == '(')
	{
		*i = *i + 1;
		out = build_regex(NULL, pattern, i, 0);
		cur = pattern[*i];
	}
	else
	{
		out = malloc(sizeof(regex));
		*out = character_factory(cur);
	}

	// Wish there was a more elegant way to do this
	if (pattern[*i + 1] == '*')
	{
		regex* k = malloc(sizeof(regex));
		*k = kleene_star_factory(out);

		regex* c;
		if (last == NULL)
		{
			c = k;
		}
		else
		{
			c = malloc(sizeof(regex));
			*c = concatenation_factory(last, k);
		}

		*i = *i + 2;
		return build_regex(c, pattern, i, 0);
	}

	if (last == NULL)
	{
		*i = *i + 1;
		return build_regex(out, pattern, i, 0);
	}

	regex* c = malloc(sizeof(regex));
	*c = concatenation_factory(last, out);

	*i = *i + 1;
	return build_regex(c, pattern, i, 0);
}

int main (int argc, char** argv)
{
	char input_string[100], search_pattern[50], matching_substring[100];
	int i=0;
	char current_char = 0;

	print_string("input: ");

	do
	{
		current_char=read_char();
		input_string[i]=current_char;
		i++;
	}
	while (current_char != '\n');

	while(1)
	{
		i=0;

		print_char('\n');

		print_string("search pattern: ");

		do
		{
			current_char=read_char();
			search_pattern[i]=current_char;
			i++;
		}
		while (current_char != '\n');

		int i = 0;
		regex* pattern = build_regex(NULL, search_pattern, &i, 0);

		int len = 0,
			cur = 0;

		print_string("output: ");

		bool found_match = 0;
		while (!is_end(input_string[cur]))
		{
			len = get_match(cur, input_string, pattern);

			if (len > 0)
			{
				print_string_range(input_string, cur, len);
				print_char('#');
				found_match = 1;
			}

			cur += max(1, len);
		}

		if (!found_match)
			print_string("No matches found.");

		dispose_def dispose = get_dispose(pattern);
		dispose(pattern);

		print_char('\n');
	}

	return 0;
}

//---------------------------------------------------------------------------
// End of file
//---------------------------------------------------------------------------
