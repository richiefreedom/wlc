/*
 * Simple wavelang translator
 */

#include <stdio.h>
#include <stdlib.h>
#include "lib/mpc/mpc.h"

#define DEBUG

#define SYM_NAME_LEN		128
#define CAT_NAME_LEN		SYM_NAME_LEN
#define SYS_NAME_LEN		SYM_NAME_LEN
#define MAX_SYMS_PER_TABLE	64
#define MAX_EQNS_PER_CAT	16

#ifdef DEBUG
#define DEBUG_PRINT(...) do {fprintf(stderr, __VA_ARGS__ );} while(0)
#else
#define DEBUG_PRINT(...) do {} while (1)
#endif

#define ERROR_PRINT(...) do {fprintf(stderr, "[ERROR] " __VA_ARGS__);} while(0)

enum symbol_type {
	SYM_VAR = 0,
	SYM_VEC,
	SYM_PAR,
	SYM_FUN
};

struct symbol {
	enum symbol_type type;
	char name[SYM_NAME_LEN];
	unsigned int capacity;
};

struct symbol_table {
	struct symbol symbols[MAX_SYMS_PER_TABLE];
	unsigned int num_symbols;
};

int add_symbol(struct symbol_table *table, char *name,
		enum symbol_type type, unsigned int capacity)
{
	unsigned int i = table->num_symbols;

	if (strlen(name) >= SYM_NAME_LEN) {
		ERROR_PRINT("Too long symbol name!\n");
		return -1;
	}

	strcpy(table->symbols[i].name, name);
	table->symbols[i].type = type;
	table->symbols[i].capacity = capacity;

	table->num_symbols++;

	return 0;
}

struct symbol *find_symbol(struct symbol_table *table, char *name)
{
	int i;

	for (i = 0; i < table->num_symbols; i++) {
		if (0 == strcmp(table->symbols[i].name, name))
			return &table->symbols[i];
	}

	return NULL;
}

struct system {
	char name[SYS_NAME_LEN];
	unsigned int num_equations;
	struct symbol_table sym_table;
};

struct catastrophe {
	char name[CAT_NAME_LEN];
	struct symbol_table sym_table;
	struct system systems[MAX_EQNS_PER_CAT];
	unsigned int num_systems;
};

enum parser_state{
	PSTATE_INITIAL = 0,
	PSTATE_CATASTROPHE,
	PSTATE_CATASTROPHE_NAME,
	PSTATE_LEVEL0_DECL,
	PSTATE_LEVEL0_AFTER_DECL,
	PSTATE_LEVEL0_SYSTEM,
	PSTATE_LEVEL0_AFTER_MAIN_BLOCK,
};


struct parse_table_entry;

typedef int (*walk_func_t)(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[]);

struct parse_table_entry {
	char *tag;
	walk_func_t func;
	struct parse_table_entry *(*parse_table[]);
};

struct catastrophe   catastrophe;
enum   parser_state  parser_state   = PSTATE_INITIAL;
struct system       *current_system = NULL;

/*
 * Most of the analyzer's code is data-driven. The following declarations are
 * the set of semantic analysis and generation rules.
 */

extern struct parse_table_entry *parse_leaf_table[];
extern struct parse_table_entry *parse_value_table[];
extern struct parse_table_entry *parse_product_table[];
extern struct parse_table_entry *parse_function_table[];
extern struct parse_table_entry *parse_expression_table[];

int apply_parse_rule(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[], int strict)
{
	int i, ret;

	for (i = 0; parse_table[i]; i++) {
		int match = 0;

		switch (strict) {
		case 1:
			if (ast->tag == strstr(ast->tag, parse_table[i]->tag))
				match = 1;
			break;
		default:
			if (strstr(ast->tag, parse_table[i]->tag))
				match = 1;
		};

		if (match) {
			ret = parse_table[i]->func(ast, sym_table,
					*parse_table[i]->parse_table);
			if (ret)
				return -1;

			return 0;
		}
	}

	ERROR_PRINT("Cannot find suitable rule %s!\n",
		(strict)? "strictly" : "relaxed");
	ERROR_PRINT("for tag: %s in %s.\n",
			ast->tag, ast->tag);

	return -1;
}

