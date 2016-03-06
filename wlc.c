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

	mpca_lang(MPCA_LANG_DEFAULT,
			"integer \"integer\" : /[0-9]+/ ;"
			"float \"float\" : /[0-9]*.?[0-9]+/ ;"
			"variable \"variable\" : /[A-Za-z]+[A-Za-z0-9]*/ ;"
			"array : <variable> '[' <expression> ']' ;"
			"expression : <product> (('+' | '-') <product>)* ;"
			"product : <value> (('*' | '/') <value>)* ;"
			"value : <float> | <integer> | <function> | <array> | <variable> | '(' <expression> ')' ;"
			"leftside : <array> | <variable> ;"
			"assignment : <leftside> /<-/ <expression> ;"
			"block : /BEGIN/ (<assignment> ';')* /END/ ;"
			"varlist: /VARIABLES/ (<variable> (','|';'))+ ;"
			"parlist: /PARAMETERS/ (<variable> (','|';'))+ ;"
			"veclist: /VECTORS/ (<array> (','|';'))+ ;"
			"system: /SYSTEM/ <variable> '('<integer>')' <varlist> <block> ;"
			"declare: <parlist> | <varlist> | <veclist>;"
			"catastrophe : /^/ /CATASTROPHE/ <variable> (<declare>)+ (<system>)+ <block>'.' /$/ ;"
			"function: <variable> '(' <expression> (',' <expression>)* ')' ;",
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
			Function
		 );

	mpc_result_t result;

	if (mpc_parse("stdin>", input, Catastrophe, &result)) {
		mpc_ast_print(result.output);
		mpc_ast_delete(result.output);
	} else {
		mpc_err_print(result.error);
		mpc_err_delete(result.error);
	}

	mpc_cleanup(17,
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
			Function
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
		"VARIABLES a, b, c, d;\n"
		"BEGIN\n"
		"a <- a + vasya(x + 4) + 3.2;"
		"a <- a + 3;"
		"a <- 4;"
		"yarr[4] <- lambda * (x + 1);"
		"END\n"
		"BEGIN\n"
		"END.\n"
	);
	return 0;
}
