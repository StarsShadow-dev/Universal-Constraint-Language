export type SourceLocation = {
	line: number,
	startColumn: number,
	endColumn: number,
}

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

type genericASTnode = {
	location: SourceLocation,
}

export type ASTtype = {
	//
	// literals
	//
	
	"bool": genericASTnode & {
		value: boolean,
	},
	"number": genericASTnode & {
		value: string,
	},
	"string": genericASTnode & {
		value: string,
	},
	
	//
	// things you get a value from
	//
	
	"identifier": genericASTnode & {
		name: string,
	},
	
	"call": genericASTnode & {
		// TODO
	},
	
	//
	// structured things (what are these called?)
	//
	
	"struct": genericASTnode & {
		// TODO
	},
	
}

export type ASTnode = ASTtype["bool"] | ASTtype["number"] | ASTtype["string"]