int walk_something(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	int i, ret;

	if (strlen(ast->contents)) {
		ret = apply_parse_rule(ast, sym_table, parse_leaf_table, 0);
		if (ret)
			return -1;
	}

	for (i = 0; i < ast->children_num; i++) {
		if (strstr(ast->children[i]->tag, "char")) {
			printf("%s", ast->children[i]->contents);
		} else {
			ret = apply_parse_rule(ast->children[i], sym_table,
				parse_table, 1);
			if (ret)
				return -1;
		}
	}

	return 0;
}

int walk_variable(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	if (strlen(ast->contents)) {
		struct symbol *sym;
		int sym_err = 1;

		if (sym_table) {
			sym = find_symbol(sym_table, ast->contents);
			if (sym)
				sym_err = 0;
		}

		sym = find_symbol(&catastrophe.sym_table, ast->contents);
		if (sym)
			sym_err = 0;

		if (sym_err) {
			ERROR_PRINT("Unknown symbol: %s!\n", ast->contents);
			return -1;
		}

		printf("%s", ast->contents);

		return 0;
	}

	ERROR_PRINT("Cannot walk variable (AST issue)!\n");
	return -1;
}

int walk_array(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	printf("%s[%s]", ast->children[0]->contents,
			ast->children[2]->contents);

	return 0;
}

int walk_ass_regex(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	if (0 == strcmp(ast->contents, "<-")) {
		printf(" = ");
		return 0;
	}
	return 0;
}

char *known_functions[] = {
	"RungeKutta",
	"Vasya",
	NULL
};

int is_known_function(char *name)
{
	int i;

	for (i = 0; known_functions[i]; i++) {
		if (0 == strcmp(known_functions[i], name))
			return 1;
	}

	return 0;
}

int walk_funcname(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	if (strlen(ast->contents)) {
		if (!is_known_function(ast->contents)) {
			ERROR_PRINT("Unknown function!\n");
			return -1;
		}
		printf("%s", ast->contents);
		return 0;
	}

	ERROR_PRINT("Cannot walk function name (AST issue)!\n");
	return -1;
}

int walk_echo(mpc_ast_t *ast, struct symbol_table *sym_table,
		struct parse_table_entry *parse_table[])
{
	printf("%s", ast->contents);

	return 0;
}

struct parse_table_entry parse_value =
	{"value|", walk_something, parse_value_table};

struct parse_table_entry parse_expression =
	{"expression|", walk_something, parse_expression_table};

struct parse_table_entry parse_variable =
	{"variable|", walk_variable, NULL};

struct parse_table_entry parse_leftside_variable =
	{"leftside|variable|", walk_variable, NULL};

struct parse_table_entry parse_value_variable =
	{"value|variable|", walk_variable, NULL};

struct parse_table_entry parse_value_integer =
	{"value|integer|", walk_echo, NULL};

struct parse_table_entry parse_value_float =
	{"value|float|", walk_echo, NULL};

struct parse_table_entry parse_array =
	{"array|", walk_array, NULL};

struct parse_table_entry parse_product =
	{"product|", walk_something, parse_product_table};

struct parse_table_entry parse_function =
	{"function|", walk_something, parse_function_table};

struct parse_table_entry parse_funcname =
	{"funcname|", walk_funcname, NULL};

struct parse_table_entry parse_ass_regex =
	{"regex", walk_ass_regex, NULL};

struct parse_table_entry *parse_value_table[] = {
	&parse_expression,
	NULL
};

struct parse_table_entry *parse_product_table[] = {
	&parse_value_variable,
	&parse_value,
	NULL
};

