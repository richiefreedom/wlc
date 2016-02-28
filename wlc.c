/*
 * Simple wavelang translator
 */

#include "lib/mpc/mpc.h"

int parser(char *input)
{
	mpc_parser_t *Integer = mpc_new("integer");
	mpc_parser_t *Float = mpc_new("float");
	mpc_parser_t *Expression = mpc_new("expression");
	mpc_parser_t *Product = mpc_new("product");
	mpc_parser_t *Value = mpc_new("value");
	mpc_parser_t *Maths = mpc_new("maths");
	mpc_parser_t *Variable = mpc_new("variable");
	mpc_parser_t *Array = mpc_new("array");
	mpc_parser_t *Leftside = mpc_new("leftside");
	mpc_parser_t *Assignment = mpc_new("assignment");
	mpc_parser_t *Block = mpc_new("block");
	mpc_parser_t *Varlist = mpc_new("varlist");
	mpc_parser_t *Parlist = mpc_new("parlist");
	mpc_parser_t *System = mpc_new("system");

	mpca_lang(MPCA_LANG_DEFAULT,
			"integer \"integer\" : /[0-9]+/ ;"
			"float \"float\" : /[0-9]*.?[0-9]+/ ;"
			"variable \"variable\" : /[A-Za-z]+[A-Za-z0-9]*/ ;"
			"array : <variable> '[' <expression> ']' ;"
			"expression : <product> (('+' | '-') <product>)* ;"
			"product : <value> (('*' | '/') <value>)* ;"
			"value : <float> | <integer> | <array> | <variable> | '(' <expression> ')' ;"
			"leftside : <array> | <variable> ;"
			"assignment : <leftside> /<-/ <expression> ;"
			"block : /BEGIN\n/ (<assignment> ';')* /END\n/ ;"
			"varlist: /VARIABLES/ (<variable> (','|';'))+\n ;"
			"parlist: /PARAMETERS/ (<variable> (','|';'))+\n ;"
			"system: /^/ /SYSTEM/ <variable> '('<integer>')' <varlist> <block> /$/ ;"
			"maths : /^/ <expression> /$/ ;",
			Integer,
			Float,
			Expression,
			Product,
			Value,
			Maths,
			Variable,
			Array,
			Leftside,
			Assignment,
			Block,
			Varlist,
			Parlist,
			System
		 );

	mpc_result_t result;

	if (mpc_parse("stdin>", input, System, &result)) {
		mpc_ast_print(result.output);
		mpc_ast_delete(result.output);
	} else {
		mpc_err_print(result.error);
		mpc_err_delete(result.error);
	}

	mpc_cleanup(14,
			Integer,
			Float,
			Expression,
			Product,
			Value,
			Maths,
			Variable,
			Array,
			Leftside,
			Assignment,
			Block,
			Varlist,
			Parlist,
			System
		   );
}

int main(int argc, char *argv[])
{
	parser(
		"SYSTEM A3 (6)\n"
		"VARIABLES a, b, c, d;\n"
		"BEGIN\n"
		"a <- a + 3.2;"
		"a <- a + 3;"
		"a <- a + 3;"
		"yarr[4] <- lambda * (x + 1);"
		"END\n"
	);
	return 0;
}
