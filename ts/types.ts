//
// general
//

export type SourceLocation = {
	line: number,
	startColumn: number,
	endColumn: number,
}

//
// tokens
//

export enum TokenType {
	number,
	string,
	word,
	
	separator,
	operator,
	
	builtinIndicator,
	selfReference,
}

export type Token = {
	type: TokenType,
	text: string,
	location: SourceLocation,
}

//
// AST
//

type genericASTnode = {
	location: SourceLocation,
}

export type ASTtype = {
	//
	// literals
	//
	
	"bool": genericASTnode & {
		type: "bool",
		value: boolean,
	},
	"number": genericASTnode & {
		type: "number",
		value: number,
	},
	"string": genericASTnode & {
		type: "string",
		value: string,
	},
	
	//
	// things you get a value from
	//
	
	"identifier": genericASTnode & {
		type: "identifier",
		name: string,
	},
	
	// "call": genericASTnode & {
	// 	// TODO
	// },
	
	//
	// structured things (what are these called?)
	//
	
	// "struct": genericASTnode & {
	// 	// TODO
	// },
	
}

export type ASTnode = ASTtype["bool"] |
ASTtype["number"] |
ASTtype["string"] |
ASTtype["identifier"]
// ASTtype["call"] |
// ASTtype["struct"]

//
// scope
//

type genericScopeObject = {
	originLocation: SourceLocation,
}

export type ScopeObject = genericScopeObject & {
	type: "bool",
	value: boolean,
} | genericScopeObject & {
	type: "number",
	value: number,
} | genericScopeObject & {
	type: "string",
	value: string,
} | genericScopeObject & {
	type: "complexValue",
	// TODO
}