struct parse_table_entry *parse_function_table[] = {
	&parse_expression,
	&parse_funcname,
	NULL
};

struct parse_table_entry *parse_expression_table[] = {
	&parse_product,
	&parse_function,
	&parse_expression,
	NULL
};

struct parse_table_entry *parse_assignment[] = {
	&parse_leftside_variable,
	&parse_ass_regex,
	&parse_variable,
	&parse_array,
	&parse_expression,
	&parse_product,
	NULL
};

struct parse_table_entry *parse_leaf_table[] = {
	&parse_value_variable,
	&parse_value_integer,
	&parse_value_float,
	NULL
};

int add_system(struct catastrophe *catastrophe, char *name,
		unsigned int num_equations)
{
	unsigned int i = catastrophe->num_systems;

	if (strlen(name) >= SYS_NAME_LEN) {
		ERROR_PRINT("Too long system name!\n");
		return -1;
	}

	strcpy(catastrophe->systems[i].name, name);
	catastrophe->systems[i].num_equations = num_equations;
	current_system = &catastrophe->systems[i];
	catastrophe->num_systems++;

	return 0;
}

int walk_level0_parlist(mpc_ast_t *ast)
{
	int i, ret;

	for (i = 1; i < ast->children_num - 1; i+=2) {
		DEBUG_PRINT("Param found: %s.\n", ast->children[i]->contents);
		ret = add_symbol(&catastrophe.sym_table,
				ast->children[i]->contents, SYM_PAR, 0);
		if (ret)
			return -1;
	}

	return 0;
}

int walk_varlist(mpc_ast_t *ast, struct symbol_table *sym_table)
{
	int i, ret;

	for (i = 1; i < ast->children_num - 1; i+=2) {
		DEBUG_PRINT("Var found: %s.\n", ast->children[i]->contents);
		ret = add_symbol(sym_table,
				ast->children[i]->contents, SYM_VAR, 0);
		if (ret)
			return -1;
	}

	return 0;
}

int walk_level0_varlist(mpc_ast_t *ast)
{
	return walk_varlist(ast, &catastrophe.sym_table);
}

int walk_system_varlist(mpc_ast_t *ast, struct system *system)
{
	return walk_varlist(ast, &system->sym_table);
}

int walk_level0_veclist(mpc_ast_t *ast)
{
	int i, ret;

	for (i = 1; i < ast->children_num - 1; i++) {
		mpc_ast_t *arr = ast->children[i];
		DEBUG_PRINT("Vec found: %s[%s].\n",
				arr->children[0]->contents,
				arr->children[2]->contents);
		ret = add_symbol(&catastrophe.sym_table,
			arr->children[0]->contents, SYM_VEC,
			(unsigned int)atoi(arr->children[2]->contents));
		if (ret)
			return -1;
	}

	return 0;
}

int walk_assignment(mpc_ast_t *ast, struct symbol_table *sym_table)
{
	int ret;

	ret = walk_something(ast, sym_table, parse_assignment);
	if (ret)
		return -1;

	printf(";\n");

	return 0;
}

int walk_block(mpc_ast_t *ast, struct symbol_table *sym_table)
{
	int i, ret;

	printf("{\n");

	for (i = 1; i < ast->children_num - 1; i+=2) {
		printf("\t");
		ret = walk_assignment(ast->children[i], sym_table);
		if (ret)
			return -1;
	}

	printf("}\n");

	return 0;
}

int walk_level0_system(mpc_ast_t *ast)
{
	int i, ret;

	DEBUG_PRINT("System found: %s(%s).\n", ast->children[1]->contents,
			ast->children[3]->contents);

	ret = add_system(&catastrophe, ast->children[1]->contents,
			(unsigned int)atoi(ast->children[3]->contents));
	if (ret)
		return -1;

	i = 5;
	while (0 == strcmp(ast->children[i]->tag, "varlist|>")) {
		ret = walk_system_varlist(ast->children[i], current_system);
		if (ret)
			return -1;
		i++;
	}

	ret = walk_block(ast->children[i], &current_system->sym_table);
	if (ret)
		return -1;

	return 0;
}

