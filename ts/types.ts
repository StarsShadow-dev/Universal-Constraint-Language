//
// general
//

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
} | "core"

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

export type ASTnode = 

//
// literals
//

genericASTnode & {
	type: "bool",
	value: boolean,
} | genericASTnode & {
	type: "number",
	value: number,
} | genericASTnode & {
	type: "string",
	value: string,
} |

//
// things you get a value from
//

genericASTnode & {
	type: "identifier",
	name: string,
} | genericASTnode & {
	type: "call",
	left: ASTnode[],
	callArguments: ASTnode[],
} | genericASTnode & {
	type: "builtinCall",
	name: string,
	callArguments: ASTnode[],
} |

//
// structured things (what are these called?)
//

genericASTnode & {
	type: "definition",
	mutable: boolean,
	name: string,
	value: ASTnode[],
} | genericASTnode & {
	type: "assignment",
	left: ASTnode[],
	right: ASTnode[],
} | genericASTnode & {
	type: "function",
	functionArguments: ASTnode[],
	returnType: ASTnode[] | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	type: "struct",
} | genericASTnode & {
	type: "while",
} | genericASTnode & {
	type: "if",
} | genericASTnode & {
	type: "return",
}

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
} | genericScopeObject & {
	type: "alias",
	mutable: boolean,
	name: string,
	value: ScopeObject[] | null,
} | genericScopeObject & {
	type: "function",
	name: string,
	returnType: ScopeObject[] | null,
	AST: ASTnode[],
} | genericScopeObject & {
	type: "struct",
	// TODO
} | genericScopeObject & {
	type: "type",
	name: string,
}