int walk_level0_main_block(mpc_ast_t *ast)
{
	int i, ret;

	DEBUG_PRINT("Main block found.\n");

	ret = walk_block(ast, &current_system->sym_table);
	if (ret)
		return -1;

	return 0;
}

int walk_level0(mpc_ast_t *ast)
{
	int ret;

	switch (parser_state) {
	case PSTATE_INITIAL:
		parser_state = PSTATE_CATASTROPHE;
		break;
	case PSTATE_CATASTROPHE:
		parser_state = PSTATE_CATASTROPHE_NAME;
		break;
	case PSTATE_CATASTROPHE_NAME:
		if (!strlen(ast->contents) ||
			ast->tag != strstr(ast->tag, "variable|regex")) {
			ERROR_PRINT("Catastrophe name expected!\n");
			return -1;
		}
		strcpy(catastrophe.name, ast->contents);
		parser_state = PSTATE_LEVEL0_DECL;
		break;
	case PSTATE_LEVEL0_AFTER_DECL:
		if (0 == strcmp(ast->tag, "system|>")) {
			parser_state = PSTATE_LEVEL0_SYSTEM;
			ret = walk_level0_system(ast);
			if (ret)
				return -1;
			break;
		}
	case PSTATE_LEVEL0_DECL:
		if (0 == strcmp(ast->tag, "parlist|>")) {
			ret = walk_level0_parlist(ast);
			if (ret)
				return -1;
			parser_state = PSTATE_LEVEL0_AFTER_DECL;
		} else if (0 == strcmp(ast->tag, "varlist|>")) {
			ret = walk_level0_varlist(ast);
			if (ret)
				return -1;
			parser_state = PSTATE_LEVEL0_AFTER_DECL;
		} else if (0 == strcmp(ast->tag, "veclist|>")) {
			ret = walk_level0_veclist(ast);
			if (ret)
				return -1;
			parser_state = PSTATE_LEVEL0_AFTER_DECL;
		} else {
			ERROR_PRINT("One of declaration lists expected!\n");
			return -1;
		}

		break;
	case PSTATE_LEVEL0_SYSTEM:
		if (0 == strcmp(ast->tag, "system|>")) {
			parser_state = PSTATE_LEVEL0_SYSTEM;
			ret = walk_level0_system(ast);
			if (ret)
				return -1;
			break;
		}
		parser_state = PSTATE_LEVEL0_AFTER_MAIN_BLOCK;
		ret = walk_level0_main_block(ast);
		if (ret)
			return -1;
		break;
	case PSTATE_LEVEL0_AFTER_MAIN_BLOCK:
		printf("Done.\n");
		break;
	default:
		ERROR_PRINT("Unpredictable parser state: %d!\n", parser_state);
		return -1;
	};

	return 0;
}

int walk_catastrophe(mpc_ast_t *ast)
{
	int i, ret;

	if (strlen(ast->contents)) {
		ERROR_PRINT("Top-level non-terminal is expected!\n");
		return -1;
	}

	for (i = 0; i < ast->children_num; i++) {
		ret = walk_level0(ast->children[i]);
		if (ret)
			return -1;
	}

	return 0;
}

int walk_ast(mpc_ast_t *ast)
{
	return walk_catastrophe(ast);
}

int parser(char *input)
{
	mpc_parser_t *Integer = mpc_new("integer");
	mpc_parser_t *Float = mpc_new("float");
	mpc_parser_t *Expression = mpc_new("expression");
	mpc_parser_t *Product = mpc_new("product");
	mpc_parser_t *Value = mpc_new("value");
	mpc_parser_t *Variable = mpc_new("variable");
	mpc_parser_t *Array = mpc_new("array");
	mpc_parser_t *Leftside = mpc_new("leftside");
	mpc_parser_t *Assignment = mpc_new("assignment");
	mpc_parser_t *Block = mpc_new("block");
	mpc_parser_t *Varlist = mpc_new("varlist");
	mpc_parser_t *Parlist = mpc_new("parlist");
	mpc_parser_t *Veclist = mpc_new("veclist");
	mpc_parser_t *System = mpc_new("system");
	mpc_parser_t *Catastrophe = mpc_new("catastrophe");
	mpc_parser_t *Declare = mpc_new("declare");
	mpc_parser_t *Function = mpc_new("function");
	mpc_parser_t *Funcname = mpc_new("funcname");

	mpca_lang(MPCA_LANG_DEFAULT,
			"integer \"integer\" : /[0-9]+/ ;"
			"float \"float\" : /[0-9]*.?[0-9]+/ ;"
			"variable \"variable\" : /[A-Za-z]+[A-Za-z0-9]*/ ;"
			"funcname \"funcname\" : /[A-Za-z]+[A-Za-z0-9]*/ ;"
			"array : <variable> '[' <expression> ']' ;"
			"expression : <product> (('+' | '-') <product>)* ;"
			"product : <value> (('*' | '/') <value>)* ;"
			"value : '(' <expression> ')' | <float> | <integer> | <function> | <array> | <variable> ;"
			"leftside : <array> | <variable> ;"
			"assignment : <leftside> /<-/ <expression> ;"
			"block : /BEGIN/ (<assignment> ';')* /END/ ;"
			"varlist: /VARIABLES/ (<variable> (','|';'))+ ;"
			"parlist: /PARAMETERS/ (<variable> (','|';'))+ ;"
			"veclist: /VECTORS/ (<array> (','|';'))+ ;"
			"system: /SYSTEM/ <variable> '('<integer>')' <varlist> <block> ;"
			"declare: <parlist> | <varlist> | <veclist>;"
			"catastrophe : /^/ /CATASTROPHE/ <variable> (<declare>)+ (<system>)+ <block>'.' /$/ ;"
			"function: <funcname> '(' <expression> (',' <expression>)* ')' ;",
			Integer,
			Float,
			Expression,
			Product,
			Value,
			Catastrophe,
			Variable,
			Array,
			Leftside,
			Assignment,
			Block,
			Varlist,
			Parlist,
			Veclist,
			System,
			Declare,
			Function,
			Funcname
		 );

	mpc_result_t result;

	if (mpc_parse("stdin>", input, Catastrophe, &result)) {
		mpc_ast_print(result.output);
		walk_ast(result.output);
		mpc_ast_delete(result.output);
		printf("\n\n");
	} else {
		mpc_err_print(result.error);
		mpc_err_delete(result.error);
	}

	mpc_cleanup(18,
			Integer,
			Float,
			Expression,
			Product,
			Value,
			Catastrophe,
			Variable,
			Array,
			Leftside,
			Assignment,
			Block,
			Varlist,
			Parlist,
			Veclist,
			System,
			Declare,
			Function,
			Funcname
		   );
}

int main(int argc, char *argv[])
{
	parser(
		"CATASTROPHE A3\n"
		"PARAMETERS\n"
		"l1, l2, l3;\n"
		"VARIABLES\n"
		"gamma1, gamma2;\n"
		"VECTORS\n"
		"Y[6];\n"
		"SYSTEM A3 (6)\n"
		"VARIABLES a, b, x, d;\n"
		"BEGIN\n"
		"a <- a + b * (1 + 3) + RungeKutta(x + 4) + 3.2;"
		"a <- a + 3;"
		"a <- 4;"
		"yarr[4] <- l1 * (x + 1);"
		"END\n"
		"BEGIN\n"
		"END.\n"
	);
	return 0;
}
